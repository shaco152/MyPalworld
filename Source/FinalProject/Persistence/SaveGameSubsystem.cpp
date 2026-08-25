#include "Persistence/SaveGameSubsystem.h"

#include "Algo/AllOf.h"
#include "AbilitySystem/PlayerAttributeSet.h"
#include "Characters/PalCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/FinalProjectGameInstance.h"
#include "Framework/FinalProjectPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/ItemInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Persistence/WorldPersistenceSubsystem.h"
#include "Storage/PalStorageComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

const FString USaveGameSubsystem::IndexSlotName = TEXT("WorldIndex");

void USaveGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadOrCreateLocalProfile();
	LoadIndex();
}

FString USaveGameSubsystem::MakeLocalProfileSlotName()
{
	FString RequestedProfile;
	FParse::Value(FCommandLine::Get(), TEXT("PlayerProfile="), RequestedProfile);
	RequestedProfile = RequestedProfile.TrimStartAndEnd();
	if (RequestedProfile.IsEmpty())
	{
		RequestedProfile = TEXT("Default");
	}

	FString SafeProfile;
	for (const TCHAR Character : RequestedProfile.Left(32))
	{
		SafeProfile.AppendChar(FChar::IsAlnum(Character) ? Character : TEXT('_'));
	}
	return FString::Printf(TEXT("LocalPlayerProfile_%s"), *SafeProfile);
}

