#include "PalAttributeSet.h"
#include "GameplayEffectExtension.h" // FGameplayEffectModCallbackData 完整类型（定义在此处，不在 GameplayEffect.h）
#include "Net/UnrealNetwork.h"

UPalAttributeSet::UPalAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitLevel(1.f);
	InitCaptureChance(0.f);
	InitMP(50.f);
	InitMaxMP(50.f);
}

void UPalAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UPalAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPalAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPalAttributeSet, Level, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPalAttributeSet, CaptureChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPalAttributeSet, MP, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPalAttributeSet, MaxMP, COND_None, REPNOTIFY_Always);
}

void UPalAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPalAttributeSet, Health, OldValue);
	OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
}

void UPalAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPalAttributeSet, MaxHealth, OldValue);
	OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth());
}

void UPalAttributeSet::OnRep_Level(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPalAttributeSet, Level, OldValue);
}

void UPalAttributeSet::OnRep_CaptureChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPalAttributeSet, CaptureChance, OldValue);
}

void UPalAttributeSet::OnRep_MP(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPalAttributeSet, MP, OldValue);
}

void UPalAttributeSet::OnRep_MaxMP(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPalAttributeSet, MaxMP, OldValue);
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
