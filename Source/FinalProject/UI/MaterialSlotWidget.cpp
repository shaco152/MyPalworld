#include "UI/MaterialSlotWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UMaterialSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

void UMaterialSlotWidget::SetStackData(const FItemStack& InStack, const FItemDefinitionRow& InDefinition)
{
	Stack = InStack;
	Definition = InDefinition;
	Refresh();
}

void UMaterialSlotWidget::Refresh()
{
	if (ItemIcon)
	{
		ItemIcon->SetBrushFromTexture(Definition.Icon);
		ItemIcon->SetVisibility(Definition.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (ItemNameText)
	{
		ItemNameText->SetText(Definition.DisplayName.IsEmpty() ? FText::FromName(Stack.ItemId) : Definition.DisplayName);
	}
	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(FMath::Max(0, Stack.Quantity)));
	}
}
