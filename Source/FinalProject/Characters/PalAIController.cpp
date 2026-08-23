#include "Characters/PalAIController.h"

#include "AbilitySystem/CaptureTags.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Characters/PalCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Combat/PalAutoBattleComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

namespace
{
	void ApplyDefaultSightSettings(UAISenseConfig_Sight* Config)
	{
		if (!Config)
		{
			return;
		}

		Config->SightRadius = 1500.f; // OnPossess 按帕鲁身份覆盖
		Config->LoseSightRadius = 2250.f;
		Config->PeripheralVisionAngleDegrees = 90.f;
		Config->SetMaxAge(3.f);
		// 阵营过滤在回调内做（沿用 bHostile/Summoned/Battling/BeingCaptured 标签，不引入 TeamId）
		Config->DetectionByAffiliation.bDetectEnemies = true;
		Config->DetectionByAffiliation.bDetectNeutrals = true;
		Config->DetectionByAffiliation.bDetectFriendlies = true;
	}
}

APalAIController::APalAIController()
{
	BehaviorTreeComp = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComp"));
	BrainComponent = BehaviorTreeComp;
	BlackboardComp = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	ApplyDefaultSightSettings(SightConfig);
	PerceptionComp->ConfigureSense(*SightConfig);
	PerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());
	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &APalAIController::HandleTargetPerceptionUpdated);
}

void APalAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn)
	{
		HomeLocation = InPawn->GetActorLocation();
	}

	if (!EnsurePerceptionRuntime())
	{
		UE_LOG(LogTemp, Error, TEXT("[诊断] PalAIController::OnPossess: %s 感知运行时初始化失败，已安全跳过视觉配置"),
			*GetNameSafe(InPawn));
		return;
	}

	// Sight 半径按身份覆盖：召唤用 SummonedAggroRange；野生用 AggroRange（被动保底 300 以产生感知事件、回调拒绝获取）
	if (const APalCharacter* Pal = Cast<APalCharacter>(InPawn))
	{
		const UPalAutoBattleComponent* AutoBattle = Pal->GetAutoBattleComponent();
		if (AutoBattle && Pal->GetAbilitySystemComponent() &&
			Pal->GetAbilitySystemComponent()->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag()))
		{
			SightConfig->SightRadius = AutoBattle->GetSummonedAggroRange();
		}
		else
		{
			SightConfig->SightRadius = FMath::Max(Pal->AggroRange, 300.f);
		}
		SightConfig->LoseSightRadius = SightConfig->SightRadius * 1.5f;
		PerceptionComp->ConfigureSense(*SightConfig);
	}

	// 默认禁用视觉感知：仅行为树启动后启用（Legacy 定时器路径不依赖感知，避免目标被非 BT 通道注入）
	if (PerceptionComp)
	{
		PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	}

	// 不在此启动行为树：启动时序由组件经 StartPalBehavior 决定
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController::OnPossess: %s Home=%s SightRadius=%.0f"),
		*GetNameSafe(InPawn), *HomeLocation.ToString(), SightConfig ? SightConfig->SightRadius : -1.f);
}

bool APalAIController::EnsurePerceptionRuntime()
{
	// 蓝图派生类曾在反射结构变化后保存过空的默认子对象属性引用；优先找回已经实例化的组件。
	if (!PerceptionComp)
	{
		PerceptionComp = FindComponentByClass<UAIPerceptionComponent>();
		if (PerceptionComp)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController: 已恢复蓝图实例丢失的 PerceptionComp 属性引用"));
		}
	}
	if (!PerceptionComp)
	{
		PerceptionComp = NewObject<UAIPerceptionComponent>(this);
		if (PerceptionComp)
		{
			AddInstanceComponent(PerceptionComp);
			PerceptionComp->RegisterComponent();
			UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController: 已重建缺失的 PerceptionComp"));
		}
	}

	// SightConfig 不是 ActorComponent；先按默认子对象名找回，找不到才创建瞬态运行时实例。
	if (!SightConfig)
	{
		SightConfig = FindObjectFast<UAISenseConfig_Sight>(this, TEXT("SightConfig"));
		if (SightConfig)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController: 已恢复蓝图实例丢失的 SightConfig 属性引用"));
		}
	}
	if (!SightConfig)
	{
		SightConfig = NewObject<UAISenseConfig_Sight>(this);
		if (SightConfig)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController: 已重建缺失的 SightConfig"));
		}
	}

	if (!PerceptionComp || !SightConfig)
	{
		return false;
	}

	ApplyDefaultSightSettings(SightConfig);
	SetPerceptionComponent(*PerceptionComp);
	PerceptionComp->ConfigureSense(*SightConfig);
	PerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());
	PerceptionComp->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &APalAIController::HandleTargetPerceptionUpdated);
	return true;
}

