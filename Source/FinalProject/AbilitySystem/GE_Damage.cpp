#include "GE_Damage.h"
#include "PalAttributeSet.h"

UGE_Damage::UGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 5.4：FSetByCallerFloat 只有默认构造 + 成员赋值；Magnitude 类型是 FGameplayEffectModifierMagnitude
	FSetByCallerFloat DamageMagnitude;
	DamageMagnitude.DataName = TEXT("Damage");

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UPalAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageMagnitude);
	Modifiers.Add(Modifier);
}
