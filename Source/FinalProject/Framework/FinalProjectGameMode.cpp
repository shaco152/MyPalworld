#include "Framework/FinalProjectGameMode.h"

#include "Engine/World.h"
#include "Framework/FinalProjectGameState.h"
#include "Framework/FinalProjectPlayerState.h"
#include "Characters/PlayerCharacter.h"
#include "Combat/TurnBattleComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Persistence/SaveGameSubsystem.h"
#include "Persistence/WorldPersistenceSubsystem.h"
#include "Online/PalSessionSubsystem.h"
#include "Storage/PalStorageComponent.h"
#include "TimerManager.h"

AFinalProjectGameMode::AFinalProjectGameMode()
{
	PlayerStateClass = AFinalProjectPlayerState::StaticClass();
	GameStateClass = AFinalProjectGameState::StaticClass();
}

void AFinalProjectGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (UPalSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UPalSessionSubsystem>())
	{
		Sessions->HandleGameplayTravelSucceeded();
	}
	AFinalProjectGameState* ProjectGameState = GetGameState<AFinalProjectGameState>();
	if (ProjectGameState)
	{
		ProjectGameState->SetLoadState(EWorldLoadState::Restoring);
	}
	bWorldRestoreComplete = true;
	if (USaveGameSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveGameSubsystem>())
	{
		if (const UWorldSaveGame* ActiveSave = Saves->GetActiveSave())
		{
			if (UWorldPersistenceSubsystem* Persistence = GetWorld()->GetSubsystem<UWorldPersistenceSubsystem>())
			{
				bWorldRestoreComplete = Persistence->RestoreBuildings(ActiveSave->Buildings, BuildingCatalog);
			}
		}
	}
	if (!bWorldRestoreComplete)
	{
		if (AFinalProjectGameState* FailedState = GetGameState<AFinalProjectGameState>())
		{
			FailedState->SetLoadState(EWorldLoadState::Failed);
		}
		return;
	}
	TryMarkWorldReady();
	// 某些已存在的 BP CDO 在新增 C++ 属性后可能保留 0；自动存档是核心能力，不允许被无效值静默关闭。
	EffectiveAutoSaveInterval = FMath::IsFinite(AutoSaveInterval) && AutoSaveInterval >= 30.f
		? AutoSaveInterval : 120.f;
	if (!FMath::IsNearlyEqual(EffectiveAutoSaveInterval, AutoSaveInterval))
	{
		UE_LOG(LogTemp, Error, TEXT("[诊断] AutoSaveInterval=%.2f 无效，已回退为 %.2f 秒"),
			AutoSaveInterval, EffectiveAutoSaveInterval);
	}
	ScheduleAutoSave(EffectiveAutoSaveInterval);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] Gameplay 世界恢复阶段完成，等待玩家 Possess"));
}

void AFinalProjectGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 与 ScheduleAutoSave 使用同一个跨 World TimerManager；先清理再销毁 GameMode，避免旧世界回调残留。
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(AutoSaveTimer);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(AutoSaveTimer);
	}
	Super::EndPlay(EndPlayReason);
}

FString AFinalProjectGameMode::InitNewPlayer(APlayerController* NewPlayerController,
	const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!ErrorMessage.IsEmpty() || !NewPlayerController)
	{
		return ErrorMessage;
	}

	AFinalProjectPlayerState* PlayerState =
		NewPlayerController->GetPlayerState<AFinalProjectPlayerState>();
	const FString ProfileOption = UGameplayStatics::ParseOption(Options, TEXT("PlayerProfileId"));
	FGuid RequestedProfileId;
	if (PlayerState && FGuid::Parse(ProfileOption, RequestedProfileId) && RequestedProfileId.IsValid())
	{
		bool bDuplicateProfile = false;
		if (const AGameStateBase* CurrentGameState = GetGameState<AGameStateBase>())
		{
			for (const APlayerState* ExistingState : CurrentGameState->PlayerArray)
			{
				const AFinalProjectPlayerState* ExistingProjectState = Cast<AFinalProjectPlayerState>(ExistingState);
				if (ExistingProjectState && ExistingProjectState != PlayerState &&
					ExistingProjectState->GetPlayerPersistentId() == RequestedProfileId)
				{
					bDuplicateProfile = true;
					break;
				}
			}
		}

		if (!bDuplicateProfile)
		{
			PlayerState->SetPlayerPersistentIdForRestore(RequestedProfileId);
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 客户端稳定身份接入：Player=%s ProfileId=%s"),
				*PlayerState->GetPlayerName(), *RequestedProfileId.ToString(EGuidFormats::Digits));
		}
		else
		{
			// 同机双实例若共用默认 Profile 槽会命中此保护。仍允许本次加入，但使用临时 ID；
			// 要测试跨重启恢复，请分别传 -PlayerProfile=Host / Client。
			PlayerState->EnsurePersistentId();
			UE_LOG(LogTemp, Error, TEXT("[诊断] 拒绝重复 PlayerProfileId=%s，已为本次连接生成临时身份；请为同机实例配置不同 -PlayerProfile"),
				*RequestedProfileId.ToString(EGuidFormats::Digits));
		}
	}
	return ErrorMessage;
}

void AFinalProjectGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	AFinalProjectPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AFinalProjectPlayerState>() : nullptr;
	if (PlayerState && !RestoredPlayerStates.Contains(PlayerState))
	{
		if (USaveGameSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveGameSubsystem>())
		{
			// 本地主机不经过 ClientTravel 的 URL Options：优先使用世界所有者 ID，
			// 新世界则使用本机 Profile ID，确保从 Standalone 切换 Listen 后身份不变。
			if (NewPlayer->IsLocalController())
			{
				const UWorldSaveGame* ActiveSave = Saves->GetActiveSave();
				const FGuid HostId = ActiveSave && ActiveSave->Metadata.OwnerPlayerId.IsValid()
					? ActiveSave->Metadata.OwnerPlayerId : Saves->GetLocalPlayerProfileId();
				PlayerState->SetPlayerPersistentIdForRestore(HostId);
			}
			Saves->RestorePlayerState(PlayerState);
		}
		RestoredPlayerStates.Add(PlayerState);
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void AFinalProjectGameMode::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	Super::FinishRestartPlayer(NewPlayer, StartRotation);
	if (NewPlayer && !RestoredPawns.Contains(NewPlayer))
	{
		if (AFinalProjectPlayerState* PlayerState = NewPlayer->GetPlayerState<AFinalProjectPlayerState>())
		{
			if (USaveGameSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveGameSubsystem>())
			{
				Saves->RestorePawn(PlayerState, NewPlayer->GetPawn());
			}
		}
		RestoredPawns.Add(NewPlayer);
	}
	TryMarkWorldReady();
	if (USaveGameSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveGameSubsystem>();
		Saves && Saves->GetActiveSave() && Saves->GetActiveSave()->Metadata.SaveRevision == 0)
	{
		Saves->SaveActiveWorld(this);
	}
}

void AFinalProjectGameMode::Logout(AController* Exiting)
{
	AFinalProjectPlayerState* ProjectPlayerState = Exiting
		? Exiting->GetPlayerState<AFinalProjectPlayerState>() : nullptr;
	const FGuid PlayerId = ProjectPlayerState ? ProjectPlayerState->GetPlayerPersistentId() : FGuid();
	APlayerCharacter* PlayerPawn = Exiting ? Cast<APlayerCharacter>(Exiting->GetPawn()) : nullptr;
	if (PlayerPawn)
	{
		if (UTurnBattleComponent* Battle = PlayerPawn->GetTurnBattleComponent())
		{
			Battle->AbortBattleForDisconnect();
		}
	}
	// PlayerState 是召唤数据权威宿主；必须在 Super::Logout 销毁 PlayerState/Pawn 之前回写并销毁实体。
	if (UPalStorageComponent* Storage = ProjectPlayerState ? ProjectPlayerState->GetPalStorage() : nullptr;
		Storage && Storage->HasSummonedPal())
	{
		Storage->RecallSummonedPal();
	}
	RestoredPlayerStates.Remove(ProjectPlayerState);
	RestoredPawns.Remove(Exiting);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 玩家离线权威清理完成：Controller=%s ProfileId=%s"),
		*GetNameSafe(Exiting), *PlayerId.ToString(EGuidFormats::Digits));
	Super::Logout(Exiting);
}

void AFinalProjectGameMode::AutoSave()
{
	bool bAccepted = false;
	int64 Revision = INDEX_NONE;
	if (USaveGameSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveGameSubsystem>())
	{
		Revision = Saves->GetActiveSave() ? Saves->GetActiveSave()->Metadata.SaveRevision : INDEX_NONE;
		bAccepted = Saves->SaveActiveWorld(this);
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 自动存档定时触发：Accepted=%d CurrentRevision=%lld NextDelay=%.1f"),
		bAccepted, Revision, bAccepted ? EffectiveAutoSaveInterval : FMath::Max(5.f, AutoSaveRetryDelay));
	ScheduleAutoSave(bAccepted ? EffectiveAutoSaveInterval : FMath::Max(5.f, AutoSaveRetryDelay));
}

void AFinalProjectGameMode::ScheduleAutoSave(float DelaySeconds)
{
	const float SafeDelay = FMath::Max(5.f, DelaySeconds);
	UGameInstance* GameInstance = GetGameInstance();
	FTimerManager& TimerManager = GameInstance ? GameInstance->GetTimerManager() : GetWorldTimerManager();
	// 使用 GameInstance TimerManager：不受当前 World 暂停/时间膨胀影响，EndPlay 会显式清理。
	TimerManager.SetTimer(AutoSaveTimer, this, &AFinalProjectGameMode::AutoSave, SafeDelay, false);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 自动存档已调度：Manager=%s Configured=%.1f EffectiveDelay=%.1f Active=%d Remaining=%.1f"),
		GameInstance ? TEXT("GameInstance") : TEXT("WorldFallback"), AutoSaveInterval, SafeDelay,
		TimerManager.IsTimerActive(AutoSaveTimer), TimerManager.GetTimerRemaining(AutoSaveTimer));
}

void AFinalProjectGameMode::TryMarkWorldReady()
{
	if (!bWorldRestoreComplete || RestoredPawns.IsEmpty())
	{
		return;
	}
	if (AFinalProjectGameState* ProjectGameState = GetGameState<AFinalProjectGameState>())
	{
		ProjectGameState->SetLoadState(EWorldLoadState::Ready);
	}
}
