#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_PlayerDamage.generated.h"

/**
 * 玩家伤害即时 GE：Modifiers 对 UPlayerAttributeSet::Health 做 Additive，
 * 数值从 Spec 的 SetByCaller.Damage 标签读取（UCombatLibrary::ApplyDamage 写入）。
 */
UCLASS()
class FINALPROJECT_API UGE_PlayerDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_PlayerDamage();
};
