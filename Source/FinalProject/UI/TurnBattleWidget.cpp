#include "TurnBattleWidget.h"

#include "Combat/TurnBattleComponent.h"
#include "UI/TurnBattleMainPanelWidget.h"
#include "UI/TurnBattleSwitchPanelWidget.h"

UTurnBattleWidget::UTurnBattleWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTurnBattleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (MainPanel)
	{
		MainPanel->OnSkillRequested.RemoveDynamic(this, &UTurnBattleWidget::HandleSkillRequested);
		MainPanel->OnSkillRequested.AddDynamic(this, &UTurnBattleWidget::HandleSkillRequested);
		MainPanel->OnSwitchRequested.RemoveDynamic(this, &UTurnBattleWidget::HandleSwitchRequested);
		MainPanel->OnSwitchRequested.AddDynamic(this, &UTurnBattleWidget::HandleSwitchRequested);
		MainPanel->OnBallRequested.RemoveDynamic(this, &UTurnBattleWidget::HandleBallRequested);
		MainPanel->OnBallRequested.AddDynamic(this, &UTurnBattleWidget::HandleBallRequested);
		MainPanel->OnHPMedicineRequested.RemoveDynamic(this, &UTurnBattleWidget::HandleHPMedicineRequested);
		MainPanel->OnHPMedicineRequested.AddDynamic(this, &UTurnBattleWidget::HandleHPMedicineRequested);
		MainPanel->OnMPMedicineRequested.RemoveDynamic(this, &UTurnBattleWidget::HandleMPMedicineRequested);
		MainPanel->OnMPMedicineRequested.AddDynamic(this, &UTurnBattleWidget::HandleMPMedicineRequested);
	}

	if (SwitchPanel)
	{
		SwitchPanel->OnSwitchConfirmed.RemoveDynamic(this, &UTurnBattleWidget::HandleSwitchConfirmed);
		SwitchPanel->OnSwitchConfirmed.AddDynamic(this, &UTurnBattleWidget::HandleSwitchConfirmed);
		SwitchPanel->OnSwitchCancelled.RemoveDynamic(this, &UTurnBattleWidget::HandleSwitchCancelled);
		SwitchPanel->OnSwitchCancelled.AddDynamic(this, &UTurnBattleWidget::HandleSwitchCancelled);
	}

	SetPanelVisibility(true);
}

void UTurnBattleWidget::InitFromBattle(UTurnBattleComponent* InBattle)
{
	Battle = InBattle;
	if (MainPanel)
	{
		MainPanel->InitFromBattle(InBattle);
	}
	if (SwitchPanel)
	{
		SwitchPanel->InitFromBattle(InBattle);
	}
	SetPanelVisibility(true);
	Refresh();
}

void UTurnBattleWidget::Refresh()
{
	if (MainPanel)
	{
		MainPanel->Refresh();
	}
	if (IsSwitchPanelVisible() && SwitchPanel)
	{
		SwitchPanel->Refresh();
	}
}

void UTurnBattleWidget::ShowSwitchPanel()
{
	if (!Battle || !Battle->IsPlayerActionPhase() || !SwitchPanel)
	{
		return;
	}

	SetPanelVisibility(false);
	SwitchPanel->Open();
}

void UTurnBattleWidget::HideSwitchPanel()
{
	SetPanelVisibility(true);
	Refresh();
}

bool UTurnBattleWidget::IsSwitchPanelVisible() const
{
	return SwitchPanel && SwitchPanel->GetVisibility() == ESlateVisibility::Visible;
}

void UTurnBattleWidget::NavigateSwitchSelection(int32 Direction)
{
	if (Battle && Battle->IsPlayerActionPhase() && IsSwitchPanelVisible() && SwitchPanel)
	{
		SwitchPanel->Navigate(Direction);
	}
}

void UTurnBattleWidget::ConfirmSwitchSelection()
{
	if (Battle && Battle->IsPlayerActionPhase() && IsSwitchPanelVisible() && SwitchPanel)
	{
		SwitchPanel->Confirm();
	}
}

void UTurnBattleWidget::CancelSwitchSelection()
{
	if (Battle && Battle->IsPlayerActionPhase() && IsSwitchPanelVisible() && SwitchPanel)
	{
		SwitchPanel->Cancel();
	}
}

void UTurnBattleWidget::SetPanelVisibility(bool bShowMainPanel)
{
	if (MainPanel)
	{
		MainPanel->SetVisibility(bShowMainPanel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SwitchPanel)
	{
		SwitchPanel->SetVisibility(bShowMainPanel ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UTurnBattleWidget::HandleSkillRequested(int32 SlotIndex)
{
	if (Battle)
	{
		Battle->TryUseSkill(SlotIndex);
	}
}

void UTurnBattleWidget::HandleSwitchRequested()
{
	ShowSwitchPanel();
}

void UTurnBattleWidget::HandleBallRequested()
{
	if (Battle)
	{
		Battle->TryThrowBall();
	}
}

void UTurnBattleWidget::HandleHPMedicineRequested()
{
	if (Battle)
	{
		Battle->TryUseMed(true);
	}
}

void UTurnBattleWidget::HandleMPMedicineRequested()
{
	if (Battle)
	{
		Battle->TryUseMed(false);
	}
}

void UTurnBattleWidget::HandleSwitchConfirmed(int32 PartyIndex)
{
	if (!Battle)
	{
		return;
	}

	HideSwitchPanel();
	Battle->TrySwitchPal(PartyIndex);
}

void UTurnBattleWidget::HandleSwitchCancelled()
{
	HideSwitchPanel();
}
