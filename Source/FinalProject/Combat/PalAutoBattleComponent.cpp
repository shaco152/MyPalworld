#include "PalAutoBattleComponent.h"
#include "AbilitySystem/CaptureTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AIController.h"
#include "Characters/PalAIController.h"
#include "Characters/PalCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Combat/PalSkillExecutor.h"
#include "Combat/PalSkillLibrary.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h" // EPathFollowingRequestResult 枚举值定义处
#include "TimerManager.h"

UPalAutoBattleComponent::UPalAutoBattleComponent()
{
	// 事件驱动：无 Tick（用户铁律，禁止每帧轮询）
	PrimaryComponentTick.bCanEverTick = false;
}

void UPalAutoBattleComponent::SetAutoBattleEnabled(bool bInEnabled)
{
	if (bActive == bInEnabled)
	{
		if (!bActive)
		{
			ClearCombatTarget(EPalTargetClearReason::Disabled);
			if (AIDriver == EPalAIDriver::BehaviorTree)
			{
				StopBehaviorTreeDriver();
			}
			else
			{
				StopTimers();
			}
		}
		// 行为树资产若在首次启动时尚未就绪，允许再次显式启用时重试。
		else if (!bPaused && AIDriver == EPalAIDriver::BehaviorTree && !bBehaviorTreeStarted)
		{
			StartBehaviorTreeDriver();
		}
		return;
	}

	bActive = bInEnabled;
	if (bActive)
	{
		if (!bPaused)
		{
			if (AIDriver == EPalAIDriver::BehaviorTree)
			{
				StartBehaviorTreeDriver();
			}
			else
			{
				StartTimers();
			}
		}
	}
	else
	{
		ClearCombatTarget(EPalTargetClearReason::Disabled);
		if (AIDriver == EPalAIDriver::BehaviorTree)
		{
			StopBehaviorTreeDriver();
		}
		else
		{
			StopTimers();
		}
	}
}

void UPalAutoBattleComponent::Pause()
{
	if (bPaused)
	{
		return;
	}
	bPaused = true;
	if (AIDriver == EPalAIDriver::BehaviorTree)
	{
		// 先清目标再 ForgetAll，避免失视回调把 Pause 错映射成返家请求。
		ClearCombatTarget(EPalTargetClearReason::Pause);
		if (bBehaviorTreeStarted)
		{
			if (APalAIController* Controller = GetPalAIController())
			{
				Controller->PausePalBehavior();
			}
		}
	}
	else
	{
		StopTimers();
		ClearCombatTarget(EPalTargetClearReason::Pause);
	}
}

void UPalAutoBattleComponent::Resume()
{
	if (!bPaused)
	{
		return;
	}
	bPaused = false;
	if (bActive)
	{
		if (AIDriver == EPalAIDriver::BehaviorTree)
		{
			if (bBehaviorTreeStarted)
			{
				if (APalAIController* Controller = GetPalAIController())
				{
					Controller->ResumePalBehavior();
				}
				else
				{
					bBehaviorTreeStarted = false;
					StartBehaviorTreeDriver();
				}
			}
			else
			{
				StartBehaviorTreeDriver();
			}
		}
		else
		{
			StartTimers();
		}
	}
}

bool UPalAutoBattleComponent::StartBehaviorTreeDriver()
{
	StopTimers();
	APalAIController* Controller = GetPalAIController();
	if (!Controller)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] BehaviorTree Driver 启动失败: %s 未由 APalAIController 控制"), *GetNameSafe(GetOwner()));
		bBehaviorTreeStarted = false;
		return false;
	}

	bBehaviorTreeStarted = Controller->StartPalBehavior(IsSummonedPal());
	if (!bBehaviorTreeStarted)
	{
		SetCombatState(false);
		return false;
	}
	if (bBehaviorTreeStarted && Target.IsValid())
	{
		Controller->SetReturnHomeRequested(false);
		Controller->SetBBTarget(Target.Get());
		SetCombatState(true);
	}
	return bBehaviorTreeStarted;
}

void UPalAutoBattleComponent::StopBehaviorTreeDriver()
{
	if (bBehaviorTreeStarted)
	{
		if (APalAIController* Controller = GetPalAIController())
		{
			Controller->StopPalBehavior();
		}
	}
	bBehaviorTreeStarted = false;
}

APalAIController* UPalAutoBattleComponent::GetPalAIController() const
{
	const APalCharacter* Pal = Cast<APalCharacter>(GetOwner());
	return Pal ? Cast<APalAIController>(Pal->GetController()) : nullptr;
}

