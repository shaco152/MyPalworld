#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MaterialInventoryWidget.generated.h"

class UItemInventoryComponent;
class UMaterialSlotWidget;
class UTextBlock;
class UWrapBox;

/** B 键材料背包根面板；动态数量仅实例化用户提供的 WBP_MaterialSlot 模板。 */
UCLASS()
class FINALPROJECT_API UMaterialInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitFromInventory(UItemInventoryComponent* InInventory);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> MaterialGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CapacityText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmptyText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MaterialInventory")
	TSubclassOf<UMaterialSlotWidget> SlotWidgetClass;

	UFUNCTION()
	void Refresh();

private:
	void BindInventory();
	void UnbindInventory();

	UPROPERTY()
	TObjectPtr<UItemInventoryComponent> Inventory;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialSlotWidget>> MaterialSlots;
};
