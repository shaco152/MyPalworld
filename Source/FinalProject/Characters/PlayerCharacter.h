#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "PlayerCharacter.generated.h"

class UAbilitySystemComponent;
class USpringArmComponent;
class UCameraComponent;
class UPalStorageComponent;
class UItemInventoryComponent;
class UBuildingComponent;
class UPlayerAttributeSet;
class UTurnBattleComponent;

/**
 * 玩家角色：持有 ASC 并授予投掷能力，第三人称相机（弹簧臂 + 相机）。
 * 输入绑定在 APalPlayerController（EnhancedInput 挂点），按键后转发到本角色的 ASC 激活能力。
 */
UCLASS()
class FINALPROJECT_API APlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APlayerCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 帕鲁存储组件（背包 5 槽 + 仓库），捕捉入库 / 召唤都走它
	UPalStorageComponent* GetStorageComponent() const;

	// 通用可堆叠物品背包（当前材料拾取/建造消耗使用）
	UItemInventoryComponent* GetItemInventoryComponent() const;

	// 建造目录、虚影、旋转、校验与放置状态机
	UBuildingComponent* GetBuildingComponent() const { return BuildingComponent; }

	// 属性集访问器（战斗系统读玩家血量用；属性集不是 ActorComponent，不能用 FindComponentByClass）
	UPlayerAttributeSet* GetAttributeSet() const { return PlayerAttributeSet; }

	// 初始属性配置（普通 float，BP 类默认值可编辑；InitAbilitySystem 时写入属性集）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|InitialAttributes")
	float InitialHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|InitialAttributes")
	float InitialMaxHealth = 100.f;

	// 死亡处理（自由战斗被野帕鲁打死 / 回合制战败）：3 秒后在 PlayerStart 重生
	void HandleDeath();
	bool IsDead() const { return bIsDead; }
	void PlayAttackMontageReplicated(class UAnimMontage* Montage);

	// 授予的攻击能力类（鼠标左键普攻，默认 C++ 类）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TSubclassOf<UGameplayAbility> AttackAbilityClass;

	// 回合制战斗组件（P 键进入；冻结输入的判定）
	UTurnBattleComponent* GetTurnBattleComponent() const { return TurnBattle; }
	bool IsInTurnBattle() const;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UPlayerAttributeSet> PlayerAttributeSet;

	// 回合制战斗（P 键）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UTurnBattleComponent> TurnBattle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	TObjectPtr<UBuildingComponent> BuildingComponent;

	// 第三人称相机：弹簧臂跟随 PawnControlRotation，角色偏航跟随控制器
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// 授予的投掷能力类。默认用 C++ 类（生成纯 C++ 球）；
	// 若要生成 BP_CaptureBall，新建 BP 子类能力并在其中设 BallClass = BP_CaptureBall，再在这里指定它。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TSubclassOf<UGameplayAbility> ThrowAbilityClass;

private:
	UFUNCTION()
	void OnRep_IsDead();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayAttackMontage(UAnimMontage* Montage);

	void ApplyDeathPresentation();

	// 3 秒重生计时器
	void Respawn();

	// 授予能力（幂等：已存在同类型则跳过），并给 Spec 打输入标签
	void GrantAbilityIfMissing(TSubclassOf<UGameplayAbility> AbilityClass, const FGameplayTag& InputTag);

	void InitAbilitySystem();

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;
	FTimerHandle RespawnTimer;
};