void UPalAutoBattleComponent::StartTimers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!SearchTimer.IsValid())
	{
		World->GetTimerManager().SetTimer(SearchTimer, this, &UPalAutoBattleComponent::SearchForTarget, TargetSearchInterval, true);
	}
	if (!BehaviourTimer.IsValid())
	{
		World->GetTimerManager().SetTimer(BehaviourTimer, this, &UPalAutoBattleComponent::UpdateBehaviour, BehaviourUpdateInterval, true);
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] AutoBattle 定时器启动: %s（索敌 %.1fs / 行为 %.2fs）"), *GetNameSafe(GetOwner()), TargetSearchInterval, BehaviourUpdateInterval);
}

void UPalAutoBattleComponent::StopTimers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (SearchTimer.IsValid())
	{
		World->GetTimerManager().ClearTimer(SearchTimer);
	}
	if (BehaviourTimer.IsValid())
	{
		World->GetTimerManager().ClearTimer(BehaviourTimer);
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] AutoBattle 定时器停止: %s"), *GetNameSafe(GetOwner()));
}

void UPalAutoBattleComponent::SetTarget(AActor* InTarget)
{
	AcquireTarget(InTarget, EPalTargetSource::Damage);
}

bool UPalAutoBattleComponent::AcquireTarget(AActor* Candidate, EPalTargetSource Source)
{
	if (!IsValid(Candidate) || bPaused)
	{
		return false;
	}

	// 回合制隔离统一收口：带 Battling 标签的对象（战斗双方/玩家）不进入自由战斗目标。
	// BT 感知回调（ShouldAcquireFromSight）已过滤，此处覆盖 Legacy 索敌/受击两条路径
	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Candidate))
	{
		if (const UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag()))
			{
				return false;
			}
		}
	}

	// Sight 不抢占仍有效的目标；Damage 可覆盖，保证受击反击优先。
	if (Source == EPalTargetSource::Sight && Target.IsValid() && Target.Get() != Candidate)
	{
		return false;
	}

	const bool bKeepDamagePriority = Target.Get() == Candidate && TargetSource == EPalTargetSource::Damage;
	if (Target.Get() != Candidate)
	{
		UnbindTargetEndPlay();
	}
	Target = Candidate;
	BindTargetEndPlay(Candidate);
	if (!bKeepDamagePriority)
	{
		TargetSource = Source;
	}
	if (AIDriver == EPalAIDriver::BehaviorTree)
	{
		if (APalAIController* Controller = GetPalAIController())
		{
			Controller->SetReturnHomeRequested(false);
			Controller->SetBBTarget(Candidate);
		}
		SetCombatState(true);
	}
	else
	{
		MoveToTarget(Candidate);
		SetCombatState(true);
	}
	return true;
}

void UPalAutoBattleComponent::ClearCombatTarget(EPalTargetClearReason Reason)
{
	if (AIDriver == EPalAIDriver::BehaviorTree)
	{
		if (APalAIController* Controller = GetPalAIController())
		{
			// 先清镜像触发 Observer Abort，让潜伏任务先解绑自己的请求/Timer。
			Controller->ClearBBTarget();
		}
	}

	ClearTargetInternal();

	if (AIDriver == EPalAIDriver::BehaviorTree)
	{
		if (APalAIController* Controller = GetPalAIController())
		{
			const bool bReturnHome = !IsSummonedPal() &&
				(Reason == EPalTargetClearReason::LostSight ||
				 Reason == EPalTargetClearReason::Killed ||
				 Reason == EPalTargetClearReason::Invalid);
			Controller->SetReturnHomeRequested(bReturnHome);
		}
	}
}

void UPalAutoBattleComponent::HandleSightLost(AActor* Actor)
{
	if (Actor && Target.Get() == Actor && TargetSource == EPalTargetSource::Sight)
	{
		ClearCombatTarget(EPalTargetClearReason::LostSight);
	}
}

void UPalAutoBattleComponent::ClearTargetInternal()
{
	UnbindTargetEndPlay();
	if (Target.IsValid())
	{
		if (APalCharacter* Pal = Cast<APalCharacter>(GetOwner()))
		{
			if (AAIController* AI = Cast<AAIController>(Pal->GetController()))
			{
				AI->StopMovement();
			}
		}
	}
	Target = nullptr;
	TargetSource = EPalTargetSource::Sight;
	bLastMoveSucceeded = false;
	SetCombatState(false);
}

