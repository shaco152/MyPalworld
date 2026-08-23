#include "PalHPBarWidget.h"

#include "Components/ProgressBar.h"

UPalHPBarWidget::UPalHPBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPalHPBarWidget::UpdateBar(float Percent)
{
	if (Bar)
	{
		Percent = FMath::Clamp(Percent, 0.f, 1.f);
		Bar->SetPercent(Percent);

		// 已验收的数据驱动表现：高血量绿、中血量黄、低血量红；笔刷本身仍由 WBP 配置。
		const FLinearColor FillColor = Percent > 0.5f
			? FLinearColor(0.2f, 0.9f, 0.3f, 1.f)
			: Percent > 0.25f
				? FLinearColor(1.f, 0.85f, 0.2f, 1.f)
				: FLinearColor(0.9f, 0.2f, 0.15f, 1.f);
		Bar->SetFillColorAndOpacity(FillColor);
	}
}
