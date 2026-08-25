#include "UI/MaterialInventoryWidget.h"

#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Items/ItemInventoryComponent.h"
#include "UI/MaterialSlotWidget.h"

void UMaterialInventoryWidget::InitFromInventory(UItemInventoryComponent* InInventory)
{
	Inventory = InInventory;
	BindInventory();
	Refresh();
}

void UMaterialInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindInventory();
	Refresh();
}

void UMaterialInventoryWidget::NativeDestruct()
{
	UnbindInventory();
	MaterialSlots.Empty();
	Super::NativeDestruct();
}

void UMaterialInventoryWidget::BindInventory()
{
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UMaterialInventoryWidget::Refresh);
		Inventory->OnInventoryChanged.AddDynamic(this, &UMaterialInventoryWidget::Refresh);
	}
}

void UMaterialInventoryWidget::UnbindInventory()
{
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UMaterialInventoryWidget::Refresh);
	}
}

void UMaterialInventoryWidget::Refresh()
{
	MaterialSlots.Reset();
	if (MaterialGrid)
	{
		MaterialGrid->ClearChildren();
	}
	if (!Inventory || !MaterialGrid || !SlotWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] MaterialInventoryWidget 配置缺失: Inventory=%s Grid=%s SlotClass=%s"),
			*GetNameSafe(Inventory), *GetNameSafe(MaterialGrid), *GetNameSafe(SlotWidgetClass));
		return;
	}

	int32 MaterialStackCount = 0;
	for (const FItemStack& Stack : Inventory->GetStacks())
	{
		FItemDefinitionRow Definition;
		if (!Stack.IsValid() || !Inventory->GetItemDefinition(Stack.ItemId, Definition) || Definition.Category != EItemCategory::Material)
		{
			continue;
		}

		if (UMaterialSlotWidget* MaterialSlot = CreateWidget<UMaterialSlotWidget>(GetOwningPlayer(), SlotWidgetClass))
		{
			MaterialSlot->SetStackData(Stack, Definition);
			MaterialSlots.Add(MaterialSlot);
			MaterialGrid->AddChildToWrapBox(MaterialSlot);
			++MaterialStackCount;
		}
	}

	if (CapacityText)
	{
		CapacityText->SetText(FText::FromString(FString::Printf(TEXT("堆叠槽 %d / %d"),
			Inventory->GetStacks().Num(), Inventory->GetStackCapacity())));
	}
	if (EmptyText)
	{
		EmptyText->SetVisibility(MaterialStackCount == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] MaterialInventoryWidget 刷新: 材料堆叠=%d"), MaterialStackCount);
}
