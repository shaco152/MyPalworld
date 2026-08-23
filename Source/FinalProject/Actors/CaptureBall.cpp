#include "CaptureBall.h"
#include "AbilitySystem/CapturableInterface.h"
#include "AbilitySystem/CaptureTags.h"
#include "AbilitySystem/PalAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Combat/PalSkillLibrary.h"
#include "AbilitySystemInterface.h"
#include "Characters/PalCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Storage/PalStorageComponent.h"
#include "UI/CaptureWidget.h"

ACaptureBall::ACaptureBall()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComp->InitSphereRadius(12.f);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComp->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 0.f; // 初速度由投掷能力设置
	ProjectileMovement->MaxSpeed = 0.f;
	ProjectileMovement->bShouldBounce = false; // 命中即停，不反弹
	ProjectileMovement->bRotationFollowsVelocity = true;

	WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("CaptureWidget"));
	WidgetComp->SetupAttachment(CollisionComp);
	WidgetComp->SetRelativeLocation(FVector(0.f, 0.f, 70.f)); // 悬在球上方
	WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);        // 始终面向摄像机
	WidgetComp->SetDrawSize(FVector2D(200.f, 160.f));
	WidgetComp->SetHiddenInGame(true); // 捕捉开始才显示
	WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 关键：TickMode=Enabled 防止控件不可见时组件自停 Tick（否则取消隐藏后不再渲染）
	WidgetComp->SetTickMode(ETickMode::Enabled);

	// 兜底：未命中任何目标时自动销毁
	InitialLifeSpan = 10.f;
}

void ACaptureBall::BeginPlay()
{
	Super::BeginPlay();

	CollisionComp->OnComponentHit.AddDynamic(this, &ACaptureBall::HandleHit);

	// 强制屏幕空间：始终面向玩家摄像机（BP 里若被设成 World 会导致控件看不见）
	if (WidgetComp)
	{
		WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	}

	// 控件类取子组件 WidgetComp 上的 "Widget Class"（唯一配置入口，蓝图中设为 WBP_CaptureUI）
	if (WidgetComp && WidgetComp->GetWidgetClass())
	{
		WidgetComp->InitWidget();
		if (UCaptureWidget* CaptureWidget = Cast<UCaptureWidget>(WidgetComp->GetUserWidgetObject()))
		{
			CaptureWidgetRef = CaptureWidget;
			CaptureWidget->InitFromBall(this);
		}
		UE_LOG(LogTemp, Warning, TEXT("[诊断] CaptureBall::BeginPlay %s: 控件类=%s, 控件实例=%s"),
			*GetName(), *WidgetComp->GetWidgetClass()->GetName(), *GetNameSafe(WidgetComp->GetUserWidgetObject()));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] CaptureBall::BeginPlay %s: WidgetComp 未设置 Widget Class！请在 BP_CaptureBall 的 WidgetComp 组件上设置"), *GetName());
	}
}

void ACaptureBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 由球的 Tick 驱动控件概率平滑填充（Capturing/Succeeding 阶段持续，UserWidget 自身 Tick 不可靠）
	if ((State == ECaptureBallState::Capturing || State == ECaptureBallState::Succeeding) && CaptureWidgetRef.IsValid())
	{
		CaptureWidgetRef->TickFill(DeltaSeconds);
	}

	if (State == ECaptureBallState::Capturing)
	{
		ShakeElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(ShakeElapsed / FMath::Max(ShakeDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		// 正弦震荡 + 随时间衰减：上下浮动 + 绕基础偏航摇摆（球体纯旋转不可见，浮动才是"晃动"）
		const float Wave = FMath::Sin(ShakeElapsed * ShakeFrequency * 2.f * PI) * (1.f - Alpha);
		const FVector NewLocation = HoverLocation + FVector(0.f, 0.f, Wave * ShakeBobAmplitude);
		const FRotator NewRotation(0.f, BaseYaw + Wave * ShakeAmplitudeDegrees, 0.f);
		SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::None);

		if (Alpha >= 1.f)
		{
			// 本阶段晃动结束 → 判定
			RollValue = FMath::FRand();
			const bool bRollPassed = RollValue < CaptureChance;

			if (!bRollPassed)
			{
				// 任一次判定失败 → 直接失败结算
				bCaptureSucceeded = false;
				ApplyCaptureOutcome();
			}
			else if (ShakePhase == 1)
			{
				// 第一次中了：概率提高到至少 SecondRollChance，进入第二次晃动判定
				const float PreviousChance = CaptureChance;
				CaptureChance = FMath::Max(CaptureChance, SecondRollChance);
				ShakePhase = 2;
				ShakeElapsed = 0.f;
				if (CaptureWidgetRef.IsValid())
				{
					CaptureWidgetRef->SetCaptureChance(CaptureChance); // 环继续向新概率填充
				}
				UE_LOG(LogTemp, Warning, TEXT("[诊断] 第一次判定通过(%.2f < %.2f)，概率 %.2f → %.2f，进入第二次判定"), RollValue, PreviousChance, PreviousChance, CaptureChance);
			}
			else
			{
				// 第二次也中了：捕捉成功 → 环填充到 100% 后结算销毁
				bCaptureSucceeded = true;
				State = ECaptureBallState::Succeeding;
				SucceedElapsed = 0.f;
				if (CaptureWidgetRef.IsValid())
				{
					CaptureWidgetRef->ShowSuccessFill();
				}
				else
				{
					ApplyCaptureOutcome(); // 没有控件时直接结算
				}
			}
		}
	}
	else if (State == ECaptureBallState::Succeeding)
	{
		// 等待圆环填满 100%（3 秒兜底超时）
		SucceedElapsed += DeltaSeconds;
		const bool bFillDone = !CaptureWidgetRef.IsValid() || CaptureWidgetRef->IsFillComplete();
		if (bFillDone || SucceedElapsed > 3.f)
		{
			ApplyCaptureOutcome();
		}
	}
}

void ACaptureBall::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (State != ECaptureBallState::Flying)
	{
		return;
	}

	// 忽略投掷者自身（防止边走边扔时撞到自己）
	if (OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}

	// 命中目标必须实现可捕捉接口，否则球作废
	if (!OtherActor || !OtherActor->GetClass()->ImplementsInterface(UCapturableInterface::StaticClass()))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, FString::Printf(TEXT("[诊断] 球命中非可捕捉对象 %s，作废"), OtherActor ? *OtherActor->GetName() : TEXT("?")));
		}
		Destroy();
		return;
	}

	// 目标已在被其他球捕捉 → 本球直接作废（防多球竞争）
	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OtherActor))
	{
		if (const UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_BeingCaptured.GetTag()))
			{
				Destroy();
				return;
			}

			// 出战中的帕鲁（玩家已召唤）禁止捕捉 → 本球作废
			if (ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag()))
			{
				UE_LOG(LogTemp, Warning, TEXT("[诊断] 球命中出战中的帕鲁 %s，禁止捕捉，球作废"), *OtherActor->GetName());
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("出战中的帕鲁无法被捕捉！"));
				}
				Destroy();
				return;
			}

			// 回合制战斗中的帕鲁禁止实时捕捉（只能用回合制内投球行动）
			if (ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag()))
			{
				Destroy();
				return;
			}
		}
	}

	// 已死亡的帕鲁（尸体延迟销毁中）→ 球作废
	if (const APalCharacter* PalChar = Cast<APalCharacter>(OtherActor))
	{
		if (PalChar->IsDead())
		{
			Destroy();
			return;
		}
	}

	StartCapture(OtherActor, Hit);
}

