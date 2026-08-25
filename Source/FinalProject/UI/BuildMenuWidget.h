#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuildMenuWidget.generated.h"

class UBuildRecipeEntryWidget;
class UBuildingComponent;
class UTextBlock;
class UWrapBox;

/** C 键建筑清单根面板；动态数量只创建 WBP_BuildRecipeEntry 模板，不在 C++ 写样式/布局。 */
UCLASS()
class FINALPROJECT_API UBuildMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitFromBuildingComponent(UBuildingComponent* InBuildingComponent);
	void HandleRecipeClicked(FName BuildingId);
	UBuildingComponent* GetBuildingComponent() const { return BuildingComponent; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> RecipeGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmptyCatalogText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BuildMenu")
	TSubclassOf<UBuildRecipeEntryWidget> EntryWidgetClass;

	UFUNCTION()
	void RefreshAffordability();

private:
	void RebuildEntries();
	void BindInventory();
	void UnbindInventory();

	UPROPERTY()
	TObjectPtr<UBuildingComponent> BuildingComponent;

	UPROPERTY()
	TArray<TObjectPtr<UBuildRecipeEntryWidget>> Entries;
};
