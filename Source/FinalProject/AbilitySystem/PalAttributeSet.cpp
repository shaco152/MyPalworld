#include "PalAttributeSet.h"
#include "GameplayEffectExtension.h" // FGameplayEffectModCallbackData 完整类型（定义在此处，不在 GameplayEffect.h）

UPalAttributeSet::UPalAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitLevel(1.f);
	InitCaptureChance(0.f);
	InitMP(50.f);
	InitMaxMP(50.f);
}

void UPalAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 伤害/治疗/耗蓝 GE 结算后钳制（回合制直接 Set 的场景由调用方自行钳制）
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		// 事件驱动：血量变化广播（头顶血条等订阅，禁止 Tick 轮询）
		OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
	}
	else if (Data.EvaluatedData.Attribute == GetMPAttribute())
	{
		SetMP(FMath::Clamp(GetMP(), 0.f, GetMaxMP()));
	}
}
