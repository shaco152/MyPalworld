#include "TurnBattleComponent.h"
#include "AbilitySystem/CaptureChanceExecCalc.h"
#include "AbilitySystem/CaptureTags.h"
#include "AbilitySystem/PalAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraActor.h"
#include "Characters/PalCharacter.h"
#include "Characters/PalPlayerController.h"
#include "Characters/PlayerCharacter.h"
#include "Combat/PalAutoBattleComponent.h"
#include "Combat/PalBattleEnemyManager.h"
#include "Combat/PalSkillExecutor.h"
#include "Combat/PalSkillLibrary.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Storage/PalStorageComponent.h"
#include "Framework/PlayerDataLibrary.h"
#include "TimerManager.h"
#include "UI/TurnBattleWidget.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// 回合制接管：暂停自由战斗 AI + 强制显示头顶血条（战斗流程统一管理，结束恢复）
	void PauseAutoBattle(APalCharacter* Pal)
	{
		if (!Pal)
		{
			return;
		}
		if (UPalAutoBattleComponent* AutoBattle = Pal->GetAutoBattleComponent())
		{
			AutoBattle->Pause();
		}
		Pal->SetHPBarForced(true);
	}

	// 战斗结束恢复：解除强制显示，恢复 AI，血条按实际交战状态刷新
	void ResumeAutoBattle(APalCharacter* Pal)
	{
		if (!Pal)
		{
			return;
		}
		Pal->SetHPBarForced(false);
		if (UPalAutoBattleComponent* AutoBattle = Pal->GetAutoBattleComponent())
		{
			AutoBattle->Resume();
			Pal->SetHPBarVisible(AutoBattle->IsInCombat());
		}
	}
}

UTurnBattleComponent::UTurnBattleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	EnemyManager = CreateDefaultSubobject<UPalBattleEnemyManager>(TEXT("EnemyManager"));
}

void UTurnBattleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UTurnBattleComponent, Phase, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTurnBattleComponent, ReplicatedOurPal, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTurnBattleComponent, ReplicatedCurrentEnemy, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTurnBattleComponent, HPMedCooldown, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTurnBattleComponent, MPMedCooldown, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTurnBattleComponent, BattleMessage, COND_OwnerOnly);
}

void UTurnBattleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Logout 是主路径；Pawn 被其他系统直接 Destroy 时仍必须在服务端恢复被接管的野生帕鲁。
	if (EndPlayReason == EEndPlayReason::Destroyed && GetOwner() && GetOwner()->HasAuthority())
	{
		AbortBattleForDisconnect();
	}
	if (IsValid(LocalBattleCamera))
	{
		LocalBattleCamera->Destroy();
		LocalBattleCamera = nullptr;
	}
	if (BattleWidget)
	{
		BattleWidget->RemoveFromParent();
		BattleWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UTurnBattleComponent::TryStartBattle()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerTryStartBattle();
		return;
	}
	if (IsActive())
	{
		return;
	}

	APlayerCharacter* Player = GetOwnerPlayer();
	UWorld* World = GetWorld();
	if (!Player || !World)
	{
		return;
	}

	if (!BattleWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] TryStartBattle: 未设置 BattleWidgetClass！请在 BP_PlayerCharacter 的 TurnBattle 组件上设置 WBP_TurnBattle"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("[诊断] 回合制战斗 UI 类未设置！"));
		}
		return;
	}

	// 搜附近敌意野帕鲁（排除召唤物/捕捉中/已在战斗/已死亡的）。
	// 先只收集候选，直到 UI 与我方帕鲁都验证成功后才交给名单管理器接管。
	TArray<APalCharacter*> EnemyCandidates;
	for (TActorIterator<APalCharacter> It(World); It; ++It)
	{
		APalCharacter* Pal = *It;
		if (!Pal->IsHostile() || Pal->IsDead())
		{
			continue;
		}
		if (FVector::Dist2D(Player->GetActorLocation(), Pal->GetActorLocation()) > BattlePullRadius)
		{
			continue;
		}
		if (const UAbilitySystemComponent* ASC = Pal->GetAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag()) ||
				ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_BeingCaptured.GetTag()) ||
				ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag()))
			{
				continue;
			}
		}
		EnemyCandidates.Add(Pal);
	}

	if (EnemyCandidates.IsEmpty())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("附近没有敌意帕鲁，无法进入战斗"));
		}
		return;
	}
	EnemyCandidates.Sort([Player](const APalCharacter& Left, const APalCharacter& Right)
	{
		return FVector::DistSquared2D(Player->GetActorLocation(), Left.GetActorLocation()) <
			FVector::DistSquared2D(Player->GetActorLocation(), Right.GetActorLocation());
	});

	// 我方出战帕鲁：已有用之；否则召唤第一个存活槽
	UPalStorageComponent* Storage = GetStorage();
	if (!Storage)
	{
		return;
	}
	OurPal = Storage->EnsureSummonedPal();
	if (!OurPal.IsValid())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("没有可战斗的帕鲁（全部阵亡）！"));
		}
		return;
	}

	// 我方打 Battling 标签：暂停自由战斗、禁止实时捕捉、伤害不自动派发死亡。
	if (UAbilitySystemComponent* ASC = OurPal->GetAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag());
	}

	// 暂停我方自由战斗 AI 并强制显示头顶血条。
	PauseAutoBattle(OurPal.Get());

	// 对峙摆位：我方到玩家左前方；敌方名单只激活第一只，其余原 Actor 隐藏候场。
	PlaceOurPalInPosition(OurPal.Get());
	FVector EnemyBattleLocation = Player->GetActorLocation() +
		Player->GetActorForwardVector() * EnemyBattleDistance;
	EnemyBattleLocation.Z = Player->GetActorLocation().Z;
	if (!EnemyManager || !EnemyManager->SetupRoster(EnemyCandidates, EnemyBattleLocation, -Player->GetActorForwardVector()))
	{
		if (UAbilitySystemComponent* ASC = OurPal->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag());
		}
		ResumeAutoBattle(OurPal.Get());
		return;
	}

	// 玩家向后退场（离开画面，不承载相机）
	OriginalPlayerLocation = Player->GetActorLocation();
	bStoredOriginalPlayerLocation = true;
	{
		FHitResult Hit;
		Player->SetActorLocation(OriginalPlayerLocation - Player->GetActorForwardVector() * PlayerRetreatDistance, true, &Hit, ETeleportType::TeleportPhysics);
	}

	// 独立观战相机：垂直于双方连线、看向中点（相机完全脱离玩家）
	APalCharacter* ActiveEnemy = GetCurrentEnemy();
	const FVector Mid = (OurPal->GetActorLocation() + ActiveEnemy->GetActorLocation()) * 0.5f;
	const FVector ToEnemy = (ActiveEnemy->GetActorLocation() - OurPal->GetActorLocation()).GetSafeNormal2D();
	const FVector Perp = FVector(-ToEnemy.Y, ToEnemy.X, 0.f);
	const FVector CamPos = Mid + Perp * CameraSideDistance + FVector(0.f, 0.f, BattleCameraHeight);

	const FRotator CameraRotation = (Mid - CamPos).Rotation();
	const FTransform InitialCameraTransform(CameraRotation, CamPos);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 观战相机初始 Transform 已生成: %s，将由拥有者客户端本地创建"),
		*CamPos.ToString());

	// 玩家也进入回合制隔离：场外野生帕鲁的视觉过滤会拒绝带 Battling 标签的候选。
	if (UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent())
	{
		PlayerASC->AddLooseGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag());
	}
	// 清除战斗开始前已经锁定玩家的场外目标；标签先写入，避免清除后被视觉立即重新获取。
	int32 ClearedOutsideTargets = 0;
	for (TActorIterator<APalCharacter> It(World); It; ++It)
	{
		if (UPalAutoBattleComponent* AutoBattle = It->GetAutoBattleComponent())
		{
			if (AutoBattle->GetCombatTarget() == Player)
			{
				AutoBattle->ClearCombatTarget(EPalTargetClearReason::LostSight);
				++ClearedOutsideTargets;
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 回合制视觉隔离: 玩家已隐藏于自由 AI，清理场外目标 %d 个"), ClearedOutsideTargets);

	Phase = ETurnBattlePhase::PlayerAction;
	SetBattleMessage(FString::Printf(TEXT("遭遇战斗！%s 出现！"), *ActiveEnemy->GetClass()->GetName()));
	RefreshWidget();
	ClientOpenBattleUI(OurPal.Get(), ActiveEnemy, InitialCameraTransform);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 回合制战斗开始：敌方 %d 只（仅当前 1 只显示），我方 %s"),
		EnemyManager->GetRemainingEnemyCount(), *OurPal->GetClass()->GetName());
}

void UTurnBattleComponent::TryUseSkill(int32 SlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerTryUseSkill(SlotIndex);
		return;
	}
	if (Phase != ETurnBattlePhase::PlayerAction)
	{
		return;
	}

	APalCharacter* Pal = OurPal.Get();
	APalCharacter* Enemy = GetCurrentEnemy();
	if (!Pal || !Enemy)
	{
		return;
	}

	const TArray<FName>& Slots = Pal->GetSkillRowNames();
	if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsNone())
	{
		SetBattleMessage(TEXT("该技能槽为空"));
		RefreshWidget();
		return;
	}

	const FPalSkillRow Skill = UPalSkillLibrary::GetSkillRowChecked(Pal->SkillTable, Slots[SlotIndex]);
	UPalAttributeSet* Set = Pal->GetAttributeSet();
	if (!Set || Set->GetMP() < Skill.MPCost)
	{
		SetBattleMessage(TEXT("MP 不足！"));
		RefreshWidget();
		return;
	}

	// 扣 MP（MP 不回复，除非用通用药）
	Set->SetMP(FMath::Clamp(Set->GetMP() - Skill.MPCost, 0.f, Set->GetMaxMP()));

	FPalSkillContext SkillContext;
	SkillContext.Source = Pal;
	SkillContext.Target = Enemy;
	SkillContext.SkillRowName = Slots[SlotIndex];
	SkillContext.Skill = Skill;
	const UPalSkillExecutor* Executor = UPalSkillExecutor::GetExecutorCDO(Skill);
	const FPalSkillExecutionResult SkillResult = Executor
		? Executor->ExecuteImmediate(SkillContext)
		: FPalSkillExecutionResult();

	SetBattleMessage(FString::Printf(TEXT("%s 使用 %s，造成 %.0f 伤害"),
		*Pal->GetClass()->GetName(), *Skill.DisplayName.ToString(), SkillResult.Damage));

	Phase = ETurnBattlePhase::Resolving;
	float DelayBeforeEnemyTurn = ResolveDelay;
	if (SkillResult.bKilled)
	{
		const FString DefeatedName = Enemy->GetClass()->GetName();
		RemoveCurrentEnemy(false);
		APalCharacter* NextEnemy = GetCurrentEnemy();
		if (!NextEnemy)
		{
			ResolveVictory();
			RefreshWidget();
			return;
		}

		DelayBeforeEnemyTurn = EnemyDefeatedDelay;
		SetBattleMessage(FString::Printf(TEXT("%s 被击败！下一只 %s 准备行动..."),
			*DefeatedName, *NextEnemy->GetClass()->GetName()));
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 回合制节奏: 敌方退场，%.1f 秒后下一只行动"), DelayBeforeEnemyTurn);
	}
	GetWorld()->GetTimerManager().SetTimer(EnemyTurnTimer, this, &UTurnBattleComponent::StartEnemyTurn, DelayBeforeEnemyTurn, false);
	RefreshWidget();
}

