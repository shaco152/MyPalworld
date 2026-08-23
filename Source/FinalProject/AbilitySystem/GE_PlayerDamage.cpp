#include "GE_PlayerDamage.h"
#include "PlayerAttributeSet.h"

UGE_PlayerDamage::UGE_PlayerDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 5.4：FSetByCallerFloat 只有默认构造 + 成员赋值；Magnitude 类型是 FGameplayEffectModifierMagnitude
	FSetByCallerFloat DamageMagnitude;
	DamageMagnitude.DataName = TEXT("Damage");

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UPlayerAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageMagnitude);
	Modifiers.Add(Modifier);
}
