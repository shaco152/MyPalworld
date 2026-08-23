#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "CaptureChanceExecCalc.generated.h"

/**
 * 捕捉概率计算：
 *   Chance = Clamp(BaseChance + (1 - HP/MaxHP) * HPBonus - 等级惩罚, 0, MaxChance)
 * 由 UGE_CaptureChance（即时 GE）触发执行，结果覆写到 UPalAttributeSet::CaptureChance。
 */
UCLASS()
class FINALPROJECT_API UCaptureChanceExecCalc : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UCaptureChanceExecCalc();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

	// 概率公式（回合制捕捉投球行动复用同一公式）：
	//   Chance = Clamp(BaseChance + (1 - HP/MaxHP) * HPBonus - 等级惩罚, 0, MaxChance)
	static float CalculateChance(float Health, float MaxHealth, float Level);

	// 可调概率参数
	static constexpr float BaseChance = 0.3f;    // 满血时的基础概率
	static constexpr float HPBonus = 0.85f;      // 残血加成上限（血量越少概率越高）
	static constexpr float MaxChance = 0.95f;    // 概率上限
	static constexpr float LevelPenalty = 0.02f; // 等级每高于 1 级的惩罚
};