void USaveGameSubsystem::LoadOrCreateLocalProfile()
{
	LocalPlayerProfileSlotName = MakeLocalProfileSlotName();
	LocalPlayerProfile = Cast<ULocalPlayerProfileSave>(
		UGameplayStatics::LoadGameFromSlot(LocalPlayerProfileSlotName, 0));
	if (!LocalPlayerProfile || !LocalPlayerProfile->PlayerProfileId.IsValid())
	{
		LocalPlayerProfile = Cast<ULocalPlayerProfileSave>(
			UGameplayStatics::CreateSaveGameObject(ULocalPlayerProfileSave::StaticClass()));
		if (LocalPlayerProfile)
		{
			LocalPlayerProfile->PlayerProfileId = FGuid::NewGuid();
			if (!UGameplayStatics::SaveGameToSlot(LocalPlayerProfile, LocalPlayerProfileSlotName, 0))
			{
				UE_LOG(LogTemp, Error, TEXT("[诊断] 本地玩家身份写入失败：Slot=%s"), *LocalPlayerProfileSlotName);
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 本地玩家身份：Slot=%s ProfileId=%s"),
		*LocalPlayerProfileSlotName,
		LocalPlayerProfile ? *LocalPlayerProfile->PlayerProfileId.ToString(EGuidFormats::Digits) : TEXT("Invalid"));
}

FGuid USaveGameSubsystem::GetLocalPlayerProfileId() const
{
	return LocalPlayerProfile ? LocalPlayerProfile->PlayerProfileId : FGuid();
}

FString USaveGameSubsystem::MakeBufferSlot(const FGuid& WorldId, const FString& Buffer)
{
	return FString::Printf(TEXT("World_%s_%s"), *WorldId.ToString(EGuidFormats::Digits), *Buffer);
}

FString USaveGameSubsystem::MakePlayerKey(const AFinalProjectPlayerState* PlayerState)
{
	if (PlayerState && PlayerState->GetPlayerPersistentId().IsValid())
	{
		return FString::Printf(TEXT("Profile:%s"),
			*PlayerState->GetPlayerPersistentId().ToString(EGuidFormats::Digits));
	}
	if (PlayerState && PlayerState->GetUniqueId().IsValid())
	{
		if (const TSharedPtr<const FUniqueNetId> UniqueId = PlayerState->GetUniqueId().GetUniqueNetId())
		{
			return UniqueId->ToString();
		}
	}
	return PlayerState ? FString::Printf(TEXT("Local:%s"), *PlayerState->GetPlayerName()) : FString();
}

void USaveGameSubsystem::LoadIndex()
{
	SaveIndexObject = Cast<UWorldSaveIndex>(UGameplayStatics::LoadGameFromSlot(IndexSlotName, 0));
	if (!SaveIndexObject)
	{
		SaveIndexObject = Cast<UWorldSaveIndex>(UGameplayStatics::CreateSaveGameObject(UWorldSaveIndex::StaticClass()));
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 世界存档索引加载：%d 条"), SaveIndexObject ? SaveIndexObject->Entries.Num() : 0);
}

void USaveGameSubsystem::PersistIndex()
{
	if (SaveIndexObject && !UGameplayStatics::SaveGameToSlot(SaveIndexObject, IndexSlotName, 0))
	{
		UE_LOG(LogTemp, Error, TEXT("[诊断] 世界存档索引写入失败"));
	}
}

FGuid USaveGameSubsystem::CreateNewWorld(const FString& DisplayName)
{
	if (bSaveInFlight)
	{
		return FGuid();
	}
	ActiveWorldSave = Cast<UWorldSaveGame>(UGameplayStatics::CreateSaveGameObject(UWorldSaveGame::StaticClass()));
	if (!ActiveWorldSave)
	{
		return FGuid();
	}
	ActiveWorldSave->Metadata.WorldId = FGuid::NewGuid();
	ActiveWorldSave->Metadata.DisplayName = DisplayName.TrimStartAndEnd().IsEmpty()
		? FString::Printf(TEXT("世界 %s"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H-%M")))
		: DisplayName.TrimStartAndEnd();
	ActiveWorldSave->Metadata.SavedAtUtc = FDateTime::UtcNow();
	ActiveWorldSave->Metadata.SaveRevision = 0;
	if (const UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance()))
	{
		ActiveWorldSave->Metadata.MapPath = GameInstance->GameplayMapPath;
	}
	FWorldSaveIndexEntry& Entry = SaveIndexObject->Entries.AddDefaulted_GetRef();
	Entry.Metadata = ActiveWorldSave->Metadata;
	Entry.ActiveBuffer = TEXT("B"); // 首次保存写 A
	PersistIndex();
	ActiveLoadedAtUtc = FDateTime::UtcNow();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 创建新世界档：%s %s"),
		*ActiveWorldSave->Metadata.WorldId.ToString(), *ActiveWorldSave->Metadata.DisplayName);
	return ActiveWorldSave->Metadata.WorldId;
}

UWorldSaveGame* USaveGameSubsystem::LoadBestBuffer(const FGuid& WorldId, FString& OutBuffer) const
{
	const FString SlotA = MakeBufferSlot(WorldId, TEXT("A"));
	const FString SlotB = MakeBufferSlot(WorldId, TEXT("B"));
	UWorldSaveGame* SaveA = UGameplayStatics::DoesSaveGameExist(SlotA, 0)
		? Cast<UWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotA, 0)) : nullptr;
	UWorldSaveGame* SaveB = UGameplayStatics::DoesSaveGameExist(SlotB, 0)
		? Cast<UWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotB, 0)) : nullptr;
	const bool bAValid = SaveA && SaveA->IsStructurallyValid() && SaveA->Metadata.WorldId == WorldId;
	const bool bBValid = SaveB && SaveB->IsStructurallyValid() && SaveB->Metadata.WorldId == WorldId;
	if (!bAValid && !bBValid)
	{
		return nullptr;
	}
	if (bAValid && (!bBValid || SaveA->Metadata.SaveRevision >= SaveB->Metadata.SaveRevision))
	{
		OutBuffer = TEXT("A");
		return SaveA;
	}
	OutBuffer = TEXT("B");
	return SaveB;
}

bool USaveGameSubsystem::LoadWorld(const FGuid& WorldId)
{
	if (!WorldId.IsValid() || bSaveInFlight)
	{
		return false;
	}
	FString Buffer;
	UWorldSaveGame* Loaded = LoadBestBuffer(WorldId, Buffer);
	if (!Loaded)
	{
		UE_LOG(LogTemp, Error, TEXT("[诊断] 世界档损坏或不存在：%s"), *WorldId.ToString());
		return false;
	}
	ActiveWorldSave = Loaded;
	ActiveLoadedAtUtc = FDateTime::UtcNow();
	if (FWorldSaveIndexEntry* Entry = FindIndexEntry(WorldId))
	{
		Entry->Metadata = Loaded->Metadata;
		Entry->ActiveBuffer = Buffer;
		PersistIndex();
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 读取世界档：%s Buffer=%s Revision=%lld"),
		*WorldId.ToString(), *Buffer, Loaded->Metadata.SaveRevision);
	return true;
}

