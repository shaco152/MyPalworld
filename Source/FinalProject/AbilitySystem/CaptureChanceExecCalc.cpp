#include "CaptureChanceExecCalc.h"
#include "PalAttributeSet.h"
#include "GameplayEffectTypes.h"

// 静态捕获定义：构造时缓存 FProperty 指针（ExecCalc 标准写法）
struct FCaptureChanceStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(Health);
	DECLARE_ATTRIBUTE_CAPTUREDEF(MaxHealth);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Level);

	FCaptureChanceStatics()
	{
		// 5.4 签名：DEFINE_ATTRIBUTE_CAPTUREDEF(属性集, 属性, 捕获来源, bSnapshot)
		DEFINE_ATTRIBUTE_CAPTUREDEF(UPalAttributeSet, Health, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UPalAttributeSet, MaxHealth, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UPalAttributeSet, Level, Target, false);
	}
};

static const FCaptureChanceStatics& CaptureChanceStatics()
{
	static FCaptureChanceStatics Statics;
	return Statics;
}

UCaptureChanceExecCalc::UCaptureChanceExecCalc()
{
	RelevantAttributesToCapture.Add(CaptureChanceStatics().HealthDef);
	RelevantAttributesToCapture.Add(CaptureChanceStatics().MaxHealthDef);
	RelevantAttributesToCapture.Add(CaptureChanceStatics().LevelDef);
}

void UCaptureChanceExecCalc::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluationParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float Health = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(CaptureChanceStatics().HealthDef, EvaluationParameters, Health);
	float MaxHealth = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(CaptureChanceStatics().MaxHealthDef, EvaluationParameters, MaxHealth);
	float Level = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(CaptureChanceStatics().LevelDef, EvaluationParameters, Level);

	// 覆写 CaptureChance 属性
	const float Chance = CalculateChance(Health, MaxHealth, Level);
	OutExecutionOutput.AddOutputModifier(
		FGameplayModifierEvaluatedData(UPalAttributeSet::GetCaptureChanceAttribute(), EGameplayModOp::Override, Chance));
}

float UCaptureChanceExecCalc::CalculateChance(float Health, float MaxHealth, float Level)
{
	// Chance = 基础概率 + 残血比例加成 - 等级惩罚
	const float HPRatio = (MaxHealth > 0.f) ? (Health / MaxHealth) : 1.f;
	float Chance = BaseChance + (1.f - HPRatio) * HPBonus;
	Chance -= FMath::Max(0.f, Level - 1.f) * LevelPenalty;
	return FMath::Clamp(Chance, 0.f, MaxChance);
}
