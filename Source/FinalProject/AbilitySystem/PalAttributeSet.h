#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "PalAttributeSet.generated.h"

// FGameplayAttributeData 的 Get/Set/Init 快捷宏
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPalHealthChanged, float, Health, float, MaxHealth);

/** 帕鲁属性集：血量 / 等级 / 捕捉概率 */
UCLASS()
class FINALPROJECT_API UPalAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPalAttributeSet();

	// 注意：这里标 EditAnywhere 也没用——FGameplayAttributeData 内部的 BaseValue/CurrentValue
	// 在引擎里是 protected + BlueprintReadOnly，详情面板不可编辑。数值配置走 APalCharacter 的
	// InitialHealth/InitialMaxHealth/InitialLevel 普通 float（InitAbilitySystem 时写入属性集）。
	UPROPERTY(BlueprintReadOnly, Category = "Pal|Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UPalAttributeSet, Health);

	UPROPERTY(BlueprintReadOnly, Category = "Pal|Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UPalAttributeSet, MaxHealth);

	UPROPERTY(BlueprintReadOnly, Category = "Pal|Attributes")
	FGameplayAttributeData Level;
	ATTRIBUTE_ACCESSORS(UPalAttributeSet, Level);

	// 当前捕捉概率（0~1），由 UGE_CaptureChance + UCaptureChanceExecCalc 计算后覆写
	UPROPERTY(BlueprintReadOnly, Category = "Pal|Attributes")
	FGameplayAttributeData CaptureChance;
	ATTRIBUTE_ACCESSORS(UPalAttributeSet, CaptureChance);

	// 血量变化广播（受伤/治疗 GE 结算后触发；头顶血条等 UI 订阅——禁止 Tick 轮询）
	UPROPERTY(BlueprintAssignable, Category = "Pal|Attributes")
	FOnPalHealthChanged OnHealthChanged;

	// MP（回合制技能消耗；自由战斗普攻不耗 MP）
	UPROPERTY(BlueprintReadOnly, Category = "Pal|Attributes")
	FGameplayAttributeData MP;
	ATTRIBUTE_ACCESSORS(UPalAttributeSet, MP);

	UPROPERTY(BlueprintReadOnly, Category = "Pal|Attributes")
	FGameplayAttributeData MaxMP;
	ATTRIBUTE_ACCESSORS(UPalAttributeSet, MaxMP);

	// 伤害/治疗/耗蓝 GE 结算后钳制（HP→[0,MaxHealth]，MP→[0,MaxMP]）
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
