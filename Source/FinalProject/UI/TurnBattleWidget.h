#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TurnBattleWidget.generated.h"

class UTurnBattleComponent;
class UTurnBattleMainPanelWidget;
class UTurnBattleSwitchPanelWidget;

/**
 * 回合制战斗 UI 根协调器。
 * WBP 负责组合 MainPanel 与 SwitchPanel；本类只做上下文下发、页面显隐和行动请求路由。
 */
UCLASS()
class FINALPROJECT_API UTurnBattleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTurnBattleWidget(const FObjectInitializer& ObjectInitializer);

	void InitFromBattle(UTurnBattleComponent* InBattle);
	void Refresh();

	void ShowSwitchPanel();
	void HideSwitchPanel();
	bool IsSwitchPanelVisible() const;
	void NavigateSwitchSelection(int32 Direction);
	void ConfirmSwitchSelection();
	void CancelSwitchSelection();

protected:
	virtual void NativeOnInitialized() override;

private:
	void SetPanelVisibility(bool bShowMainPanel);

	UFUNCTION()
	void HandleSkillRequested(int32 SlotIndex);

	UFUNCTION()
	void HandleSwitchRequested();

	UFUNCTION()
	void HandleBallRequested();

	UFUNCTION()
	void HandleHPMedicineRequested();

	UFUNCTION()
	void HandleMPMedicineRequested();

	UFUNCTION()
	void HandleSwitchConfirmed(int32 PartyIndex);

	UFUNCTION()
	void HandleSwitchCancelled();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTurnBattleMainPanelWidget> MainPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTurnBattleSwitchPanelWidget> SwitchPanel;

	UPROPERTY()
	TObjectPtr<UTurnBattleComponent> Battle;
};
