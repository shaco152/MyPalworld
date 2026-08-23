#include "PalCharacter.h"
#include "AbilitySystem/CaptureTags.h"
#include "AbilitySystem/GE_CaptureChance.h"
#include "AbilitySystem/PalAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Characters/PalAIController.h"
#include "Combat/PalAutoBattleComponent.h"
#include "Combat/PalSkillLibrary.h"
#include "Actors/CaptureBall.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Storage/PalStorageComponent.h"
#include "UI/PalHPBarWidget.h"

APalCharacter::APalCharacter()
{
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AttributeSet = CreateDefaultSubobject<UPalAttributeSet>(TEXT("AttributeSet"));
	AutoBattle = CreateDefaultSubobject<UPalAutoBattleComponent>(TEXT("AutoBattle"));

	// 自动战斗寻路依赖 AI 控制器（MoveToActor 路径跟随；组件本身无 Tick，事件驱动）。
	// 帕鲁 AI 控制器负责感知/黑板/行为树生命周期；帕鲁 BP 类上可覆盖 AIControllerClass
	AIControllerClass = APalAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 头顶血条（屏幕空间始终面向相机；初始隐藏，交战/回合制时显示）
	HPBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HPBar"));
	HPBarComp->SetupAttachment(RootComponent);
	HPBarComp->SetRelativeLocation(FVector(0.f, 0.f, 160.f)); // 头顶高度（BP 里可按模型调）
	HPBarComp->SetWidgetSpace(EWidgetSpace::Screen);
	HPBarComp->SetDrawSize(FVector2D(140.f, 24.f));
	HPBarComp->SetWidgetClass(UPalHPBarWidget::StaticClass());
	HPBarComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HPBarComp->SetHiddenInGame(true);
	// 关键：TickMode=Enabled 防隐藏时组件自停 Tick（项目踩过的坑）
	HPBarComp->SetTickMode(ETickMode::Enabled);
}

UAbilitySystemComponent* APalCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void APalCharacter::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] APalCharacter::BeginPlay: Actor=%s(%s)"), *GetName(), *GetClass()->GetName());
	// 编辑器直接放置时兜底（正常流程 PossessedBy 已初始化）
	InitAbilitySystem();

	// 野生帕鲁启用 AI（主动/被动都启，被动由感知过滤不主动索敌）；
	// 召唤物（Storage 在 FinishSpawning 前打 Summoned 标签）、死亡、捕捉中、回合制中不按野生启动
	if (AutoBattle && !IsDead())
	{
		const UAbilitySystemComponent* ASC = AbilitySystem;
		const bool bSummoned = ASC && ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag());
		const bool bCaptured = ASC && ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_BeingCaptured.GetTag());
		const bool bBattling = ASC && ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag());
		if (!bSummoned && !bCaptured && !bBattling)
		{
			AutoBattle->SetAutoBattleEnabled(true);
		}
	}

	// 战斗状态变化 → 血条显隐（事件驱动）
	if (AutoBattle)
	{
		AutoBattle->OnCombatStateChanged.AddDynamic(this, &APalCharacter::OnAutoBattleCombatChanged);
	}

	// 头顶血条控件初始化（懒建；隐藏状态下 InitWidget 同样可用）
	if (HPBarComp && HPBarComp->GetWidgetClass())
	{
		HPBarComp->InitWidget();
		if (UPalHPBarWidget* Bar = Cast<UPalHPBarWidget>(HPBarComp->GetUserWidgetObject()))
		{
			HPBarWidget = Bar;
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 头顶血条: %s 控件已初始化"), *GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 头顶血条: %s 控件不是 UPalHPBarWidget（实际 %s）"), *GetName(), *GetNameSafe(HPBarComp->GetUserWidgetObject()));
		}
	}
}

void APalCharacter::SetHPBarVisible(bool bVisible)
{
	if (HPBarComp)
	{
		HPBarComp->SetHiddenInGame(!bVisible);
	}
	if (bVisible)
	{
		UpdateHPBarPercent();
	}
}

void APalCharacter::SetHPBarForced(bool bForced)
{
	bHPBarForcedVisible = bForced;
	SetHPBarVisible(bForced);
}

void APalCharacter::OnAutoBattleCombatChanged(bool bInCombat)
{
	if (bHPBarForcedVisible)
	{
		return; // 回合制强制显示期间忽略战斗状态变化
	}
	SetHPBarVisible(bInCombat);
}

void APalCharacter::OnHealthAttributeChanged(float Health, float MaxHealth)
{
	// 事件驱动：仅在血条可见时刷新（SetHPBarVisible 时也会补刷一次）
	if (HPBarComp && !HPBarComp->bHiddenInGame)
	{
		UpdateHPBarPercent();
	}
}

void APalCharacter::UpdateHPBarPercent()
{
	if (HPBarWidget.IsValid() && AttributeSet)
	{
		const float Percent = AttributeSet->GetMaxHealth() > 0.f ? AttributeSet->GetHealth() / AttributeSet->GetMaxHealth() : 0.f;
		HPBarWidget->UpdateBar(Percent);
	}
}

void APalCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// AI 控制器接管（自动战斗寻路用；正常流程，勿与玩家接管混淆）
	UE_LOG(LogTemp, Warning, TEXT("[诊断] APalCharacter::PossessedBy: Actor=%s(%s), Controller=%s"), *GetName(), *GetClass()->GetName(), *GetNameSafe(NewController));
	InitAbilitySystem();
}