void ACaptureBall::StartCapture(AActor* Pal, const FHitResult& Hit)
{
	State = ECaptureBallState::Capturing;
	HitLocation = Hit.Location;
	CapturedPal = Pal;
	HoverLocation = GetActorLocation();
	BaseYaw = GetActorRotation().Yaw;
	PalOriginalLocation = Pal->GetActorLocation();

	// 立即悬停：停止投射物模拟（5.4 签名需要传入命中结果）
	if (ProjectileMovement)
	{
		ProjectileMovement->StopSimulating(Hit);
		ProjectileMovement->SetActive(false);
	}

	// 帕鲁进入捕捉状态（隐藏 + 无碰撞 + 停止移动 + 加标签）
	ICapturableInterface::Execute_BeginCapture(Pal, this, HitLocation);

	// 通过 GAS 查询当前捕捉概率（ExecCalc 计算并写回属性）
	CaptureChance = ICapturableInterface::Execute_GetCaptureChance(Pal);

	// 显示控件
	if (WidgetComp)
	{
		WidgetComp->SetHiddenInGame(false);
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] StartCapture: 帕鲁=%s, 概率=%.2f, 控件实例=%s, 控件隐藏=%d"),
		*Pal->GetName(), CaptureChance, *GetNameSafe(WidgetComp ? WidgetComp->GetUserWidgetObject() : nullptr),
		WidgetComp ? (int32)WidgetComp->bHiddenInGame : -1);

	// 判定不在命中时做，而在每次晃动结束时（Tick 里逐阶段判定）
	OnCaptureStarted.Broadcast(CaptureChance);
}

void ACaptureBall::ApplyCaptureOutcome()
{
	if (State != ECaptureBallState::Capturing && State != ECaptureBallState::Succeeding)
	{
		return;
	}
	State = ECaptureBallState::Resolving;

	// 最终结果广播（给蓝图调试/表现用）
	OnCaptureResolved.Broadcast(bCaptureSucceeded, CaptureChance, RollValue);

	// 成功 → 读帕鲁数值存入投掷者的存储组件（优先进背包，满则仓库），随后 ResolveCapture 销毁帕鲁
	if (bCaptureSucceeded && CapturedPal.IsValid())
	{
		if (APalCharacter* Pal = Cast<APalCharacter>(CapturedPal.Get()))
		{
			FStoredPalInfo Info;
			Info.PalClass = Pal->GetClass();
			Info.Icon = Pal->PortraitIcon; // 头像贴图随帕鲁数据入库
			if (const UPalAttributeSet* Set = Pal->GetAttributeSet())
			{
				Info.Level = Set->GetLevel();
				Info.Health = Set->GetHealth();
				Info.MaxHealth = Set->GetMaxHealth();
				Info.MP = Set->GetMP();
				Info.MaxMP = Set->GetMaxMP();
				// 落盘诊断：记录被捕捉的具体实例与读到的数值（排查"改了等级没生效"用）
				UE_LOG(LogTemp, Warning, TEXT("[诊断] 捕捉读取属性: 实例=%s | 读得 Level=%.0f/Health=%.0f/MaxHealth=%.0f/MP=%.0f"),
					*Pal->GetName(), Info.Level, Info.Health, Info.MaxHealth, Info.MP);
			}

			// 技能槽随捕捉入库（野生帕鲁默认技能）
			Info.SkillRowNames = Pal->GetSkillRowNames();
			UPalSkillLibrary::NormalizeSkillSlots(Info.SkillRowNames);

			// 投掷者 = 玩家（GA_ThrowPalSphere 已设 Instigator），兜底用 Owner
			APawn* Thrower = GetInstigator();
			if (!Thrower)
			{
				Thrower = Cast<APawn>(GetOwner());
			}
			if (UPalStorageComponent* Storage = Thrower ? Thrower->FindComponentByClass<UPalStorageComponent>() : nullptr)
			{
				Storage->AddCapturedPal(Info);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[诊断] ApplyCaptureOutcome: 投掷者 %s 上没有 UPalStorageComponent，帕鲁数据丢失"),
					*GetNameSafe(Thrower));
			}
		}
	}

	// 应用结果：失败 → 帕鲁回到捕捉前位置恢复；成功 → 打印日志并销毁帕鲁
	if (CapturedPal.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] ApplyCaptureOutcome: 成功=%d, 判定值=%.2f, 概率=%.2f, 复位位置=%s"),
			(int32)bCaptureSucceeded, RollValue, CaptureChance, *PalOriginalLocation.ToString());
		ICapturableInterface::Execute_ResolveCapture(CapturedPal.Get(), bCaptureSucceeded, PalOriginalLocation);
	}

	State = ECaptureBallState::Done;
	Destroy();
}
