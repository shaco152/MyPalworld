#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "PlayerAttributeSet.generated.h"

#define PLAYER_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerHealthChanged, float, Health, float, MaxHealth);

/** 玩家属性集：血量（自由战斗中被敌意野帕鲁伤害的对象） */
UCLASS()
class FINALPROJECT_API UPlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPlayerAttributeSet();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 血量变化广播（受伤/治疗 GE 结算后触发；UI 订阅更新——禁止 Tick 轮询）
	UPROPERTY(BlueprintAssignable, Category = "Player|Attributes")
	FOnPlayerHealthChanged OnHealthChanged;

	// 数值配置走 APlayerCharacter 的 InitialHealth/InitialMaxHealth 普通 float（同帕鲁模式）
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Player|Attributes")
	FGameplayAttributeData Health;
	PLAYER_ATTRIBUTE_ACCESSORS(UPlayerAttributeSet, Health);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Player|Attributes")
	FGameplayAttributeData MaxHealth;
	PLAYER_ATTRIBUTE_ACCESSORS(UPlayerAttributeSet, MaxHealth);

	// 伤害 GE 结算后血量钳制到 [0, MaxHealth]
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
