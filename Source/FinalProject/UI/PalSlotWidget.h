#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Storage/PalStorageComponent.h" // FStoredPalInfo 按值成员需要完整类型
#include "PalSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UBorder;
class UPalBoxWidget;

/** 拖放载荷：记录被拖槽的来源（背包/仓库 + 槽位索引） */
UCLASS()
class FINALPROJECT_API UPalDragPayload : public UObject
{
	GENERATED_BODY()

public:
	bool bFromParty = false;
	int32 SourceIndex = -1;
};

/** 槽交互模式（ConfigureSlot 指定） */
UENUM(BlueprintType)
enum class EPalSlotInteractionMode : uint8
{
	DisplayOnly,       // HUD：不拦截鼠标、不拖放、不选择
	StorageDragDrop,   // 仓库界面：拖放源/放置目标
	Selection,         // 回合制切换页：左键点击广播选择
};

/**
 * 帕鲁槽控件（背包 / 仓库 / HUD / 回合制切换页通用）：
 * 显示帕鲁名字与等级、高亮当前选中槽；交互模式由 ConfigureSlot 指定——
 * DisplayOnly（HUD，不拦截鼠标）/ StorageDragDrop（仓库，拖放源与放置目标）/ Selection（切换页，点击广播选择）。
 */
UCLASS()
class FINALPROJECT_API UPalSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPalSlotWidget(const FObjectInitializer& ObjectInitializer);

	// 槽位配置（由父控件在创建/NativeConstruct 时调用；取代原公开裸字段）
	void ConfigureSlot(bool bInPartySlot, int32 InSlotIndex,
		EPalSlotInteractionMode InMode = EPalSlotInteractionMode::DisplayOnly,
		UPalBoxWidget* InOwnerBox = nullptr);

	// 槽位索引只读访问（悬停日志等外部读取用；拖放载荷由槽内部生成）
	int32 GetSlotIndex() const { return SlotIndex; }

	// 设置槽显示内容：空槽灰色 + "空"，有效显示 "类名 Lv.X"，bActive 时高亮边框，
	// bSummoned 时显示"（出战）"标识且禁止拖放（出战帕鲁需先按 F 收回才能交换）
	void SetSlotData(const FStoredPalInfo& Info, bool bActive, bool bSummoned = false);

	// 悬停事件（仓库界面右侧详情面板监听；HUD 槽无人监听则不生效）
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotHovered, UPalSlotWidget*, Slot, const FStoredPalInfo&, Info);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSlotUnhovered);

	UPROPERTY(BlueprintAssignable, Category = "PalSlot")
	FOnSlotHovered OnSlotHovered;

	UPROPERTY(BlueprintAssignable, Category = "PalSlot")
	FOnSlotUnhovered OnSlotUnhovered;

	// 点击选择（Selection 模式且槽内有帕鲁时广播；存活/非出战槽的有效性过滤由切换面板负责）
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotSelected, int32, SlotIndex);

	UPROPERTY(BlueprintAssignable, Category = "PalSlot")
	FOnSlotSelected OnSlotSelected;

	// 仓库点击（StorageDragDrop 模式且"未发生拖拽"的 MouseUp 时广播）：
	// 携带槽对象/背包或仓库身份/索引/完整数据，供 PalBoxWidget 锁定技能管理目标/清空/提示。
	// 与 Selection 的 OnSlotSelected 两套语义互不混用
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnStorageSlotClicked, UPalSlotWidget*, Slot, bool, bFromParty, int32, SlotIndex, const FStoredPalInfo&, Info);

	UPROPERTY(BlueprintAssignable, Category = "PalSlot")
	FOnStorageSlotClicked OnStorageSlotClicked;

protected:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Icon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Highlight;

private:
	// 交互模式（ConfigureSlot 设置：DisplayOnly=HUD / StorageDragDrop=仓库 / Selection=回合切换页）
	EPalSlotInteractionMode InteractionMode = EPalSlotInteractionMode::DisplayOnly;

	// 是否背包侧槽（拖放载荷记录来源）
	bool bPartySlot = false;

	// 槽位索引（ConfigureSlot 设置，GetSlotIndex 只读访问）
	int32 SlotIndex = -1;

	// 归属的仓库界面（仅 StorageDragDrop 模式非空，拖放交换回调走它）
	UPROPERTY()
	TObjectPtr<UPalBoxWidget> OwnerBox;

	bool bHasPal = false;          // 本槽当前是否有帕鲁（空槽不可拖出）
	bool bIsSummonedSlot = false;  // 本槽是出战帕鲁的槽（禁止拖出/作为交换目标）
	bool bDragDetected = false;    // 本次按下后是否已触发拖拽（MouseUp 据此区分点击/拖拽）
	FStoredPalInfo CachedInfo;     // 当前槽数据（拖拽视觉重建用）
};