bool APalAIController::StartPalBehavior(bool bIsSummoned)
{
	if (!BehaviorTreeAsset || !BehaviorTreeAsset->BlackboardAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController::StartPalBehavior: 未配置行为树/黑板资产（在 BP_PalAIController 设置）"));
		return false;
	}

	// 标准黑板生命周期：UseBlackboard 初始化并绑定 Controller 黑板指针；随后写初始镜像值，最后运行树
	UBlackboardComponent* InitializedBlackboard = nullptr;
	if (!UseBlackboard(BehaviorTreeAsset->BlackboardAsset, InitializedBlackboard))
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController::StartPalBehavior: 黑板初始化失败: %s"), *GetNameSafe(BehaviorTreeAsset->BlackboardAsset));
		return false;
	}
	BlackboardComp = InitializedBlackboard;

	// 初始镜像：出生锚点 / 角色身份 / 返家复位 / 清空目标
	BlackboardComp->SetValueAsVector(TEXT("HomeLocation"), HomeLocation);
	BlackboardComp->SetValueAsBool(TEXT("bIsSummoned"), bIsSummoned);
	BlackboardComp->SetValueAsBool(TEXT("bReturnHomeRequested"), false);
	BlackboardComp->SetValueAsObject(TEXT("TargetActor"), nullptr);

	if (!RunBehaviorTree(BehaviorTreeAsset))
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController::StartPalBehavior: 行为树启动失败: %s"), *GetNameSafe(BehaviorTreeAsset));
		return false;
	}

	// 启动后启用视觉感知（Legacy 模式不启用感知，避免非 BT 路径注入目标）
	if (PerceptionComp)
	{
		PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController: 行为树已启动: %s（召唤=%d）"), *GetNameSafe(BehaviorTreeAsset), (int32)bIsSummoned);
	return true;
}

void APalAIController::StopPalBehavior()
{
	if (PerceptionComp)
	{
		PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
		PerceptionComp->ForgetAll();
	}
	if (BehaviorTreeComp)
	{
		BehaviorTreeComp->StopTree();
	}
	ClearBBTarget();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController: 行为树已停止"));
}

void APalAIController::PausePalBehavior()
{
	if (PerceptionComp)
	{
		PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
		PerceptionComp->ForgetAll();
	}
	if (BehaviorTreeComp)
	{
		BehaviorTreeComp->PauseLogic(TEXT("PalPause"));
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController: 行为树已暂停"));
}

void APalAIController::ResumePalBehavior()
{
	if (BehaviorTreeComp)
	{
		BehaviorTreeComp->ResumeLogic(TEXT("PalPause"));
	}
	if (PerceptionComp)
	{
		PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalAIController: 行为树已恢复"));
}

void APalAIController::SetBBTarget(AActor* TargetActor)
{
	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsObject(TEXT("TargetActor"), TargetActor);
	}
}

void APalAIController::ClearBBTarget()
{
	SetBBTarget(nullptr);
}

void APalAIController::SetReturnHomeRequested(bool bRequested)
{
	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsBool(TEXT("bReturnHomeRequested"), bRequested);
	}
}

void APalAIController::SetSummonedRole(bool bIsSummoned)
{
	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsBool(TEXT("bIsSummoned"), bIsSummoned);
	}
}

void APalAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	UPalAutoBattleComponent* AutoBattle = nullptr;
	if (const APalCharacter* Pal = Cast<APalCharacter>(GetPawn()))
	{
		AutoBattle = Pal->GetAutoBattleComponent();
	}
	if (!AutoBattle)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		if (ShouldAcquireFromSight(Actor))
		{
			AutoBattle->AcquireTarget(Actor, EPalTargetSource::Sight);
		}
	}
	else
	{
		// 失去感知：交给组件裁决（仅 Sight 来源的目标才清除；Damage 目标不因失视野丢失）
		AutoBattle->HandleSightLost(Actor);
	}
}

bool APalAIController::ShouldAcquireFromSight(AActor* Candidate) const
{
	if (!Candidate)
	{
		return false;
	}

	const APalCharacter* Pal = Cast<APalCharacter>(GetPawn());
	const UPalAutoBattleComponent* AutoBattle = Pal ? Pal->GetAutoBattleComponent() : nullptr;
	if (!Pal || !AutoBattle)
	{
		return false;
	}

	// 自身
	if (Candidate == Pal)
	{
		return false;
	}

	// 死亡
	if (const APalCharacter* OtherPal = Cast<APalCharacter>(Candidate))
	{
		if (OtherPal->IsDead())
		{
			return false;
		}
	}
	else if (const APlayerCharacter* Player = Cast<APlayerCharacter>(Candidate))
	{
		if (Player->IsDead())
		{
			return false;
		}
	}

	// 捕捉中 / 回合制战斗中的对象不获取
	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Candidate))
	{
		if (const UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_BeingCaptured.GetTag()) ||
				ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag()))
			{
				return false;
			}
		}
	}

	const bool bSummoned = AutoBattle->IsSummonedPal();

	// 被动野生：拒绝 Sight 主动获取（受击目标仍由 Damage 路径 OnDamaged 提供）
	if (!bSummoned && Pal->AggroRange <= 0.f)
	{
		return false;
	}

	if (bSummoned)
	{
		// 召唤帕鲁：只获取合法敌意野生——显式排除带 Summoned 标签的候选（捕获后帕鲁可能保留敌意配置，避免同阵营互攻）
		const APalCharacter* TargetPal = Cast<APalCharacter>(Candidate);
		if (!TargetPal || !TargetPal->IsHostile())
		{
			return false;
		}
		const UAbilitySystemComponent* TargetASC = TargetPal->GetAbilitySystemComponent();
		return TargetASC && !TargetASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag());
	}

	// 野生主动：获取玩家方（玩家本身或召唤帕鲁）
	if (Cast<APlayerCharacter>(Candidate))
	{
		return true;
	}
	const APalCharacter* TargetPal = Cast<APalCharacter>(Candidate);
	if (!TargetPal)
	{
		return false;
	}
	const UAbilitySystemComponent* ASC = TargetPal->GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag());
}