void UTurnBattleComponent::TrySwitchPal(int32 PartyIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerTrySwitchPal(PartyIndex);
		return;
	}
	if (Phase != ETurnBattlePhase::PlayerAction)
	{
		return;
	}

	UPalStorageComponent* Storage = GetStorage();
	if (!Storage)
	{
		return;
	}

	// 校验：槽有效、有帕鲁、存活、非当前出战（切换子页面已过滤，这里兜底）
	if (!Storage->PartyPals.IsValidIndex(PartyIndex) || !Storage->PartyPals[PartyIndex].IsValid() ||
		Storage->PartyPals[PartyIndex].Health <= 0.f ||
		(Storage->HasSummonedPal() && Storage->GetSummonedPartyIndex() == PartyIndex))
	{
		SetBattleMessage(TEXT("无法切换到该帕鲁"));
		RefreshWidget();
		return;
	}

	// 收回当前 + 召唤目标（组件内部处理召回与回写）
	Storage->ActivePartyIndex = PartyIndex;
	APalCharacter* NewPal = Storage->SummonOrRecallActivePal();
	if (!NewPal)
	{
		SetBattleMessage(TEXT("切换失败"));
		RefreshWidget();
		return;
	}

	OurPal = NewPal;
	if (UAbilitySystemComponent* ASC = NewPal->GetAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag());
	}

	// 回合制接管新上场的帕鲁（召唤时会自动启用 AI，这里立刻暂停 + 强制血条）
	PauseAutoBattle(NewPal);

	// 新帕鲁站到对峙位
	PlaceOurPalInPosition(NewPal);

	SetBattleMessage(FString::Printf(TEXT("切换帕鲁！%s 上场！"), *NewPal->GetClass()->GetName()));
	Phase = ETurnBattlePhase::Resolving;
	GetWorld()->GetTimerManager().SetTimer(EnemyTurnTimer, this, &UTurnBattleComponent::StartEnemyTurn, ResolveDelay, false);
	RefreshWidget();
}

