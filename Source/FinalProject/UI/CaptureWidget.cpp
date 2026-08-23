#include "CaptureWidget.h"
#include "Components/TextBlock.h"
#include "Components/RadialSlider.h"
#include "Actors/CaptureBall.h"

void UCaptureWidget::InitFromBall(ACaptureBall* InBall)
{
	if (!InBall)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] CaptureWidget::InitFromBall: 球为空！"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] CaptureWidget::InitFromBall: 绑定成功, 球=%s, 本控件=%s, ChanceText=%s, ChanceRing=%s"),
		*InBall->GetName(), *GetName(), *GetNameSafe(ChanceText), *GetNameSafe(ChanceRing));

	InBall->OnCaptureStarted.AddDynamic(this, &UCaptureWidget::HandleCaptureStarted);
}

void UCaptureWidget::TickFill(float DeltaSeconds)
{
	// 概率平滑插值：显示值逐帧 lerp 逼近目标值（由球的 Tick 驱动）
	if (!FMath::IsNearlyEqual(DisplayedChance, TargetChance, 0.001f))
	{
		DisplayedChance = FMath::FInterpTo(DisplayedChance, TargetChance, DeltaSeconds, FillSpeed);
		UpdateDisplay();
	}
}

void UCaptureWidget::HandleCaptureStarted(float Chance)
{
	SetCaptureChance(Chance);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] CaptureWidget::HandleCaptureStarted: 概率=%.2f, 文本=%s"), Chance, ChanceText ? *ChanceText->GetText().ToString() : TEXT("无"));
}

void UCaptureWidget::SetCaptureChance(float Chance)
{
	TargetChance = FMath::Clamp(Chance, 0.f, 1.f);
	UpdateDisplay(); // 立即刷新一次，随后 TickFill 平滑填充
}

void UCaptureWidget::ShowSuccessFill()
{
	TargetChance = 1.f; // 成功：环继续从当前值平滑填到 100%
	UE_LOG(LogTemp, Warning, TEXT("[诊断] CaptureWidget::ShowSuccessFill: 开始填充到 100%%"));
}

void UCaptureWidget::UpdateDisplay()
{
	if (ChanceText)
	{
		ChanceText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(DisplayedChance * 100.f))));
	}
	if (ChanceRing)
	{
		ChanceRing->SetValue(DisplayedChance);
	}
}
