#include "UI/SaveSlotWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void USaveSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SelectButton->OnClicked.RemoveDynamic(this, &USaveSlotWidget::HandleClicked);
	SelectButton->OnClicked.AddDynamic(this, &USaveSlotWidget::HandleClicked);
}

void USaveSlotWidget::NativeDestruct()
{
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &USaveSlotWidget::HandleClicked);
	}
	Super::NativeDestruct();
}

void USaveSlotWidget::SetSaveMetadata(const FWorldSaveMetadata& InMetadata)
{
	Metadata = InMetadata;
	if (WorldNameText)
	{
		WorldNameText->SetText(FText::FromString(Metadata.DisplayName));
	}
	if (DetailsText)
	{
		const int64 Hours = Metadata.TotalPlaySeconds / 3600;
		const int64 Minutes = (Metadata.TotalPlaySeconds % 3600) / 60;
		DetailsText->SetText(FText::FromString(FString::Printf(TEXT("%s UTC  |  %lld小时%02lld分  |  R%lld"),
			*Metadata.SavedAtUtc.ToString(TEXT("yyyy-MM-dd HH:mm")), Hours, Minutes, Metadata.SaveRevision)));
	}
}

void USaveSlotWidget::HandleClicked()
{
	OnSelected.Broadcast(Metadata.WorldId);
}
