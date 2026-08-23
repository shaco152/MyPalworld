#pragma once

#include "CoreMinimal.h"
#include "Combat/PalSkillData.h"
#include "PalSkillExecutor.generated.h"

class APalCharacter;

/** 单次同步技能执行的值语义上下文；执行器 CDO 不得缓存其中任何字段。 */
USTRUCT()
struct FINALPROJECT_API FPalSkillContext
{
	GENERATED_BODY()

	APalCharacter* Source = nullptr;
	AActor* Target = nullptr;
	FName SkillRowName = NAME_None;
	FPalSkillRow Skill;
};

/** 技能执行结果：调用方用 bKilled 保留现有回合制退场/胜负分支。 */
USTRUCT(BlueprintType)
struct FINALPROJECT_API FPalSkillExecutionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	float Damage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	bool bApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	bool bKilled = false;
};

/**
 * 帕鲁技能执行器基类：无状态 CDO 同步直调。
 * 当前 ExecuteImmediate 依次计算、播放演出并立即结算；三阶段虚函数为未来 AnimNotify 命中帧保留接缝。
 */
UCLASS(Abstract, BlueprintType)
class FINALPROJECT_API UPalSkillExecutor : public UObject
{
	GENERATED_BODY()

public:
	// 三个调用点唯一允许使用的解析入口；空类统一回退到直接伤害执行器
	static const UPalSkillExecutor* GetExecutorCDO(const FPalSkillRow& Skill);

	// 外部唯一执行入口；CDO 全程 const，禁止保存运行时状态
	FPalSkillExecutionResult ExecuteImmediate(const FPalSkillContext& Context) const;

protected:
	virtual float CalculateDamage(const FPalSkillContext& Context) const;
	virtual void PlayPresentation(const FPalSkillContext& Context) const;
	virtual FPalSkillExecutionResult ApplyEffect(const FPalSkillContext& Context, float Damage) const;
};

/** ExecutorClass 为空时使用的具体默认执行器。 */
UCLASS()
class FINALPROJECT_API UPalSkillDirectDamageExecutor : public UPalSkillExecutor
{
	GENERATED_BODY()
};
