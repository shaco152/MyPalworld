#include "GE_CaptureChance.h"
#include "CaptureChanceExecCalc.h"

UGE_CaptureChance::UGE_CaptureChance()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 5.4：GE 通过 FGameplayEffectExecutionDefinition 引用 ExecCalc（不再是旧版 FCustomCalculationBasedMagnitude）
	FGameplayEffectExecutionDefinition ExecDefinition;
	ExecDefinition.CalculationClass = UCaptureChanceExecCalc::StaticClass();
	Executions.Add(ExecDefinition);
}
