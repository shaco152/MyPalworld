#include "PalSkillChoiceWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UPalSkillChoiceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button)
	{
		Button->OnClicked.RemoveDynamic(this, &UPalSkillChoiceWidget::HandleClicked);
		Button->OnClicked.AddDynamic(this, &UPalSkillChoiceWidget::HandleClicked);
	}
}

void UPalSkillChoiceWidget::SetChoice(FName InRowName, const FText& InLabel, bool bEquipped, bool bEnabled)
{
	RowName = InRowName;
	if (Label)
	{
		Label->SetText(InLabel);
	}
	if (EquippedMarker)
	{
		// 已装备标记：C++ 只切换可见性，静态文字由 WBP 设计器设置
		EquippedMarker->SetVisibility(bEquipped ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Button)
	{
		Button->SetIsEnabled(bEnabled);
	}
}

void UPalSkillChoiceWidget::HandleClicked()
{
	if (!RowName.IsNone())
	{
		OnSkillChoiceClicked.Broadcast(RowName);
	}
}