void USaveGameSubsystem::TravelToActiveWorld(bool bListenServer)
{
	if (!ActiveWorldSave || ActiveWorldSave->Metadata.MapPath.IsEmpty())
	{
		OnSaveFinished.Broadcast(false, TEXT("没有可进入的活动世界档"));
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FString Options = bListenServer ? TEXT("listen") : FString();
	UGameplayStatics::OpenLevel(World, FName(*ActiveWorldSave->Metadata.MapPath), true, Options);
}

bool USaveGameSubsystem::SaveActiveWorld(UObject* WorldContextObject)
{
	FString FailureReason;
	if (!CanSaveActiveWorld(WorldContextObject, FailureReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 世界存档请求被拒绝：%s"), *FailureReason);
		return false;
	}
	UWorld* World = WorldContextObject->GetWorld();
	PendingSaveObject = Cast<UWorldSaveGame>(UGameplayStatics::CreateSaveGameObject(UWorldSaveGame::StaticClass()));
	if (!PendingSaveObject)
	{
		return false;
	}
	PendingSaveObject->SchemaVersion = UWorldSaveGame::CurrentSchemaVersion;
	PendingSaveObject->Metadata = ActiveWorldSave->Metadata;
	PendingSaveObject->Metadata.SaveRevision++;
	PendingSaveObject->Metadata.SavedAtUtc = FDateTime::UtcNow();
	PendingSaveObject->Metadata.TotalPlaySeconds += FMath::Max<int64>(0,
		static_cast<int64>((FDateTime::UtcNow() - ActiveLoadedAtUtc).GetTotalSeconds()));
	if (!CaptureWorld(World, PendingSaveObject))
	{
		PendingSaveObject = nullptr;
		OnSaveFinished.Broadcast(false, TEXT("存档校验失败，上一缓冲保持不变"));
		return false;
	}

	const FWorldSaveIndexEntry* Entry = FindIndexEntry(PendingSaveObject->Metadata.WorldId);
	PendingBuffer = Entry && Entry->ActiveBuffer == TEXT("A") ? TEXT("B") : TEXT("A");
	const FString SlotName = MakeBufferSlot(PendingSaveObject->Metadata.WorldId, PendingBuffer);
	bSaveInFlight = true;
	FAsyncSaveGameToSlotDelegate Delegate;
	Delegate.BindUObject(this, &USaveGameSubsystem::HandleAsyncSaveFinished);
	UGameplayStatics::AsyncSaveGameToSlot(PendingSaveObject, SlotName, 0, Delegate);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 异步保存开始：Slot=%s Revision=%lld"),
		*SlotName, PendingSaveObject->Metadata.SaveRevision);
	return true;
}

bool USaveGameSubsystem::CanSaveActiveWorld(const UObject* WorldContextObject, FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	if (bSaveInFlight)
	{
		OutFailureReason = TEXT("已有存档正在写入，请稍后再试");
		return false;
	}
	if (!ActiveWorldSave)
	{
		OutFailureReason = TEXT("当前没有活动世界档");
		return false;
	}
	if (!WorldContextObject)
	{
		OutFailureReason = TEXT("存档世界上下文无效");
		return false;
	}
	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		OutFailureReason = TEXT("当前世界无效");
		return false;
	}
	if (World->GetNetMode() == NM_Client)
	{
		OutFailureReason = TEXT("客户端不能直接写世界档，必须由主机保存");
		return false;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (const APlayerCharacter* Character = PlayerState ? Cast<APlayerCharacter>(PlayerState->GetPawn()) : nullptr;
				Character && Character->IsInTurnBattle())
			{
				OutFailureReason = TEXT("回合制战斗进行中，只能保存战斗外快照");
				return false;
			}
		}
	}
	return ValidateWorldForSave(World, OutFailureReason);
}

