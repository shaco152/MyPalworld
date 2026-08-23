#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/PalSlotWidget.h" // TSubclassOf<UPalSlotWidget> 的 UPROPERTY 反射需要完整类型
#include "PalBoxWidget.generated.h"

class UHorizontalBox;
class UWrapBox;
class UPalStorageComponent;
class UPalBattleDetailWidget;
class UPalSkillManagementWidget;

/**
 * 仓库界面（E 键开关）：左侧背包 5 槽（WBP 预摆具名绑定），右侧仓库格（动态数量，
 * 唯一例外：SlotWidgetClass 模板进设计器摆放的 BoxGrid），槽间拖放交换数据；
 * 监听 UPalStorageComponent::OnStorageChanged 自动刷新。布局/尺寸/间距全部在 WBP。
 */
UCLASS()
class FINALPROJECT_API UPalBoxWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由控制器创建后调用一次：只保存存储引用并绑定刷新委托（槽配置与动态建槽在 NativeConstruct 做）
	void InitFromStorage(UPalStorageComponent* InStorage);

	// 槽拖放回调：调存储组件交换（成功后委托广播自动触发 Refresh）
	bool HandleSlotDrop(bool bFromParty, int32 FromIndex, bool bToParty, int32 ToIndex);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 兜底放置目标：拖到背包/仓库面板空白处（没命中任何槽）时，由整个界面接收并放入该侧第一个空槽
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	// 背包槽面板（左侧，5 个预摆槽的父容器；同时用于空白处拖放命中检测）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> PartyPanel;

	// 背包 5 槽（WBP 预摆，名称与槽位序号对应；NativeConstruct 每次重新配置）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalSlotWidget> PartySlot0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalSlotWidget> PartySlot1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalSlotWidget> PartySlot2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalSlotWidget> PartySlot3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalSlotWidget> PartySlot4;

	// 仓库槽网格（WBP 摆放，动态仓库格挂这里；换行间距用 WrapBox 的 InnerSlotPadding 在设计器设）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> BoxGrid;

	// 悬浮详情面板（WBP 预摆 WBP_BattleDetail；悬停槽显示血量/MP/头像/等级，离开恢复锁定详情）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalBattleDetailWidget> DetailWidget;

	// 技能管理面板（WBP 预摆 WBP_PalSkillManagement；点击锁定背包槽后显示该帕鲁四槽与可学池）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalSkillManagementWidget> SkillManagementWidget;

	// 单槽控件类（BP 里设为 WBP_PalSlot，仅动态仓库格使用）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalBox")
	TSubclassOf<UPalSlotWidget> SlotWidgetClass;

	// 槽悬停回调：悬浮槽显示右侧详情（头像/名字/等级/HP/MP），离开恢复锁定详情
	UFUNCTION()
	void OnSlotHovered(UPalSlotWidget* PalSlot, const FStoredPalInfo& Info);

	UFUNCTION()
	void OnSlotUnhovered();

	// 仓库点击回调：锁定/清空技能管理目标（StorageDragDrop 未拖拽的点击）
	UFUNCTION()
	void OnStorageSlotClicked(UPalSlotWidget* PalSlot, bool bFromParty, int32 SlotIndex, const FStoredPalInfo& Info);

	// 存储变化回调
	UFUNCTION()
	void Refresh();

private:
	// 每次 NativeConstruct 重新配置 5 个预摆背包槽（AddToViewport 循环会重建控件树）
	void ConfigurePartySlots();

	// 每次 NativeConstruct 按 BoxPals.Num() 重建动态仓库槽（树重建后 BoxGrid 为空，必须全量重建）
	void EnsureBoxSlots();

	// 存储委托与槽悬停/点击委托的绑定/解绑（Remove+Add 幂等）
	void BindStorage();
	void UnbindStorage();
	void BindSlotHover();
	void UnbindSlotHover();
	void BindSlotClicks();
	void UnbindSlotClicks();

	// 技能管理目标维护与级联刷新
	void ClearSkillSelection();
	void ApplySkillTarget();   // 锁定变化 → SetTargetPal（清 Pending）；存储刷新走 Refresh() 保留 Pending
	void UpdateDetailFromSelection();

	UPROPERTY()
	TArray<TObjectPtr<UPalSlotWidget>> PartySlots;

	UPROPERTY()
	TArray<TObjectPtr<UPalSlotWidget>> BoxSlots;

	UPROPERTY()
	TObjectPtr<UPalStorageComponent> Storage;

	// 当前锁定的技能管理目标背包槽（INDEX_NONE = 无锁定）
	int32 SelectedPartyIndex = INDEX_NONE;
};