void UTurnBattleComponent::TryThrowBall()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerTryThrowBall();
		return;
	}
	if (Phase != ETurnBattlePhase::PlayerAction)
	{
		return;
	}

	APalCharacter* Enemy = GetCurrentEnemy();
	if (!Enemy)
	{
		return;
	}

	// 复用捕捉概率公式（与实时捕捉一致：残血越高概率越高、等级惩罚）
	UPalAttributeSet* Set = Enemy->GetAttributeSet();
	const float Chance = UCaptureChanceExecCalc::CalculateChance(
		Set ? Set->GetHealth() : 1.f, Set ? Set->GetMaxHealth() : 1.f, Set ? Set->GetLevel() : 1.f);
	const float Roll = FMath::FRand();
	float DelayBeforeEnemyTurn = ResolveDelay;

	if (Roll < Chance)
	{
		// 捕捉成功：快照入库（满则丢弃，与实时捕捉行为一致）+ 敌帕鲁立即退场
		FStoredPalInfo Info;
		Info.PalClass = Enemy->GetClass();
		Info.Icon = Enemy->PortraitIcon;
		if (Set)
		{
			Info.Level = Set->GetLevel();
			Info.Health = Set->GetHealth();
			Info.MaxHealth = Set->GetMaxHealth();
			Info.MP = Set->GetMP();
			Info.MaxMP = Set->GetMaxMP();
		}
		Info.SkillRowNames = Enemy->GetSkillRowNames();
		UPalSkillLibrary::NormalizeSkillSlots(Info.SkillRowNames);
		// 优先进仓库（防现捉现用：本场战斗不能立刻用刚抓的帕鲁）
		if (UPalStorageComponent* Storage = GetStorage())
		{
			Storage->AddCapturedPalToBox(Info);
		}

		RemoveCurrentEnemy(true);
		SetBattleMessage(FString::Printf(TEXT("捕捉成功！%s 进入了仓库（概率 %.0f%%）"), *Info.PalClass->GetName(), Chance * 100.f));

		if (!GetCurrentEnemy())
		{
			Phase = ETurnBattlePhase::Resolving;
			ResolveVictory();
			RefreshWidget();
			return;
		}

		DelayBeforeEnemyTurn = EnemyDefeatedDelay;
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 回合制节奏: 捕捉退场，%.1f 秒后下一只行动"), DelayBeforeEnemyTurn);
	}
	else
	{
		SetBattleMessage(FString::Printf(TEXT("捕捉失败...（概率 %.0f%%）"), Chance * 100.f));
	}

	Phase = ETurnBattlePhase::Resolving;
	GetWorld()->GetTimerManager().SetTimer(EnemyTurnTimer, this, &UTurnBattleComponent::StartEnemyTurn, DelayBeforeEnemyTurn, false);
	RefreshWidget();
}

void UTurnBattleComponent::TryUseMed(bool bHP)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerTryUseMed(bHP);
		return;
	}
	if (Phase != ETurnBattlePhase::PlayerAction)
	{
		return;
	}

	APalCharacter* Pal = OurPal.Get();
	UPalAttributeSet* Set = Pal ? Pal->GetAttributeSet() : nullptr;
	if (!Set)
	{
		return;
	}

	if (bHP)
	{
		if (HPMedCooldown > 0)
		{
			SetBattleMessage(FString::Printf(TEXT("回血药冷却中（%d 回合）"), HPMedCooldown));
			RefreshWidget();
			return;
		}
		if (Set->GetHealth() >= Set->GetMaxHealth())
		{
			SetBattleMessage(TEXT("HP 已满，无需用药"));
			RefreshWidget();
			return;
		}
		Set->SetHealth(FMath::Min(Set->GetMaxHealth(), Set->GetHealth() + Set->GetMaxHealth() * MedRestorePercent));
		HPMedCooldown = MedCooldownTurns;
		SetBattleMessage(TEXT("使用回血药：恢复 10% HP"));
	}
	else
	{
		if (MPMedCooldown > 0)
		{
			SetBattleMessage(FString::Printf(TEXT("回蓝药冷却中（%d 回合）"), MPMedCooldown));
			RefreshWidget();
			return;
		}
		if (Set->GetMP() >= Set->GetMaxMP())
		{
			SetBattleMessage(TEXT("MP 已满，无需用药"));
			RefreshWidget();
			return;
		}
		Set->SetMP(FMath::Min(Set->GetMaxMP(), Set->GetMP() + Set->GetMaxMP() * MedRestorePercent));
		MPMedCooldown = MedCooldownTurns;
		SetBattleMessage(TEXT("使用回蓝药：恢复 10% MP"));
	}

	Phase = ETurnBattlePhase::Resolving;
	GetWorld()->GetTimerManager().SetTimer(EnemyTurnTimer, this, &UTurnBattleComponent::StartEnemyTurn, ResolveDelay, false);
	RefreshWidget();
}

