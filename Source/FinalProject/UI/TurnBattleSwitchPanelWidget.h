#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TurnBattleSwitchPanelWidget.generated.h"

class UButton;
class UTurnBattleComponent;
class UPalBattleDetailWidget;
class UPalSlotWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnBattleSwitchConfirmed, int32, PartyIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTurnBattleSwitchCancelled);

/** 切换帕鲁子页面：拥有五槽选择状态，统一处理鼠标与控制器转发的选择请求。 */
UCLASS()
class FINALPROJECT_API UTurnBattleSwitchPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitFromBattle(UTurnBattleComponent* InBattle);
	void Open();
	void Refresh();
	void Navigate(int32 Direction);
	void Confirm();
	void Cancel();
	int32 GetSelectedIndex() const { return SwitchIndex; }

	UPROPERTY(BlueprintAssignable, Category = "TurnBattle")
	FOnTurnBattleSwitchConfirmed OnSwitchConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "TurnBattle")
	FOnTurnBattleSwitchCancelled OnSwitchCancelled;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void ConfigureSlots();
	bool IsBrowsable(int32 PartyIndex) const;
	bool IsSelectable(int32 PartyIndex) const;
	void SelectFrom(int32 StartIndex, int32 Direction);

	UFUNCTION()
	void HandleSlotSelected(int32 PartyIndex);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalBattleDetailWidget> SwitchDetail;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalSlotWidget> SwitchSlot0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalSlotWidget> SwitchSlot1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalSlotWidget> SwitchSlot2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalSlotWidget> SwitchSlot3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalSlotWidget> SwitchSlot4;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CancelButton;

	UPROPERTY()
	TObjectPtr<UTurnBattleComponent> Battle;

	UPROPERTY()
	TArray<TObjectPtr<UPalSlotWidget>> SwitchSlots;

	int32 SwitchIndex = -1;
};