void UPalAutoBattleComponent::BindTargetEndPlay(AActor* InTarget)
{
	if (InTarget)
	{
		InTarget->OnEndPlay.AddUniqueDynamic(this, &UPalAutoBattleComponent::HandleTargetEndPlay);
	}
}

void UPalAutoBattleComponent::UnbindTargetEndPlay()
{
	if (AActor* CurrentTarget = Target.Get())
	{
		CurrentTarget->OnEndPlay.RemoveDynamic(this, &UPalAutoBattleComponent::HandleTargetEndPlay);
	}
}

void UPalAutoBattleComponent::HandleTargetEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	if (Actor && Target.Get() == Actor)
	{
		ClearCombatTarget(EPalTargetClearReason::Invalid);
	}
}

void UPalAutoBattleComponent::SetCombatState(bool bInCombat)
{
	if (bInCombatState != bInCombat)
	{
		bInCombatState = bInCombat;

		// 交战提速/恢复：否则与玩家同速（600）或比冲刺（1000）慢，永远追不进攻击距离 → 永远不打
		if (APalCharacter* Pal = Cast<APalCharacter>(GetOwner()))
		{
			if (UCharacterMovementComponent* MoveComp = Pal->GetCharacterMovement())
			{
				if (bInCombat)
				{
					if (BaseWalkSpeed <= 0.f)
					{
						BaseWalkSpeed = MoveComp->MaxWalkSpeed;
					}
					MoveComp->MaxWalkSpeed = BaseWalkSpeed * CombatSpeedMultiplier;
					UE_LOG(LogTemp, Warning, TEXT("[诊断] 交战提速: %s %.0f → %.0f"), *Pal->GetName(), BaseWalkSpeed, MoveComp->MaxWalkSpeed);
				}
				else if (BaseWalkSpeed > 0.f)
				{
					MoveComp->MaxWalkSpeed = BaseWalkSpeed;
					BaseWalkSpeed = 0.f;
				}
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[诊断] 战斗状态变化: %s %s"), *GetNameSafe(GetOwner()), bInCombat ? TEXT("进入交战") : TEXT("脱离交战"));
		OnCombatStateChanged.Broadcast(bInCombat);
	}
}

void UPalAutoBattleComponent::MoveToTarget(AActor* InTarget)
{
	APalCharacter* Pal = Cast<APalCharacter>(GetOwner());
	if (!Pal || !InTarget)
	{
		return;
	}

	AAIController* AI = Cast<AAIController>(Pal->GetController());
	if (!AI)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] %s 没有 AIController，无法追击（检查 APalCharacter::AIControllerClass）"), *Pal->GetName());
		return;
	}

	// 目标几乎没动且上次寻路成功 → 不重复发寻路请求（0.15s 高频重发会造成移动抖动）
	if (bLastMoveSucceeded && FVector::DistSquared2D(InTarget->GetActorLocation(), LastMoveTargetLoc) < 150.f * 150.f)
	{
		return;
	}
	LastMoveTargetLoc = InTarget->GetActorLocation();

	// 导航寻路（bUsePathfinding=true）：绕过岩石/地形障碍。
	// 关键（体型无关）：接受半径 = 双方胶囊半径 + 60 余量——巨型帕鲁胶囊相抵点可能在数百 cm 外，
	// 固定小接受半径物理上不可达，寻路会硬顶导致"回溯"。bStopOnOverlap=false 避免再叠加自身半径。
	const float Acceptance = CapsuleRadiusOf(Pal) + CapsuleRadiusOf(InTarget) + 60.f;
	const EPathFollowingRequestResult::Type Result = AI->MoveToActor(InTarget, Acceptance, false, true, false);
	// Failed=0 / AlreadyAtGoal=1 / RequestSuccessful=2：AlreadyAtGoal 也视为成功（够近了，交给攻击分支）
	bLastMoveSucceeded = (Result != EPathFollowingRequestResult::Failed);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] %s MoveToActor Result=%d 接受半径=%.0f（0失败/1已在目标/2成功）"), *Pal->GetName(), (int32)Result, Acceptance);
}

