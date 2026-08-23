#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Damage.generated.h"

/**
 * 帕鲁伤害即时 GE：Modifiers 对 UPalAttributeSet::Health 做 Additive，
 * 数值从 Spec 的 SetByCaller.Damage 标签读取（UCombatLibrary::ApplyDamage 写入）。
 * 结算后由 UPalAttributeSet::PostGameplayEffectExecute 钳制到 [0, MaxHealth]。
 */
UCLASS()
class FINALPROJECT_API UGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Damage();
};
