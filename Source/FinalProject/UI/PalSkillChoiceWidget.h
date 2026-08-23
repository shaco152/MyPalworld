#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PalSkillChoiceWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillChoiceClicked, FName, SkillRowName);

/**
 * 可学技能候选按钮（WBP_PalSkillChoice 父类）：
 * 纯展示 + 行名广播；已装备标记与禁用态由调用方下发，本类不读取 Storage/Pal/DataTable。
 */
UCLASS()
class FINALPROJECT_API UPalSkillChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 设置展示：行名主键、显示文本、已装备标记（显示且禁用）、可用性
	void SetChoice(FName InRowName, const FText& InLabel, bool bEquipped, bool bEnabled);

	// 点击广播技能行名（管理面板据此记录 PendingRowName）
	UPROPERTY(BlueprintAssignable, Category = "PalSkill")
	FOnSkillChoiceClicked OnSkillChoiceClicked;

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EquippedMarker;

	FName RowName = NAME_None;
};