bool USaveGameSubsystem::ValidateWorldForSave(const UWorld* World, FString& OutFailureReason) const
{
	if (!World)
	{
		OutFailureReason = TEXT("当前世界无效");
		return false;
	}
	const UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		OutFailureReason = TEXT("FinalProjectGameInstance 不可用，无法解析帕鲁定义");
		return false;
	}

	auto ValidatePalArray = [GameInstance, &OutFailureReason](const FReplicatedStoredPalList& PalList,
		const FString& PlayerLabel, const TCHAR* ContainerLabel) -> bool
	{
		for (int32 Index = 0; Index < PalList.Num(); ++Index)
		{
			const FStoredPalInfo& Info = PalList[Index];
			if (!Info.IsValid())
			{
				continue;
			}
			FName DefinitionId = Info.PalDefinitionId;
			if (DefinitionId.IsNone())
			{
				GameInstance->ResolvePalDefinitionId(Info.PalClass, DefinitionId);
			}
			if (DefinitionId.IsNone())
			{
				OutFailureReason = FString::Printf(TEXT("%s的%s槽%d：帕鲁类 %s 未登记到 DT_PalDefinitions"),
					*PlayerLabel, ContainerLabel, Index + 1, *GetPathNameSafe(Info.PalClass.Get()));
				return false;
			}
			if (!Info.PalInstanceId.IsValid())
			{
				OutFailureReason = FString::Printf(TEXT("%s的%s槽%d：%s 缺少服务器 PalInstanceId"),
					*PlayerLabel, ContainerLabel, Index + 1, *DefinitionId.ToString());
				return false;
			}
		}
		return true;
	};

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		for (const APlayerState* BaseState : GameState->PlayerArray)
		{
			const AFinalProjectPlayerState* PlayerState = Cast<AFinalProjectPlayerState>(BaseState);
			if (!PlayerState)
			{
				continue;
			}
			const UPalStorageComponent* Storage = PlayerState->GetPalStorage();
			if (!Storage)
			{
				continue;
			}
			const FString PlayerLabel = PlayerState->GetPlayerName().IsEmpty()
				? PlayerState->GetName() : PlayerState->GetPlayerName();
			if (!ValidatePalArray(Storage->PartyPals, PlayerLabel, TEXT("背包")) ||
				!ValidatePalArray(Storage->BoxPals, PlayerLabel, TEXT("仓库")))
			{
				return false;
			}
		}
	}
	return true;
}

bool USaveGameSubsystem::CaptureWorld(UWorld* World, UWorldSaveGame* TargetSave)
{
	bool bValid = true;
	// 保存当前在线玩家时保留旧档中的离线玩家。否则主机独自加载一个联机世界后，
	// 任意一次自动/手动保存都会永久删除未在线客户端的角色、帕鲁与材料记录。
	if (ActiveWorldSave && ActiveWorldSave->Metadata.WorldId == TargetSave->Metadata.WorldId)
	{
		TargetSave->Players = ActiveWorldSave->Players;
	}
	else
	{
		TargetSave->Players.Reset();
	}

	int32 CapturedOnlinePlayers = 0;
	if (AGameStateBase* GameState = World->GetGameState())
	{
		for (APlayerState* BaseState : GameState->PlayerArray)
		{
			AFinalProjectPlayerState* PlayerState = Cast<AFinalProjectPlayerState>(BaseState);
			if (!PlayerState)
			{
				continue;
			}
			PlayerState->EnsurePersistentId();
			const FGuid PlayerId = PlayerState->GetPlayerPersistentId();
			const FString PlayerKey = MakePlayerKey(PlayerState);
			const int32 ExistingIndex = TargetSave->Players.IndexOfByPredicate(
				[&PlayerId, &PlayerKey](const FPlayerSaveRecord& Existing)
				{
					return (PlayerId.IsValid() && Existing.PlayerPersistentId == PlayerId) ||
						(!PlayerKey.IsEmpty() && Existing.PlayerKey == PlayerKey);
				});

			FPlayerSaveRecord NewRecord;
			FPlayerSaveRecord& Record = ExistingIndex == INDEX_NONE
				? TargetSave->Players.Add_GetRef(MoveTemp(NewRecord))
				: (TargetSave->Players[ExistingIndex] = MoveTemp(NewRecord));
			Record.PlayerPersistentId = PlayerState->GetPlayerPersistentId();
			Record.PlayerKey = PlayerKey;
			if (APawn* Pawn = PlayerState->GetPawn())
			{
				Record.PawnTransform = Pawn->GetActorTransform();
				if (const APlayerCharacter* Character = Cast<APlayerCharacter>(Pawn))
				{
					if (const UPlayerAttributeSet* Attributes = Character->GetAttributeSet())
					{
						Record.Health = Attributes->GetHealth();
						Record.MaxHealth = Attributes->GetMaxHealth();
					}
				}
			}
			if (UPalStorageComponent* Storage = PlayerState->GetPalStorage())
			{
				Storage->RecallSummonedPal();
				Record.ActivePartyIndex = Storage->ActivePartyIndex;
				for (const FStoredPalInfo& Info : Storage->PartyPals)
				{
					FPalSaveRecord& PalRecord = Record.PartyPals.AddDefaulted_GetRef();
					bValid &= ConvertStoredPalToSave(Info, PalRecord) || !Info.IsValid();
				}
				for (const FStoredPalInfo& Info : Storage->BoxPals)
				{
					FPalSaveRecord& PalRecord = Record.BoxPals.AddDefaulted_GetRef();
					bValid &= ConvertStoredPalToSave(Info, PalRecord) || !Info.IsValid();
				}
			}
			if (const UItemInventoryComponent* Inventory = PlayerState->GetItemInventory())
			{
				Record.ItemStacks = Inventory->GetStacks();
			}
			++CapturedOnlinePlayers;
		}
	}

	// 首次保存或旧档迁移时固定世界所有者。PlayerArray 的本地 PlayerController
	// 就是 Standalone/Listen 主机；后续重载同一世界时用该 ID 恢复主机记录。
	if (!TargetSave->Metadata.OwnerPlayerId.IsValid())
	{
		if (APlayerController* LocalController = World->GetFirstPlayerController();
			LocalController && LocalController->IsLocalController())
		{
			if (const AFinalProjectPlayerState* OwnerState =
				LocalController->GetPlayerState<AFinalProjectPlayerState>())
			{
				TargetSave->Metadata.OwnerPlayerId = OwnerState->GetPlayerPersistentId();
			}
		}
	}
	if (UWorldPersistenceSubsystem* Persistence = World->GetSubsystem<UWorldPersistenceSubsystem>())
	{
		Persistence->CaptureBuildings(TargetSave->Buildings);
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 世界玩家快照：Online=%d TotalRecords=%d PreservedOffline=%d Owner=%s"),
		CapturedOnlinePlayers, TargetSave->Players.Num(),
		FMath::Max(0, TargetSave->Players.Num() - CapturedOnlinePlayers),
		*TargetSave->Metadata.OwnerPlayerId.ToString(EGuidFormats::Digits));
	return bValid;
}

