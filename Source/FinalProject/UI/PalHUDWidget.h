#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/PalSlotWidget.h" // BindWidget UPalSlotWidget 成员需要完整类型
#include "PalHUDWidget.generated.h"

class UPalStorageComponent;
class UProgressBar;
class UTextBlock;
class UPlayerAttributeSet;

/**
 * 常驻背包 HUD：一行 5 个槽显示背包帕鲁（WBP 预摆具名绑定），高亮当前选中的槽（左右方向键切换）。
 * 槽为 DisplayOnly 模式 → 不参与拖放，点击不拦截游戏输入。
 * 左上角显示玩家血条（WBP 摆放，属性集 OnHealthChanged 委托驱动——禁止 Tick）。
 */
UCLASS()
class FINALPROJECT_API UPalHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由控制器创建后调用一次：只保存存储引用并绑定刷新委托（注意 PC 调用顺序为 AddToViewport → 本函数，
	// 首次 NativeConstruct 时 Storage 可能尚未设置，绑定必须在本函数与 NativeConstruct 双路幂等进行）
	void InitFromStorage(UPalStorageComponent* InStorage);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 背包 5 槽（WBP 预摆，名称与槽位序号对应；NativeConstruct 每次重新配置为 DisplayOnly）
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

	// 玩家血条与数值文本（用户在 WBP_PalHUD 里摆放并设置样式；C++ 只更新数值）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> PlayerHPBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerHPText;

	// 存储变化回调
	UFUNCTION()
	void Refresh();

private:
	// 每次 NativeConstruct 重新配置 5 个预摆槽（AddToViewport 循环会重建控件树）
	void ConfigurePartySlots();

	// 存储委托绑定/解绑（Remove+Add 幂等）
	void BindStorage();
	void UnbindStorage();

	// 绑定/解绑玩家血量变化委托（事件驱动）
	void BindPlayerHealth();
	void UnbindPlayerHealth();

	// 血量变化回调（属性集委托触发）
	UFUNCTION()
	void OnPlayerHealthChanged(float Health, float MaxHealth);

	UPROPERTY()
	TArray<TObjectPtr<UPalSlotWidget>> PartySlots;

	UPROPERTY()
	TObjectPtr<UPalStorageComponent> Storage;

	// 已绑定的玩家属性集（解绑用）
	TWeakObjectPtr<UPlayerAttributeSet> BoundPlayerSet;
};
