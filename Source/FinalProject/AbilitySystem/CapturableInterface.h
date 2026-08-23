#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CapturableInterface.generated.h"

class ACaptureBall;

UINTERFACE(MinimalAPI, BlueprintType)
class UCapturableInterface : public UInterface
{
	GENERATED_BODY()
};

/** 可被帕鲁球捕捉的对象需要实现的接口（由 APalCharacter 实现） */
class FINALPROJECT_API ICapturableInterface
{
	GENERATED_BODY()

public:
	// 查询当前捕捉概率（0~1）。
	// 实现方式：应用 UGE_CaptureChance（ExecCalc 读血量/等级）→ 读 CaptureChance 属性。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Capture")
	float GetCaptureChance() const;

	// 进入捕捉状态：加 BeingCaptured 标签、隐藏网格、关闭碰撞、停止移动
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Capture")
	void BeginCapture(ACaptureBall* Ball, const FVector& HitLocation);

	// 应用捕捉结果：失败 → 回到命中点恢复；成功 → 打印日志并销毁
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Capture")
	void ResolveCapture(bool bSuccess, const FVector& HitLocation);
};