bool USaveGameSubsystem::ConvertStoredPalToSave(const FStoredPalInfo& Info, FPalSaveRecord& OutRecord) const
{
	if (!Info.IsValid())
	{
		return false;
	}
	FName DefinitionId = Info.PalDefinitionId;
	if (DefinitionId.IsNone())
	{
		if (const UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance()))
		{
			GameInstance->ResolvePalDefinitionId(Info.PalClass, DefinitionId);
		}
	}
	if (DefinitionId.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("[诊断] 拒绝保存帕鲁条目：类 %s 无 PalDefinitionId 映射"), *GetNameSafe(Info.PalClass.Get()));
		return false;
	}
	if (!Info.PalInstanceId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[诊断] 拒绝保存帕鲁条目：DefinitionId=%s 缺少服务器 PalInstanceId"), *DefinitionId.ToString());
		return false;
	}
	OutRecord.PalInstanceId = Info.PalInstanceId;
	OutRecord.PalDefinitionId = DefinitionId;
	OutRecord.ClassFallback = Info.PalClass;
	OutRecord.Level = Info.Level;
	OutRecord.Health = Info.Health;
	OutRecord.MaxHealth = Info.MaxHealth;
	OutRecord.MP = Info.MP;
	OutRecord.MaxMP = Info.MaxMP;
	OutRecord.SkillRowNames = Info.SkillRowNames;
	return true;
}

FStoredPalInfo USaveGameSubsystem::ConvertSavePalToStored(const FPalSaveRecord& Record) const
{
	FStoredPalInfo Info;
	if (Record.PalDefinitionId.IsNone())
	{
		return Info;
	}
	Info.PalDefinitionId = Record.PalDefinitionId;
	Info.PalInstanceId = Record.PalInstanceId;
	if (const UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance()))
	{
		Info.PalClass = GameInstance->ResolvePalClass(Record.PalDefinitionId);
	}
	if (!Info.PalClass && !Record.ClassFallback.IsNull())
	{
		Info.PalClass = Record.ClassFallback.LoadSynchronous();
	}
	if (!Info.PalClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[诊断] 帕鲁恢复失败：DefinitionId=%s 无可用类"), *Record.PalDefinitionId.ToString());
		return FStoredPalInfo();
	}
	Info.Icon = Info.PalClass->GetDefaultObject<APalCharacter>()->PortraitIcon;
	Info.Level = Record.Level;
	Info.Health = Record.Health;
	Info.MaxHealth = Record.MaxHealth;
	Info.MP = Record.MP;
	Info.MaxMP = Record.MaxMP;
	Info.SkillRowNames = Record.SkillRowNames;
	return Info;
}