void UTurnBattleComponent::StartEnemyTurn()
{
	if (Phase != ETurnBattlePhase::Resolving)
	{
		return; // 胜利/失败已提前结算
	}

	APalCharacter* Enemy = GetCurrentEnemy();
	APalCharacter* Pal = OurPal.Get();
	if (!Enemy || !Pal)
	{
		ResolveDefeat();
		return;
	}

	// 敌方 AI：随机一个可用技能（MP 够且槽非空），没有则普攻（槽 0）
	const TArray<FName>& Slots = Enemy->GetSkillRowNames();
	UPalAttributeSet* ESet = Enemy->GetAttributeSet();
	FName ChosenRow = Slots.IsValidIndex(0) ? Slots[0] : NAME_None;

	TArray<int32> Candidates;
	for (int32 i = 1; i <= 3; ++i)
	{
		if (!Slots.IsValidIndex(i) || Slots[i].IsNone())
		{
			continue;
		}
		const FPalSkillRow Row = UPalSkillLibrary::GetSkillRowChecked(Enemy->SkillTable, Slots[i]);
		if (ESet && ESet->GetMP() >= Row.MPCost)
		{
			Candidates.Add(i);
		}
	}
	if (Candidates.Num() > 0)
	{
		const int32 Chosen = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		ChosenRow = Slots[Chosen];
	}

	const FPalSkillRow Skill = UPalSkillLibrary::GetSkillRowChecked(Enemy->SkillTable, ChosenRow);
	if (ESet)
	{
		ESet->SetMP(FMath::Clamp(ESet->GetMP() - Skill.MPCost, 0.f, ESet->GetMaxMP()));
	}

	FPalSkillContext SkillContext;
	SkillContext.Source = Enemy;
	SkillContext.Target = Pal;
	SkillContext.SkillRowName = ChosenRow;
	SkillContext.Skill = Skill;
	const UPalSkillExecutor* Executor = UPalSkillExecutor::GetExecutorCDO(Skill);
	const FPalSkillExecutionResult SkillResult = Executor
		? Executor->ExecuteImmediate(SkillContext)
		: FPalSkillExecutionResult();

	SetBattleMessage(FString::Printf(TEXT("%s 使用 %s，造成 %.0f 伤害"),
		*Enemy->GetClass()->GetName(), *Skill.DisplayName.ToString(), SkillResult.Damage));

	if (SkillResult.bKilled)
	{
		HandleOurPalDefeated();
	}

	const float DelayBeforePlayerTurn = SkillResult.bKilled ? OurPalDefeatedDelay : ResolveDelay;
	if (SkillResult.bKilled && Phase != ETurnBattlePhase::Defeat)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 回合制节奏: 我方换人，%.1f 秒后恢复玩家行动"), DelayBeforePlayerTurn);
	}
	GetWorld()->GetTimerManager().SetTimer(EnemyTurnTimer, this, &UTurnBattleComponent::FinishEnemyTurn, DelayBeforePlayerTurn, false);
	RefreshWidget();
}

void UTurnBattleComponent::FinishEnemyTurn()
{
	if (Phase != ETurnBattlePhase::Resolving)
	{
		return;
	}
	Phase = ETurnBattlePhase::PlayerAction;

	// 通用药冷却递减（每过一回合计 1）
	HPMedCooldown = FMath::Max(0, HPMedCooldown - 1);
	MPMedCooldown = FMath::Max(0, MPMedCooldown - 1);
	RefreshWidget();
}

