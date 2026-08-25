#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Building/BuildingData.h"
#include "BuildRecipeEntryWidget.generated.h"

class UBuildMenuWidget;
class UButton;
class UImage;
class UTextBlock;

/** WBP_BuildRecipeEntry 原子控件：一张可点击建筑图案 + 名称 + 材料成本。 */
UCLASS()
class FINALPROJECT_API UBuildRecipeEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitRecipe(UBuildMenuWidget* InOwnerMenu, FName InBuildingId, const FBuildingRecipeRow& InRecipe);
	void RefreshAffordability();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> BuildingIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BuildingNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AvailabilityText;

	UFUNCTION()
	void HandleClicked();

private:
	FString BuildCostString() const;

	UPROPERTY()
	TObjectPtr<UBuildMenuWidget> OwnerMenu;

	FName BuildingId = NAME_None;
	FBuildingRecipeRow Recipe;
};
