#include "UI/BuildMenuWidget.h"

#include "Building/BuildingComponent.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Items/ItemInventoryComponent.h"
#include "UI/BuildRecipeEntryWidget.h"

void UBuildMenuWidget::InitFromBuildingComponent(UBuildingComponent* InBuildingComponent)
{
	BuildingComponent = InBuildingComponent;
	BindInventory();
}

void UBuildMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindInventory();
	RebuildEntries();
}

void UBuildMenuWidget::NativeDestruct()
{
	UnbindInventory();
	Entries.Empty();
	Super::NativeDestruct();
}

void UBuildMenuWidget::BindInventory()
{
	if (UItemInventoryComponent* Inventory = BuildingComponent ? BuildingComponent->GetInventory() : nullptr)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UBuildMenuWidget::RefreshAffordability);
		Inventory->OnInventoryChanged.AddDynamic(this, &UBuildMenuWidget::RefreshAffordability);
	}
}

void UBuildMenuWidget::UnbindInventory()
{
	if (UItemInventoryComponent* Inventory = BuildingComponent ? BuildingComponent->GetInventory() : nullptr)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UBuildMenuWidget::RefreshAffordability);
	}
}

void UBuildMenuWidget::RebuildEntries()
{
	Entries.Reset();
	if (RecipeGrid)
	{
		RecipeGrid->ClearChildren();
	}
	if (!BuildingComponent || !RecipeGrid || !EntryWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] BuildMenuWidget 配置缺失: Component=%s Grid=%s EntryClass=%s"),
			*GetNameSafe(BuildingComponent), *GetNameSafe(RecipeGrid), *GetNameSafe(EntryWidgetClass));
		return;
	}

	for (const FName BuildingId : BuildingComponent->GetRecipeIds())
	{
		const FBuildingRecipeRow* Recipe = BuildingComponent->FindRecipe(BuildingId);
		if (!Recipe)
		{
			continue;
		}
		if (UBuildRecipeEntryWidget* Entry = CreateWidget<UBuildRecipeEntryWidget>(GetOwningPlayer(), EntryWidgetClass))
		{
			Entry->InitRecipe(this, BuildingId, *Recipe);
			Entries.Add(Entry);
			RecipeGrid->AddChildToWrapBox(Entry);
		}
	}

	if (EmptyCatalogText)
	{
		EmptyCatalogText->SetVisibility(Entries.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] BuildMenuWidget 建筑条目=%d"), Entries.Num());
}

void UBuildMenuWidget::RefreshAffordability()
{
	for (UBuildRecipeEntryWidget* Entry : Entries)
	{
		if (Entry)
		{
			Entry->RefreshAffordability();
		}
	}
}

void UBuildMenuWidget::HandleRecipeClicked(FName BuildingId)
{
	if (BuildingComponent)
	{
		BuildingComponent->SelectBuilding(BuildingId);
	}
}