void UTurnBattleComponent::HandleOurPalDefeated()
{
	UPalStorageComponent* Storage = GetStorage();
	if (!Storage)
	{
		ResolveDefeat();
		return;
	}

	// 收回阵亡帕鲁（回写 HP=0 → 该槽不可召唤，等存储回血复活）
	const FString DeadName = OurPal.IsValid() ? OurPal->GetClass()->GetName() : TEXT("帕鲁");
	Storage->RecallSummonedPal();
	OurPal = nullptr;

	// 自动换下一个存活帕鲁（阵亡切换不额外消耗回合）
	OurPal = Storage->EnsureSummonedPal();
	if (OurPal.IsValid())
	{
		if (UAbilitySystemComponent* ASC = OurPal->GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag());
		}
		// 回合制接管自动换上场的帕鲁
		PauseAutoBattle(OurPal.Get());
		PlaceOurPalInPosition(OurPal.Get());
		SetBattleMessage(FString::Printf(TEXT("%s 阵亡，%s 上场！"), *DeadName, *OurPal->GetClass()->GetName()));
	}
	else
	{
		SetBattleMessage(TEXT("所有帕鲁都倒下了..."));
		ResolveDefeat();
	}
}

void UTurnBattleComponent::RemoveCurrentEnemy(bool bCaptured)
{
	if (EnemyManager)
	{
		EnemyManager->AdvanceRoster(bCaptured);
	}
}

void UTurnBattleComponent::ResolveVictory()
{
	Phase = ETurnBattlePhase::Victory;
	SetBattleMessage(TEXT("战斗胜利！野生帕鲁全部被击败！"));
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 回合制节奏: 胜利画面保留 %.1f 秒"), VictoryDisplayDuration);
	GetWorld()->GetTimerManager().SetTimer(EndTimer, this, &UTurnBattleComponent::EndBattle, VictoryDisplayDuration, false);
}

void UTurnBattleComponent::ResolveDefeat()
{
	Phase = ETurnBattlePhase::Defeat;
	SetBattleMessage(TEXT("战斗失败..."));

	// 玩家死亡 → 3 秒后重生（APlayerCharacter::HandleDeath 内部处理）
	if (APlayerCharacter* Player = GetOwnerPlayer())
	{
		Player->HandleDeath();
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 回合制节奏: 失败画面保留 %.1f 秒"), DefeatDisplayDuration);
	GetWorld()->GetTimerManager().SetTimer(EndTimer, this, &UTurnBattleComponent::EndBattle, DefeatDisplayDuration, false);
}

void UTurnBattleComponent::EndBattle()
{
	CleanupBattle(Phase == ETurnBattlePhase::Defeat, true, TEXT("NormalEnd"));
}

void UTurnBattleComponent::AbortBattleForDisconnect()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	CleanupBattle(true, false, TEXT("PlayerDisconnected"));
}

void UTurnBattleComponent::CleanupBattle(bool bRestoreEnemies, bool bNotifyOwningClient, const TCHAR* Reason)
{
	if (bBattleCleanupInProgress)
	{
		return;
	}
	const bool bHasRuntimeState = IsActive() || OurPal.IsValid() || bStoredOriginalPlayerLocation ||
		(EnemyManager && EnemyManager->HasRoster());
	if (!bHasRuntimeState)
	{
		return;
	}
	TGuardValue<bool> CleanupGuard(bBattleCleanupInProgress, true);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnemyTurnTimer);
		World->GetTimerManager().ClearTimer(EndTimer);
	}

	APlayerCharacter* Player = GetOwnerPlayer();
	if (Player)
	{
		if (UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent())
		{
			PlayerASC->RemoveLooseGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag());
		}
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 回合制视觉隔离解除: 玩家重新对自由 AI 可见"));
	}

	// 我方存活帕鲁移除 Battling 标签 + 恢复自由战斗 AI
	if (OurPal.IsValid())
	{
		if (UAbilitySystemComponent* ASC = OurPal->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag());
		}
		ResumeAutoBattle(OurPal.Get());
		OurPal = nullptr;
	}

	// 胜利名单已经耗尽；战败、断线和异常销毁必须恢复仍存活的当前/候场 Actor。
	if (EnemyManager)
	{
		if (bRestoreEnemies)
		{
			EnemyManager->RestoreWorld();
		}
		else
		{
			EnemyManager->Clear();
		}
	}

	// 通用药冷却清除（战斗结束清冷却）
	HPMedCooldown = 0;
	MPMedCooldown = 0;

	// 收 UI + 恢复输入
	if (BattleWidget)
	{
		BattleWidget->RemoveFromParent();
		BattleWidget = nullptr;
	}
	if (Player)
	{
		if (APalPlayerController* PC = Cast<APalPlayerController>(Player->GetController()); PC && PC->IsLocalController())
		{
			PC->SetPersistentHUDVisible(true);
			PC->SetInputMode(FInputModeGameOnly());
			PC->SetShowMouseCursor(false);
		}
	}

	// 拥有者客户端在 ClientCloseBattleUI 中 Blend 回 Pawn；服务器只恢复权威位置。
	if (Player && bStoredOriginalPlayerLocation)
	{
		FHitResult Hit;
		Player->SetActorLocation(OriginalPlayerLocation, true, &Hit, ETeleportType::TeleportPhysics);
		bStoredOriginalPlayerLocation = false;
	}
	Phase = ETurnBattlePhase::Inactive;
	ReplicatedOurPal = nullptr;
	ReplicatedCurrentEnemy = nullptr;
	if (bNotifyOwningClient)
	{
		ClientCloseBattleUI();
	}
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 回合制权威清场完成：Reason=%s RestoreEnemies=%d NotifyClient=%d"),
		Reason, bRestoreEnemies, bNotifyOwningClient);
}

