#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/PalSkillChoiceWidget.h" // TSubclassOf<UPalSkillChoiceWidget> 的 UPROPERTY 反射需要完整类型
#include "PalSkillManagementWidget.generated.h"

class USkillButtonWidget;
class UTextBlock;
class UWrapBox;
class UPalStorageComponent;

/**
 * 技能管理面板（WBP_PalSkillManagement 父类）：
 * 显示锁定背包帕鲁的 4 个技能槽与可学池，装配经 Storage::SetPalSkill。
 * 不单独绑定 OnStorageChanged——由父 UPalBoxWidget::Refresh 级联，避免重复观察者。
 */
UCLASS()
class FINALPROJECT_API UPalSkillManagementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 保存存储引用（幂等，可重复调用）
	void InitFromStorage(UPalStorageComponent* InStorage);

	// 设置管理目标（INDEX_NONE 清空）并刷新；锁定变化走这里（清 Pending）
	void SetTargetPal(int32 InPartyIndex);

	// 刷新四槽与可学池显示（存储驱动走这里，保留装配中 Pending）
	void Refresh();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	// 候选控件类（BP 里设为 WBP_PalSkillChoice）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalSkill")
	TSubclassOf<UPalSkillChoiceWidget> ChoiceWidgetClass;

private:
	// 四个装备槽按钮的配置与点击绑定（每次 NativeConstruct 重做，Remove+Add 幂等）
	void ConfigureSlotButtons();

	// 按可学池全量重建动态候选（模板类进 ChoiceGrid；树重建后必须全量重建）
	void RebuildChoices();

	// 状态提示（选中/成功/拒绝/无目标）
	void UpdateStatus(const FText& Status);

	// Pending 齐备时提交 SetPalSkill
	void TryCommit();

	UFUNCTION()
	void HandleSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleChoiceClicked(FName SkillRowName);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USkillButtonWidget> SkillSlot0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USkillButtonWidget> SkillSlot1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USkillButtonWidget> SkillSlot2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USkillButtonWidget> SkillSlot3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> ChoiceGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UPalStorageComponent> Storage;

	UPROPERTY()
	TArray<TObjectPtr<UPalSkillChoiceWidget>> ChoiceWidgets;

	int32 TargetPartyIndex = INDEX_NONE;
	int32 PendingSlotIndex = INDEX_NONE;
	FName PendingRowName = NAME_None;
};