void USaveGameSubsystem::HandleAsyncSaveFinished(const FString& SlotName, const int32 UserIndex, bool bSuccess)
{
	bSaveInFlight = false;
	if (bSuccess && PendingSaveObject)
	{
		ActiveWorldSave = PendingSaveObject;
		ActiveLoadedAtUtc = FDateTime::UtcNow();
		if (FWorldSaveIndexEntry* Entry = FindIndexEntry(ActiveWorldSave->Metadata.WorldId))
		{
			Entry->Metadata = ActiveWorldSave->Metadata;
			Entry->ActiveBuffer = PendingBuffer;
		}
		PersistIndex();
	}
	const FString Message = bSuccess ? TEXT("世界保存完成") : TEXT("世界保存失败，上一缓冲仍可读取");
	if (bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] %s：%s"), *Message, *SlotName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[诊断] %s：%s"), *Message, *SlotName);
	}
	PendingSaveObject = nullptr;
	PendingBuffer.Reset();
	OnSaveFinished.Broadcast(bSuccess, Message);
}

TArray<FWorldSaveMetadata> USaveGameSubsystem::GetSaveList() const
{
	TArray<FWorldSaveMetadata> Result;
	if (SaveIndexObject)
	{
		for (const FWorldSaveIndexEntry& Entry : SaveIndexObject->Entries)
		{
			Result.Add(Entry.Metadata);
		}
	}
	Result.Sort([](const FWorldSaveMetadata& A, const FWorldSaveMetadata& B) { return A.SavedAtUtc > B.SavedAtUtc; });
	return Result;
}

TArray<FWorldSaveMetadata> USaveGameSubsystem::RefreshSaveListFromDisk()
{
	if (bSaveInFlight)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 存档正在写入，跳过磁盘列表刷新"));
		return GetSaveList();
	}

	LoadIndex();
	if (!SaveIndexObject)
	{
		return {};
	}

	bool bIndexChanged = false;
	for (int32 Index = SaveIndexObject->Entries.Num() - 1; Index >= 0; --Index)
	{
		FWorldSaveIndexEntry& Entry = SaveIndexObject->Entries[Index];
		FString BestBuffer;
		UWorldSaveGame* BestSave = Entry.Metadata.WorldId.IsValid()
			? LoadBestBuffer(Entry.Metadata.WorldId, BestBuffer) : nullptr;
		if (!BestSave)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 清理悬空世界存档索引：%s"),
				*Entry.Metadata.WorldId.ToString());
			SaveIndexObject->Entries.RemoveAt(Index);
			bIndexChanged = true;
			continue;
		}

		const FWorldSaveMetadata& DiskMetadata = BestSave->Metadata;
		const bool bMetadataChanged =
			Entry.Metadata.WorldId != DiskMetadata.WorldId ||
			Entry.Metadata.DisplayName != DiskMetadata.DisplayName ||
			Entry.Metadata.SavedAtUtc != DiskMetadata.SavedAtUtc ||
			Entry.Metadata.TotalPlaySeconds != DiskMetadata.TotalPlaySeconds ||
			Entry.Metadata.SaveRevision != DiskMetadata.SaveRevision ||
			Entry.Metadata.MapPath != DiskMetadata.MapPath;
		if (bMetadataChanged || Entry.ActiveBuffer != BestBuffer)
		{
			Entry.Metadata = DiskMetadata;
			Entry.ActiveBuffer = BestBuffer;
			bIndexChanged = true;
		}
	}

	if (bIndexChanged)
	{
		PersistIndex();
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 世界存档列表磁盘刷新完成：%d 条"), SaveIndexObject->Entries.Num());
	return GetSaveList();
}