float UPalAutoBattleComponent::CapsuleRadiusOf(const AActor* Actor)
{
	if (const ACharacter* Char = Cast<ACharacter>(Actor))
	{
		if (const UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
		{
			return Capsule->GetScaledCapsuleRadius();
		}
	}
	return 0.f;
}

void UPalAutoBattleComponent::SearchForTarget()
{
	APalCharacter* Pal = Cast<APalCharacter>(GetOwner());
	if (AIDriver != EPalAIDriver::LegacyTimer || !Pal || Pal->IsDead() || bPaused || !bActive || Target.IsValid())
	{
		return;
	}

	// 捕捉中的帕鲁暂停索敌
	if (const UAbilitySystemComponent* ASC = Pal->GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_BeingCaptured.GetTag()))
		{
			return;
		}
	}

	if (AActor* Found = FindNearestTarget())
	{
		if (AcquireTarget(Found, EPalTargetSource::Sight))
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 索敌: %s → %s"), *Pal->GetName(), *Found->GetName());
		}
		return;
	}

	// 无目标：被召唤的帕鲁跟随召唤者（保持 FollowDistance，不再呆站原地；导航寻路绕过障碍）
	if (IsSummonedPal())
	{
		if (AActor* OwnerActor = Pal->GetOwner())
		{
			if (AAIController* AI = Cast<AAIController>(Pal->GetController()))
			{
				AI->MoveToActor(OwnerActor, FollowDistance, true, true, false);
			}
		}
	}
}

void UPalAutoBattleComponent::UpdateBehaviour()
{
	APalCharacter* Pal = Cast<APalCharacter>(GetOwner());
	if (AIDriver != EPalAIDriver::LegacyTimer || !Pal || Pal->IsDead() || bPaused || !bActive)
	{
		return;
	}

	// 捕捉中的帕鲁暂停行为
	if (const UAbilitySystemComponent* ASC = Pal->GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_BeingCaptured.GetTag()))
		{
			return;
		}
	}

	// 目标死亡/被销毁失效 → 清目标并广播，交给索敌重新找（避免静默卡死）
	if (Target.IsValid())
	{
		bool bTargetDead = false;
		if (const APalCharacter* TargetPal = Cast<APalCharacter>(Target.Get()))
		{
			bTargetDead = TargetPal->IsDead();
		}
		else if (const APlayerCharacter* TargetPlayer = Cast<APlayerCharacter>(Target.Get()))
		{
			bTargetDead = TargetPlayer->IsDead();
		}
		if (bTargetDead)
		{
			ClearCombatTarget(EPalTargetClearReason::Killed);
			return;
		}
	}
	else
	{
		ClearCombatTarget(EPalTargetClearReason::Invalid);
		return;
	}

	AttackCooldown = FMath::Max(0.f, AttackCooldown - BehaviourUpdateInterval);

	const FVector MyLoc = Pal->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();
	const FVector Dir = (TargetLoc - MyLoc).GetSafeNormal2D();
	const float Dist = FVector::Dist2D(MyLoc, TargetLoc);

	// 体型无关：用"表面到表面"距离判定攻击（减去双方胶囊半径）。
	// 巨型帕鲁中心距可能数百 cm 也无法再近，按中心距判定永远打不到。
	const float PalRadius = CapsuleRadiusOf(Pal);
	const float TargetRadius = CapsuleRadiusOf(Target.Get());
	const float SurfaceDist = FMath::Max(0.f, Dist - PalRadius - TargetRadius);

	// 每秒一条行为状态日志（排查"有目标却不追击/不攻击"；移动模式：0无/1行走/2下落/3游泳/4飞行）
	DebugLogAccumulator += BehaviourUpdateInterval;
	if (DebugLogAccumulator >= 1.f)
	{
		DebugLogAccumulator = 0.f;
		const int32 MoveMode = Pal->GetCharacterMovement() ? (int32)Pal->GetCharacterMovement()->MovementMode : -1;
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 行为: %s 目标=%s 中心距=%.0f 表面距=%.0f（半径%.0f/%.0f）冷却=%.1f 分支=%s 移动模式=%d"),
			*Pal->GetName(), *Target->GetName(), Dist, SurfaceDist, PalRadius, TargetRadius, AttackCooldown,
			SurfaceDist > Pal->AttackRange ? TEXT("追击") : TEXT("攻击"), MoveMode);
	}

	if (SurfaceDist > Pal->AttackRange)
	{
		// 追击：只刷新寻路，不硬转面向——避免与 bOrientRotationToMovement 打架（来回抽搐/回溯感）
		MoveToTarget(Target.Get());
	}
	else
	{
		// 攻击范围内：面向目标（叠加模型朝向修正角），冷却完毕打普攻
		UE_LOG(LogTemp, Warning, TEXT("[诊断] %s 进入攻击分支：目标=%s 表面距=%.0f 冷却=%.1f"), *Pal->GetName(), *Target->GetName(), SurfaceDist, AttackCooldown);
		if (!Dir.IsNearlyZero())
		{
			Pal->SetActorRotation(Pal->GetFacingRotation(Dir));
		}
		if (AttackCooldown <= 0.f)
		{
			if (AAIController* AI = Cast<AAIController>(Pal->GetController()))
			{
				AI->StopMovement();
			}
			TryBasicAttack(Target.Get());
		}
	}
}

