#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/ItemData.h"
#include "MaterialSlotWidget.generated.h"

class UImage;
class UTextBlock;

/** WBP_MaterialSlot 原子控件：样式完全由设计器负责，C++ 只写图标/名称/堆叠数。 */
UCLASS()
class FINALPROJECT_API UMaterialSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStackData(const FItemStack& InStack, const FItemDefinitionRow& InDefinition);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> QuantityText;

private:
	void Refresh();

	FItemStack Stack;
	FItemDefinitionRow Definition;
};
