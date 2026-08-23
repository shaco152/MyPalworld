#include "PlayerAttributeSet.h"
#include "GameplayEffectExtension.h" // FGameplayEffectModCallbackData 完整类型（定义在此处，不在 GameplayEffect.h）

UPlayerAttributeSet::UPlayerAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
}

void UPlayerAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		// 事件驱动：受伤/治疗结算后广播，UI 订阅刷新
		OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
	}
}