bool UPalAutoBattleComponent::TryBasicAttack(AActor* InTarget)
{
	APalCharacter* Pal = Cast<APalCharacter>(GetOwner());
	if (!Pal || !InTarget)
	{
		return false;
	}

	// 普攻 = 技能槽 0：经统一执行器结算（伤害公式按行 DamagePerLevel，演出按行 RangeType 取帕鲁类蒙太奇）
	const TArray<FName>& Slots = Pal->GetSkillRowNames();
	const FName BasicSkill = Slots.IsValidIndex(0) ? Slots[0] : NAME_None;
	const FPalSkillRow Skill = UPalSkillLibrary::GetSkillRowChecked(Pal->SkillTable, BasicSkill);

	FPalSkillContext SkillContext;
	SkillContext.Source = Pal;
	SkillContext.Target = InTarget;
	SkillContext.SkillRowName = BasicSkill;
	SkillContext.Skill = Skill;

	FPalSkillExecutionResult Result;
	if (const UPalSkillExecutor* Executor = UPalSkillExecutor::GetExecutorCDO(Skill))
	{
		Result = Executor->ExecuteImmediate(SkillContext);
	}

	AttackCooldown = Pal->AttackInterval;
	if (Result.bKilled && Target.Get() == InTarget)
	{
		ClearCombatTarget(EPalTargetClearReason::Killed);
	}
	return Result.bApplied;
}

bool UPalAutoBattleComponent::IsSummonedPal() const
{
	const APalCharacter* Pal = Cast<APalCharacter>(GetOwner());
	return Pal && Pal->GetAbilitySystemComponent() &&
		Pal->GetAbilitySystemComponent()->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag());
}

AActor* UPalAutoBattleComponent::FindNearestTarget() const
{
	const APalCharacter* Pal = Cast<APalCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Pal || !World)
	{
		return nullptr;
	}

	// 身份：被召唤的帕鲁打敌意野帕鲁；野生敌意帕鲁打玩家 + 玩家召唤的帕鲁
	const bool bSummoned = IsSummonedPal();

	const float SearchRadius = bSummoned ? SummonedAggroRange : Pal->AggroRange;
	if (SearchRadius <= 0.f)
	{
		return nullptr; // 野生被动模式（反击由 OnDamaged 直接设目标，不走索敌）
	}

	const FVector MyLoc = Pal->GetActorLocation();
	const float RadiusSq = SearchRadius * SearchRadius;
	AActor* Best = nullptr;
	float BestDistSq = FLT_MAX;

	// 候选判定：距离内且更近则选中
	auto Consider = [&](AActor* Candidate)
	{
		if (!Candidate)
		{
			return;
		}
		const float DistSq = FVector::DistSquared2D(MyLoc, Candidate->GetActorLocation());
		if (DistSq <= RadiusSq && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	};

	if (bSummoned)
	{
		// 玩家帕鲁：索敌敌意野生帕鲁（跳过死者、召唤物、战斗中的）
		for (TActorIterator<APalCharacter> It(World); It; ++It)
		{
			APalCharacter* Other = *It;
			if (Other == Pal || !Other->IsHostile() || Other->IsDead())
			{
				continue;
			}
			const UAbilitySystemComponent* ASC = Other->GetAbilitySystemComponent();
			if (ASC && (ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag()) ||
				ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag())))
			{
				continue;
			}
			Consider(Other);
		}
	}
	else
	{
		// 野生帕鲁：索敌玩家与玩家召唤的帕鲁
		for (TActorIterator<APlayerCharacter> It(World); It; ++It)
		{
			APlayerCharacter* Player = *It;
			if (!Player->IsDead())
			{
				Consider(Player);
			}
		}
		for (TActorIterator<APalCharacter> It(World); It; ++It)
		{
			APalCharacter* Other = *It;
			if (Other == Pal || Other->IsDead())
			{
				continue;
			}
			const UAbilitySystemComponent* ASC = Other->GetAbilitySystemComponent();
			if (ASC && ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag()))
			{
				Consider(Other);
			}
		}
	}

	return Best;
}
