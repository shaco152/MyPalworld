#include "PlayerCharacter.h"
#include "AbilitySystem/CaptureTags.h"
#include "AbilitySystem/GA_PlayerAttack.h"
#include "AbilitySystem/GA_ThrowPalSphere.h"
#include "AbilitySystem/PlayerAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Combat/TurnBattleComponent.h"
#include "Engine/Engine.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpringArmComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Storage/PalStorageComponent.h"

APlayerCharacter::APlayerCharacter()
{
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	PlayerAttributeSet = CreateDefaultSubobject<UPlayerAttributeSet>(TEXT("PlayerAttributeSet"));
	StorageComponent = CreateDefaultSubobject<UPalStorageComponent>(TEXT("PalStorage"));
	TurnBattle = CreateDefaultSubobject<UTurnBattleComponent>(TEXT("TurnBattle"));
	ThrowAbilityClass = UGA_ThrowPalSphere::StaticClass();
	AttackAbilityClass = UGA_PlayerAttack::StaticClass();

	// 第三人称相机：弹簧臂跟随 PawnControlRotation（只影响相机，不影响角色）
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// 角色偏航跟随控制器（瞄准/移动方向与相机一致），俯仰不跟随（避免角色前后倾）
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

UAbilitySystemComponent* APlayerCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

bool APlayerCharacter::IsInTurnBattle() const
{
	return TurnBattle && TurnBattle->IsActive();
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] APlayerCharacter::BeginPlay: Actor=%s(%s)"), *GetName(), *GetClass()->GetName());
	// 编辑器直接放置时兜底（正常流程 PossessedBy 已初始化）
	InitAbilitySystem();
}

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] APlayerCharacter::PossessedBy: Actor=%s(%s), Controller=%s"), *GetName(), *GetClass()->GetName(), *GetNameSafe(NewController));
	InitAbilitySystem();
}

void APlayerCharacter::InitAbilitySystem()
{
	if (!AbilitySystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] APlayerCharacter::InitAbilitySystem: AbilitySystem 为空！"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] InitAbilitySystem: AbilityActorInfo有效=%d, ThrowAbilityClass=%s"), AbilitySystem->AbilityActorInfo.IsValid(), *GetNameSafe(ThrowAbilityClass));

	// 初始化（幂等：AbilityActorInfo 已存在则跳过）
	if (!AbilitySystem->AbilityActorInfo.IsValid())
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}

	// 注册玩家属性集（与帕鲁同模式：AddSpawnedAttribute 查重幂等；数值配置走普通 float）
	AbilitySystem->AddSpawnedAttribute(PlayerAttributeSet);
	PlayerAttributeSet->InitHealth(InitialHealth);
	PlayerAttributeSet->InitMaxHealth(InitialMaxHealth);

	// 授予投掷/攻击能力（幂等）
	GrantAbilityIfMissing(ThrowAbilityClass, CaptureTags::TAG_InputTag_Throw.GetTag());
	GrantAbilityIfMissing(AttackAbilityClass, CaptureTags::TAG_InputTag_Attack.GetTag());
}

void APlayerCharacter::GrantAbilityIfMissing(TSubclassOf<UGameplayAbility> AbilityClass, const FGameplayTag& InputTag)
{
	if (!AbilityClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] GrantAbilityIfMissing: 能力类为空，跳过授予"));
		return;
	}

	// 已存在同类型能力则跳过
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA(AbilityClass))
		{
			return;
		}
	}

	FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
	// 在 Spec 层显式打上输入标签（BP 子类 CDO 的 AbilityTags 继承不可靠时的保险）
	Spec.DynamicAbilityTags.AddTag(InputTag);
	AbilitySystem->GiveAbility(Spec);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 已授予能力 %s（标签 %s）"), *AbilityClass->GetName(), *InputTag.ToString());
}

void APlayerCharacter::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	// 隐藏 + 无碰撞 + 禁用输入
	SetActorEnableCollision(false);
	GetMesh()->SetVisibility(false, true);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] APlayerCharacter::HandleDeath: 玩家死亡，3 秒后重生"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("玩家死亡，3 秒后重生"));
	}

	GetWorldTimerManager().SetTimer(RespawnTimer, this, &APlayerCharacter::Respawn, 3.f, false);
}

void APlayerCharacter::Respawn()
{
	bIsDead = false;

	// 找 PlayerStart（TestMap 没有 PlayerStart → 原点兜底并落盘诊断）
	FVector SpawnLoc = FVector::ZeroVector;
	APlayerStart* Start = nullptr;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Start = *It;
		break;
	}
	if (Start)
	{
		SpawnLoc = Start->GetActorLocation();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] Respawn: 地图无 PlayerStart，重生到原点"));
	}

	SetActorLocation(SpawnLoc, false, nullptr, ETeleportType::ResetPhysics);
	SetActorEnableCollision(true);
	GetMesh()->SetVisibility(true, true);

	// 满血复活（Init* 不走 GE，不会触发 PostGameplayEffectExecute → 手动广播通知 UI）
	if (PlayerAttributeSet)
	{
		PlayerAttributeSet->InitHealth(InitialMaxHealth);
		PlayerAttributeSet->InitMaxHealth(InitialMaxHealth);
		PlayerAttributeSet->OnHealthChanged.Broadcast(PlayerAttributeSet->GetHealth(), PlayerAttributeSet->GetMaxHealth());
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] Respawn: 玩家重生在 %s"), *SpawnLoc.ToString());
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("已重生"));
	}
}