const FPlayerSaveRecord* USaveGameSubsystem::FindPlayerRecord(const AFinalProjectPlayerState* PlayerState) const
{
	if (!ActiveWorldSave || !PlayerState)
	{
		return nullptr;
	}
	const FGuid PersistentId = PlayerState->GetPlayerPersistentId();
	const FString PlayerKey = MakePlayerKey(PlayerState);
	for (const FPlayerSaveRecord& Record : ActiveWorldSave->Players)
	{
		if ((PersistentId.IsValid() && Record.PlayerPersistentId == PersistentId) ||
			(!PlayerKey.IsEmpty() && Record.PlayerKey == PlayerKey))
		{
			return &Record;
		}
	}

	// Schema 1 + NULL OSS 的客户端身份带进程随机后缀。升级后的首个稳定 Profile
	// 可认领“同一机器且唯一未被占用”的旧客户端记录，并在 RestorePlayerState 中迁移。
	if (PlayerState->GetWorld() && PlayerState->GetWorld()->GetNetMode() == NM_ListenServer && PersistentId.IsValid())
	{
		FString MachinePrefix = PlayerState->GetPlayerName();
		int32 SuffixSeparator = INDEX_NONE;
		if (MachinePrefix.FindLastChar(TEXT('-'), SuffixSeparator))
		{
			const FString Suffix = MachinePrefix.Mid(SuffixSeparator + 1);
			const bool bLooksLikeNullRandomSuffix = Suffix.Len() == 32 &&
				Algo::AllOf(Suffix, [](TCHAR Character) { return FChar::IsHexDigit(Character); });
			if (bLooksLikeNullRandomSuffix)
			{
				MachinePrefix.LeftInline(SuffixSeparator, false);
			}
		}

		TSet<FGuid> ClaimedPlayerIds;
		if (const AGameStateBase* GameState = PlayerState->GetWorld()->GetGameState())
		{
			for (const APlayerState* ExistingBaseState : GameState->PlayerArray)
			{
				const AFinalProjectPlayerState* ExistingState = Cast<AFinalProjectPlayerState>(ExistingBaseState);
				if (ExistingState && ExistingState != PlayerState && ExistingState->GetPlayerPersistentId().IsValid())
				{
					ClaimedPlayerIds.Add(ExistingState->GetPlayerPersistentId());
				}
			}
		}

		const FPlayerSaveRecord* LegacyCandidate = nullptr;
		for (const FPlayerSaveRecord& Record : ActiveWorldSave->Players)
		{
			const bool bLegacyKey = !Record.PlayerKey.StartsWith(TEXT("Profile:"));
			const bool bIsOwner = ActiveWorldSave->Metadata.OwnerPlayerId.IsValid() &&
				Record.PlayerPersistentId == ActiveWorldSave->Metadata.OwnerPlayerId;
			const bool bSameMachine = !MachinePrefix.IsEmpty() && Record.PlayerKey.Contains(MachinePrefix);
			if (!bLegacyKey || bIsOwner || !bSameMachine || ClaimedPlayerIds.Contains(Record.PlayerPersistentId))
			{
				continue;
			}
			if (LegacyCandidate)
			{
				LegacyCandidate = nullptr; // 多个候选时拒绝猜测，避免串档。
				break;
			}
			LegacyCandidate = &Record;
		}
		if (LegacyCandidate)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 客户端旧身份迁移候选：LegacyKey=%s NewProfile=%s"),
				*LegacyCandidate->PlayerKey, *PersistentId.ToString(EGuidFormats::Digits));
			return LegacyCandidate;
		}
	}
	// 仅 Standalone 主机允许旧档迁移回退。Schema 1 的联机档没有 OwnerPlayerId，
	// 但保存顺序始终是主机先进入 PlayerArray，因此首条记录就是世界所有者。
	// Listen Server 的远程加入者绝不走此回退，避免复制主机数据。
	if (PlayerState->GetWorld() && PlayerState->GetWorld()->GetNetMode() == NM_Standalone)
	{
		if (ActiveWorldSave->Metadata.OwnerPlayerId.IsValid())
		{
			if (const FPlayerSaveRecord* OwnerRecord = ActiveWorldSave->Players.FindByPredicate(
				[this](const FPlayerSaveRecord& Record)
				{
					return Record.PlayerPersistentId == ActiveWorldSave->Metadata.OwnerPlayerId;
				}))
			{
				return OwnerRecord;
			}
		}
		if (!ActiveWorldSave->Players.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 旧世界档缺少 OwnerPlayerId，Standalone 按首条玩家记录迁移"));
			return &ActiveWorldSave->Players[0];
		}
	}
	return nullptr;
}

