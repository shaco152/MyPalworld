#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TurnBattleMainPanelWidget.generated.h"

class UButton;
class UTextBlock;
class UTurnBattleComponent;
class UPalBattleDetailWidget;
class USkillButtonWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnBattleSkillRequested, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTurnBattleActionRequested);

/** 战斗主页面：显示双方状态并把鼠标按钮请求上抛给根协调器。 */
UCLASS()
class FINALPROJECT_API UTurnBattleMainPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitFromBattle(UTurnBattleComponent* InBattle);
	void Refresh();

	UPROPERTY(BlueprintAssignable, Category = "TurnBattle")
	FOnTurnBattleSkillRequested OnSkillRequested;

	UPROPERTY(BlueprintAssignable, Category = "TurnBattle")
	FOnTurnBattleActionRequested OnSwitchRequested;

	UPROPERTY(BlueprintAssignable, Category = "TurnBattle")
	FOnTurnBattleActionRequested OnBallRequested;

	UPROPERTY(BlueprintAssignable, Category = "TurnBattle")
	FOnTurnBattleActionRequested OnHPMedicineRequested;

	UPROPERTY(BlueprintAssignable, Category = "TurnBattle")
	FOnTurnBattleActionRequested OnMPMedicineRequested;

protected:
	virtual void NativeOnInitialized() override;

private:
	void ConfigureSkillButtons();

	UFUNCTION()
	void HandleSkillClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleSwitchClicked();

	UFUNCTION()
	void HandleBallClicked();

	UFUNCTION()
	void HandleHPMedicineClicked();

	UFUNCTION()
	void HandleMPMedicineClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalBattleDetailWidget> EnemyDetail;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPalBattleDetailWidget> OurDetail;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USkillButtonWidget> SkillButton0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USkillButtonWidget> SkillButton1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USkillButtonWidget> SkillButton2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USkillButtonWidget> SkillButton3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SwitchButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BallButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MedHPButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MedMPButton;

	UPROPERTY()
	TObjectPtr<UTurnBattleComponent> Battle;
};
