#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CaptureWidget.generated.h"

class ACaptureBall;
class UTextBlock;
class URadialSlider;

/**
 * 帕鲁球上挂载的捕捉控件：概率文字 + 环形进度条。
 * 绑定 ACaptureBall 的 OnCaptureStarted 委托，概率值用 lerp 插值平滑填充。
 */
UCLASS()
class FINALPROJECT_API UCaptureWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由 ACaptureBall 创建控件后调用，绑定球上的捕捉委托
	void InitFromBall(ACaptureBall* InBall);

	// 由球的 Tick 每帧驱动：概率显示值 lerp 逼近目标（不依赖 UserWidget 自身 Tick，嵌在 WidgetComponent 里时它不可靠）
	void TickFill(float DeltaSeconds);

	// 设置新的目标概率（第一次判定通过后球会调用，环继续向新值填充）
	void SetCaptureChance(float Chance);

	// 捕捉成功：目标值设为 100%，环继续平滑填满（球等填满后才销毁）
	void ShowSuccessFill();

	// 环是否已填到 100%（球据此延迟销毁）
	bool IsFillComplete() const { return DisplayedChance >= 0.995f; }

protected:
	// BindWidget 命名必须与蓝图控件树里的名字一致
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ChanceText;

	// 环形进度条：0~1，1 为满环（蓝图里配置 Locked、隐藏滑块手柄、360° 起止角）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URadialSlider> ChanceRing;

	// 平滑填充速度（FInterpTo 插值系数，越大越快；4 ≈ 约 1 秒填到接近目标）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture")
	float FillSpeed = 4.f;

private:
	UFUNCTION()
	void HandleCaptureStarted(float Chance);

	void UpdateDisplay();

	// 目标概率（球广播）与当前显示值（每帧 lerp 逼近目标）
	float TargetChance = 0.f;
	float DisplayedChance = 0.f;
};