APalCharacter* UTurnBattleComponent::GetCurrentEnemy() const
{
	return GetOwner() && GetOwner()->HasAuthority()
		? (EnemyManager ? EnemyManager->GetActiveEnemy() : nullptr)
		: ReplicatedCurrentEnemy.Get();
}

bool UTurnBattleComponent::IsSwitchPanelVisible() const
{
	return IsPlayerActionPhase() && BattleWidget && BattleWidget->IsSwitchPanelVisible();
}

void UTurnBattleComponent::NavigateSwitchSelection(int32 Direction)
{
	if (IsSwitchPanelVisible())
	{
		BattleWidget->NavigateSwitchSelection(Direction);
	}
}

void UTurnBattleComponent::ConfirmSwitchSelection()
{
	if (IsSwitchPanelVisible())
	{
		BattleWidget->ConfirmSwitchSelection();
	}
}

void UTurnBattleComponent::CancelSwitchSelection()
{
	if (IsSwitchPanelVisible())
	{
		BattleWidget->CancelSwitchSelection();
	}
}

APlayerController* UTurnBattleComponent::GetPlayerController() const
{
	const APlayerCharacter* Player = GetOwnerPlayer();
	return Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
}

void UTurnBattleComponent::RotateBattleCamera(const FVector2D& Delta)
{
	if (!LocalBattleCamera)
	{
		return;
	}
	// 直接旋转观战相机 Actor（独立于玩家，不经过控制器输入管线；无需任何 IA 配置）
	FRotator R = LocalBattleCamera->GetActorRotation();
	R.Yaw += Delta.X * CameraRotateRate;
	R.Pitch = FMath::Clamp(R.Pitch - Delta.Y * CameraRotateRate, -60.f, 20.f);
	LocalBattleCamera->SetActorRotation(R);
}

void UTurnBattleComponent::PlaceOurPalInPosition(APalCharacter* Pal)
{
	if (!Pal)
	{
		return;
	}
	APlayerCharacter* Player = GetOwnerPlayer();
	if (!Player)
	{
		return;
	}

	// 站位：玩家左前方（前方 OurPalForwardOffset、左侧 OurPalSideOffset），与玩家同一方向（面朝敌人）
	// Z 轴与玩家对齐：双方帕鲁同一水平面（不各自贴地面，斜坡上不会高低不齐）
	FVector Loc = Player->GetActorLocation() + Player->GetActorForwardVector() * OurPalForwardOffset
		- Player->GetActorRightVector() * OurPalSideOffset;
	Loc.Z = Player->GetActorLocation().Z;
	Pal->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
	Pal->SetActorRotation(Pal->GetFacingRotation(Player->GetActorForwardVector()));
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 摆位: 我方 %s → 玩家左前方对峙位（同方向，Z=%.0f 对齐）"), *Pal->GetName(), Loc.Z);
}

APlayerCharacter* UTurnBattleComponent::GetOwnerPlayer() const
{
	return Cast<APlayerCharacter>(GetOwner());
}

UPalStorageComponent* UTurnBattleComponent::GetStorage() const
{
	return UPlayerDataLibrary::ResolvePalStorage(GetOwner());
}

void UTurnBattleComponent::RefreshWidget()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ReplicatedOurPal = OurPal.Get();
		ReplicatedCurrentEnemy = EnemyManager ? EnemyManager->GetActiveEnemy() : nullptr;
		GetOwner()->ForceNetUpdate();
	}
	if (BattleWidget)
	{
		BattleWidget->Refresh();
	}
}

