#include "UI/BuildRecipeEntryWidget.h"

#include "Building/BuildingComponent.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Items/ItemInventoryComponent.h"
#include "UI/BuildMenuWidget.h"

void UBuildRecipeEntryWidget::InitRecipe(UBuildMenuWidget* InOwnerMenu, FName InBuildingId, const FBuildingRecipeRow& InRecipe)
{
	OwnerMenu = InOwnerMenu;
	BuildingId = InBuildingId;
	Recipe = InRecipe;
	RefreshAffordability();
}

void UBuildRecipeEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &UBuildRecipeEntryWidget::HandleClicked);
		SelectButton->OnClicked.AddDynamic(this, &UBuildRecipeEntryWidget::HandleClicked);
	}
	if (BuildingIcon)
	{
		BuildingIcon->SetBrushFromTexture(Recipe.Icon);
	}
	if (BuildingNameText)
	{
		BuildingNameText->SetText(Recipe.DisplayName.IsEmpty() ? FText::FromName(BuildingId) : Recipe.DisplayName);
	}
	RefreshAffordability();
}

void UBuildRecipeEntryWidget::NativeDestruct()
{
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &UBuildRecipeEntryWidget::HandleClicked);
	}
	Super::NativeDestruct();
}

FString UBuildRecipeEntryWidget::BuildCostString() const
{
	const UBuildingComponent* Building = OwnerMenu ? OwnerMenu->GetBuildingComponent() : nullptr;
	const UItemInventoryComponent* Inventory = Building ? Building->GetInventory() : nullptr;
	TArray<FString> Parts;
	for (const FItemAmount& Cost : Recipe.MaterialCosts)
	{
		if (!Cost.IsValid())
		{
			continue;
		}
		FString ItemName = Cost.ItemId.ToString();
		int32 Owned = 0;
		if (Inventory)
		{
			FItemDefinitionRow Definition;
			if (Inventory->GetItemDefinition(Cost.ItemId, Definition) && !Definition.DisplayName.IsEmpty())
			{
				ItemName = Definition.DisplayName.ToString();
			}
			Owned = Inventory->GetTotalQuantity(Cost.ItemId);
		}
		Parts.Add(FString::Printf(TEXT("%s %d/%d"), *ItemName, Owned, Cost.Quantity));
	}
	return Parts.Num() > 0 ? FString::Join(Parts, TEXT("  ")) : TEXT("免费");
}

void UBuildRecipeEntryWidget::RefreshAffordability()
{
	const UBuildingComponent* Building = OwnerMenu ? OwnerMenu->GetBuildingComponent() : nullptr;
	const UItemInventoryComponent* Inventory = Building ? Building->GetInventory() : nullptr;
	const bool bAffordable = Inventory && Inventory->HasItems(Recipe.MaterialCosts);

	if (CostText)
	{
		CostText->SetText(FText::FromString(BuildCostString()));
	}
	if (AvailabilityText)
	{
		AvailabilityText->SetText(FText::FromString(bAffordable ? TEXT("可建造") : TEXT("材料不足")));
		AvailabilityText->SetColorAndOpacity(bAffordable ? FSlateColor(FLinearColor::Green) : FSlateColor(FLinearColor::Red));
	}
	if (SelectButton)
	{
		SelectButton->SetIsEnabled(bAffordable && Recipe.BuildingClass != nullptr);
	}
}

void UBuildRecipeEntryWidget::HandleClicked()
{
	if (OwnerMenu)
	{
		OwnerMenu->HandleRecipeClicked(BuildingId);
	}
}