bool USaveGameSubsystem::RestorePlayerState(AFinalProjectPlayerState* PlayerState)
{
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		return false;
	}
	const FPlayerSaveRecord* Record = FindPlayerRecord(PlayerState);
	if (!Record)
	{
		PlayerState->EnsurePersistentId();
		return false;
	}
	const FGuid RequestedProfileId = PlayerState->GetPlayerPersistentId();
	const bool bMigrateLegacyClient = PlayerState->GetWorld() &&
		PlayerState->GetWorld()->GetNetMode() == NM_ListenServer && RequestedProfileId.IsValid() &&
		Record->PlayerPersistentId != RequestedProfileId && !Record->PlayerKey.StartsWith(TEXT("Profile:"));
	if (bMigrateLegacyClient)
	{
		const int32 RecordIndex = ActiveWorldSave->Players.IndexOfByPredicate(
			[Record](const FPlayerSaveRecord& Candidate) { return &Candidate == Record; });
		if (ActiveWorldSave->Players.IsValidIndex(RecordIndex))
		{
			FPlayerSaveRecord& MutableRecord = ActiveWorldSave->Players[RecordIndex];
			MutableRecord.PlayerPersistentId = RequestedProfileId;
			MutableRecord.PlayerKey = MakePlayerKey(PlayerState);
			Record = &MutableRecord;
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 客户端旧身份已迁移到稳定 ProfileId=%s"),
				*RequestedProfileId.ToString(EGuidFormats::Digits));
		}
	}
	else
	{
		PlayerState->SetPlayerPersistentIdForRestore(Record->PlayerPersistentId);
	}
	TArray<FStoredPalInfo> Party;
	TArray<FStoredPalInfo> Box;
	for (const FPalSaveRecord& PalRecord : Record->PartyPals)
	{
		Party.Add(ConvertSavePalToStored(PalRecord));
	}
	for (const FPalSaveRecord& PalRecord : Record->BoxPals)
	{
		Box.Add(ConvertSavePalToStored(PalRecord));
	}
	PlayerState->GetPalStorage()->RestoreSnapshot(Party, Box, Record->ActivePartyIndex);
	PlayerState->GetItemInventory()->RestoreStacks(Record->ItemStacks);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PlayerState 存档恢复完成：%s"), *PlayerState->GetPlayerPersistentId().ToString());
	return true;
}

bool USaveGameSubsystem::RestorePawn(AFinalProjectPlayerState* PlayerState, APawn* Pawn) const
{
	const FPlayerSaveRecord* Record = FindPlayerRecord(PlayerState);
	if (!Record || !Pawn)
	{
		return false;
	}
	Pawn->SetActorTransform(Record->PawnTransform, false, nullptr, ETeleportType::ResetPhysics);
	if (APlayerCharacter* Character = Cast<APlayerCharacter>(Pawn))
	{
		if (UPlayerAttributeSet* Attributes = Character->GetAttributeSet())
		{
			Attributes->InitMaxHealth(FMath::Max(1.f, Record->MaxHealth));
			Attributes->InitHealth(FMath::Clamp(Record->Health, 0.f, Attributes->GetMaxHealth()));
			Attributes->OnHealthChanged.Broadcast(Attributes->GetHealth(), Attributes->GetMaxHealth());
		}
	}
	return true;
}

FWorldSaveIndexEntry* USaveGameSubsystem::FindIndexEntry(const FGuid& WorldId)
{
	return SaveIndexObject ? SaveIndexObject->Entries.FindByPredicate([&WorldId](const FWorldSaveIndexEntry& Entry)
	{
		return Entry.Metadata.WorldId == WorldId;
	}) : nullptr;
}

const FWorldSaveIndexEntry* USaveGameSubsystem::FindIndexEntry(const FGuid& WorldId) const
{
	return SaveIndexObject ? SaveIndexObject->Entries.FindByPredicate([&WorldId](const FWorldSaveIndexEntry& Entry)
	{
		return Entry.Metadata.WorldId == WorldId;
	}) : nullptr;
}

void USaveGameSubsystem::ClearActiveWorld()
{
	if (!bSaveInFlight)
	{
		ActiveWorldSave = nullptr;
		ActiveLoadedAtUtc = FDateTime();
	}
}