void UTurnBattleComponent::OnRep_BattleState()
{
	if (Phase == ETurnBattlePhase::Inactive)
	{
		ClientCloseBattleUI_Implementation();
		return;
	}
	RefreshWidget();
}

void UTurnBattleComponent::ClientOpenBattleUI_Implementation(APalCharacter* InOurPal, APalCharacter* InEnemy,
	FTransform InitialCameraTransform)
{
	ReplicatedOurPal = InOurPal;
	ReplicatedCurrentEnemy = InEnemy;
	APlayerController* PC = GetPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 客户端观战相机切换失败：PC=%s"), *GetNameSafe(PC));
		return;
	}
	if (IsValid(LocalBattleCamera))
	{
		LocalBattleCamera->Destroy();
		LocalBattleCamera = nullptr;
	}
	FActorSpawnParameters CameraSpawnParams;
	CameraSpawnParams.Owner = PC;
	CameraSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CameraSpawnParams.ObjectFlags |= RF_Transient;
	if (UWorld* World = GetWorld())
	{
		LocalBattleCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
			InitialCameraTransform, CameraSpawnParams);
	}
	if (LocalBattleCamera)
	{
		LocalBattleCamera->SetReplicates(false);
		LocalBattleCamera->SetReplicateMovement(false);
		PC->SetViewTargetWithBlend(LocalBattleCamera, ViewTransitionDuration,
			EViewTargetBlendFunction::VTBlend_Cubic, 0.f, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[诊断] 客户端本地观战相机创建失败"));
	}
	if (!BattleWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 客户端战斗 UI 创建条件不足：Class=None"));
		return;
	}
	if (!BattleWidget)
	{
		BattleWidget = CreateWidget<UTurnBattleWidget>(PC, BattleWidgetClass);
	}
	if (!BattleWidget)
	{
		return;
	}
	BattleWidget->InitFromBattle(this);
	BattleWidget->AddToViewport(0);
	FInputModeGameAndUI Mode;
	// 独立 -game 窗口中 DoNotLock 会让右键拖拽越出视口并中断相对 Mouse XY。
	// 保留 GameAndUI 供战斗按钮点击；仅在鼠标按下捕获时隐藏光标并把拖拽限制在本窗口。
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	Mode.SetHideCursorDuringCapture(true);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	if (APalPlayerController* PalPC = Cast<APalPlayerController>(PC))
	{
		PalPC->ResetBattleCameraInputState();
		PalPC->SetPersistentHUDVisible(false);
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 客户端观战相机已切换：Camera=%s，右键拖拽输入已锁定到视口"),
		*GetNameSafe(LocalBattleCamera));
	RefreshWidget();
}

void UTurnBattleComponent::ClientCloseBattleUI_Implementation()
{
	if (BattleWidget)
	{
		BattleWidget->RemoveFromParent();
		BattleWidget = nullptr;
	}
	if (APlayerController* PC = GetPlayerController(); PC && PC->IsLocalController())
	{
		if (APawn* PlayerPawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(PlayerPawn, ViewTransitionDuration,
				EViewTargetBlendFunction::VTBlend_Cubic, 0.f, false);
		}
		if (APalPlayerController* PalPC = Cast<APalPlayerController>(PC))
		{
			PalPC->ResetBattleCameraInputState();
			PalPC->SetPersistentHUDVisible(true);
		}
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	if (IsValid(LocalBattleCamera))
	{
		LocalBattleCamera->SetLifeSpan(ViewTransitionDuration + 0.3f);
		LocalBattleCamera = nullptr;
	}
}

void UTurnBattleComponent::ServerTryStartBattle_Implementation() { TryStartBattle(); }
void UTurnBattleComponent::ServerTryUseSkill_Implementation(int32 SlotIndex) { TryUseSkill(SlotIndex); }
void UTurnBattleComponent::ServerTrySwitchPal_Implementation(int32 PartyIndex) { TrySwitchPal(PartyIndex); }
void UTurnBattleComponent::ServerTryThrowBall_Implementation() { TryThrowBall(); }
void UTurnBattleComponent::ServerTryUseMed_Implementation(bool bHP) { TryUseMed(bHP); }

void UTurnBattleComponent::SetBattleMessage(const FString& Msg)
{
	BattleMessage = Msg;
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 回合制: %s"), *Msg);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, Msg);
	}
}
