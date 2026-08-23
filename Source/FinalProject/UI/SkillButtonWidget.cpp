#include "SkillButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

USkillButtonWidget::USkillButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USkillButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button)
	{
		Button->OnClicked.RemoveDynamic(this, &USkillButtonWidget::HandleClicked);
		Button->OnClicked.AddDynamic(this, &USkillButtonWidget::HandleClicked);
	}
}

void USkillButtonWidget::Configure(int32 InSlotIndex)
{
	SlotIndex = InSlotIndex;
}

void USkillButtonWidget::SetPresentation(const FText& InLabel, bool bEnabled)
{
	if (Label)
	{
		Label->SetText(InLabel);
	}
	if (Button)
	{
		Button->SetIsEnabled(bEnabled);
	}
}

void USkillButtonWidget::HandleClicked()
{
	if (SlotIndex >= 0)
	{
		OnSkillClicked.Broadcast(SlotIndex);
	}
}
