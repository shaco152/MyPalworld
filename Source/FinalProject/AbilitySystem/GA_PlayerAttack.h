#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerAttack.generated.h"

class UAnimMontage;

/**
 * 玩家普攻能力（鼠标左键，InputTag.Attack）：
 * 从摄像机视角做球形扫描：Pawn 走 UCombatLibrary::ApplyDamage，资源 Actor 走 HitReact 掉落。
 * 冷却用能力实例内时间戳（简单可靠，不引入冷却 GE）。
 * 动画：AttackMontage 在攻击瞬间播放（BP 子类里设置资产；为空则无动画）。
 */
UCLASS()
class FINALPROJECT_API UGA_PlayerAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PlayerAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// 攻击动画蒙太奇（BP 子类设置；为空不播放。玩家蓝图的 AnimInstance 需支持蒙太奇槽）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	// 单次攻击伤害
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	float AttackPower = 20.f;

	// 攻击距离（cm）与球形扫描半径（近似近战判定）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	float AttackRange = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	float AttackRadius = 60.f;

	// 攻击冷却（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	float CooldownDuration = 0.8f;

private:
	// 上次攻击时刻（能力实例字段，InstancedPerActor 默认每主人一份实例）
	double LastAttackTime = 0.0;
};
