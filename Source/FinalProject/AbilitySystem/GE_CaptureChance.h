#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_CaptureChance.generated.h"

/**
 * 即时 GE：每次应用时执行 UCaptureChanceExecCalc，
 * 把计算出的捕捉概率覆写到 UPalAttributeSet::CaptureChance。
 * 帕鲁查询概率 = ApplyGameplayEffectSpecToSelf(this GE) + 读属性。
 */
UCLASS()
class FINALPROJECT_API UGE_CaptureChance : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_CaptureChance();
};
