#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_ThrowPalSphere.generated.h"

class ACaptureBall;

/**
 * 投掷帕鲁球能力：挂 InputTag.Throw 标签，玩家按键后
 * TryActivateAbilitiesByTag 激活，沿瞄准方向生成 ACaptureBall。
 */
UCLASS()
class FINALPROJECT_API UGA_ThrowPalSphere : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_ThrowPalSphere();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// 生成的球类（默认 C++ 类，蓝图中可换成 BP_CaptureBall）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw")
	TSubclassOf<ACaptureBall> BallClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw")
	float ThrowSpeed = 2500.f;
};