void APalCharacter::InitAbilitySystem()
{
	if (!AbilitySystem)
	{
		return;
	}

	// 坑：GetOwnerActor() 在组件注册阶段就可能被引擎预设——首次调用 InitAbilitySystem 时已非空，
	// 用"GetOwnerActor()!=nullptr 即已初始化"做守卫会把整个初始化体跳过（等级配置永远不生效）。
	if (AbilitySystem->GetOwnerActor() != this)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}

	// 注册属性集（5.x 不会自动发现 Pawn 子对象上的属性集；AddSpawnedAttribute 内部查重，可反复调用）
	AbilitySystem->AddSpawnedAttribute(AttributeSet);

	// 血量变化委托绑定（幂等守卫：InitAbilitySystem 会被多次调用）
	if (!bHealthBound)
	{
		bHealthBound = true;
		AttributeSet->OnHealthChanged.AddDynamic(this, &APalCharacter::OnHealthAttributeChanged);
	}

	// 用设计器配置的初始数值覆盖属性集（编辑器改的是普通 float 配置项；
	// FGameplayAttributeData 的 Base/Current 成员在引擎里是 protected 不可编辑，且运行时读 CurrentValue）
	AttributeSet->InitHealth(InitialHealth);
	AttributeSet->InitMaxHealth(InitialMaxHealth);
	AttributeSet->InitLevel(InitialLevel);
	AttributeSet->InitMP(InitialMP);
	AttributeSet->InitMaxMP(InitialMaxMP);

	// 技能槽初始化：运行时槽为空 → 取 BP 配置的默认技能（野生帕鲁初始技能）
	if (SkillRowNames.IsEmpty())
	{
		SkillRowNames = DefaultSkillRowNames;
	}
	UPalSkillLibrary::NormalizeSkillSlots(SkillRowNames);

	// 落盘诊断：确认每个帕鲁实例实际应用的初始数值（排查"改了等级没生效"用）
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 属性初始化: %s | 配置 InitialLevel=%.0f/InitialHealth=%.0f/InitialMP=%.0f → 属性集 Level=%.0f/Health=%.0f/MP=%.0f/技能槽=%d"),
		*GetName(), InitialLevel, InitialHealth, InitialMP, AttributeSet->GetLevel(), AttributeSet->GetHealth(), AttributeSet->GetMP(), SkillRowNames.Num());
}

FRotator APalCharacter::GetFacingRotation(const FVector& Direction) const
{
	FRotator R = Direction.Rotation();
	R.Yaw += FacingYawOffset;
	return R;
}

void APalCharacter::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	// 停止自动战斗与移动 + 隐藏血条
	if (AutoBattle)
	{
		AutoBattle->SetAutoBattleEnabled(false);
	}
	SetHPBarVisible(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] APalCharacter::HandleDeath: %s 死亡"), *GetName());

	// 被召唤的帕鲁：通知主人存储组件收回（回写 HP=0 → 不可召唤，等存储回血复活）
	if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag()))
	{
		if (AActor* OwnerActor = GetOwner())
		{
			if (UPalStorageComponent* Storage = OwnerActor->FindComponentByClass<UPalStorageComponent>())
			{
				Storage->HandleSummonedPalDeath(this);
			}
		}
		return; // 实体已由存储组件销毁
	}

	// 野生帕鲁：尸体延迟销毁
	SetLifeSpan(2.f);
}

void APalCharacter::OnDamaged(AActor* Attacker)
{
	if (bIsDead || !AutoBattle || !Attacker)
	{
		return;
	}
	// 反击：锁定攻击者并激活自动战斗（即使 AggroRange=0 的被动野帕鲁）
	AutoBattle->SetTarget(Attacker);
	AutoBattle->SetAutoBattleEnabled(true);
}

float APalCharacter::GetCaptureChance_Implementation() const
{
	if (!AbilitySystem || !AttributeSet)
	{
		return 0.f;
	}

	// 应用即时 GE：ExecCalc 读血量/等级并把概率写回 CaptureChance 属性
	const UGameplayEffect* ChanceGE = GetDefault<UGE_CaptureChance>();
	if (!ChanceGE)
	{
		return 0.f;
	}

	const FGameplayEffectSpec Spec(ChanceGE, AbilitySystem->MakeEffectContext(), 1.f);
	AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec);

	return AttributeSet->GetCaptureChance();
}

void APalCharacter::BeginCapture_Implementation(ACaptureBall* Ball, const FVector& HitLocation)
{
	// 防多球竞争标签
	if (AbilitySystem)
	{
		AbilitySystem->AddLooseGameplayTag(CaptureTags::TAG_State_Pal_BeingCaptured.GetTag());
	}

	// 不可见（组件级隐藏，不用 SetActorHiddenInGame 以免整 Actor 隐藏）
	GetMesh()->SetVisibility(false, true);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 停止移动
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->SetMovementMode(MOVE_None);
	}
}

void APalCharacter::ResolveCapture_Implementation(bool bSuccess, const FVector& HitLocation)
{
	if (AbilitySystem)
	{
		AbilitySystem->RemoveLooseGameplayTag(CaptureTags::TAG_State_Pal_BeingCaptured.GetTag());
	}

	if (bSuccess)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("%s 捕捉成功!"), *GetName()));
		}
		Destroy();
		return;
	}

	// 失败：回到捕捉前位置（球传入），恢复可见 / 碰撞 / 移动
	SetActorLocation(HitLocation, false, nullptr, ETeleportType::TeleportPhysics);
	GetMesh()->SetVisibility(true, true);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Falling); // 落地后自动转回 Walking
	}
}
