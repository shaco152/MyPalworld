#include "TurnBattleMainPanelWidget.h"

#include "AbilitySystem/PalAttributeSet.h"
#include "Characters/PalCharacter.h"
#include "Combat/PalSkillLibrary.h"
#include "Combat/TurnBattleComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/PalBattleDetailWidget.h"
#include "UI/SkillButtonWidget.h"

void UTurnBattleMainPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ConfigureSkillButtons();

	if (SwitchButton)
	{
		SwitchButton->OnClicked.RemoveDynamic(this, &UTurnBattleMainPanelWidget::HandleSwitchClicked);
		SwitchButton->OnClicked.AddDynamic(this, &UTurnBattleMainPanelWidget::HandleSwitchClicked);
	}
	if (BallButton)
	{
		BallButton->OnClicked.RemoveDynamic(this, &UTurnBattleMainPanelWidget::HandleBallClicked);
		BallButton->OnClicked.AddDynamic(this, &UTurnBattleMainPanelWidget::HandleBallClicked);
	}
	if (MedHPButton)
	{
		MedHPButton->OnClicked.RemoveDynamic(this, &UTurnBattleMainPanelWidget::HandleHPMedicineClicked);
		MedHPButton->OnClicked.AddDynamic(this, &UTurnBattleMainPanelWidget::HandleHPMedicineClicked);
	}
	if (MedMPButton)
	{
		MedMPButton->OnClicked.RemoveDynamic(this, &UTurnBattleMainPanelWidget::HandleMPMedicineClicked);
		MedMPButton->OnClicked.AddDynamic(this, &UTurnBattleMainPanelWidget::HandleMPMedicineClicked);
	}
}

void UTurnBattleMainPanelWidget::InitFromBattle(UTurnBattleComponent* InBattle)
{
	Battle = InBattle;
	Refresh();
}

void UTurnBattleMainPanelWidget::ConfigureSkillButtons()
{
	USkillButtonWidget* Buttons[] = {SkillButton0, SkillButton1, SkillButton2, SkillButton3};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Buttons); ++Index)
	{
		if (USkillButtonWidget* SkillButton = Buttons[Index])
		{
			SkillButton->Configure(Index);
			SkillButton->OnSkillClicked.RemoveDynamic(this, &UTurnBattleMainPanelWidget::HandleSkillClicked);
			SkillButton->OnSkillClicked.AddDynamic(this, &UTurnBattleMainPanelWidget::HandleSkillClicked);
		}
	}
}

void UTurnBattleMainPanelWidget::Refresh()
{
	if (!Battle)
	{
		return;
	}

	if (EnemyDetail)
	{
		EnemyDetail->UpdateFromPal(Battle->GetCurrentEnemy());
	}
	if (OurDetail)
	{
		OurDetail->UpdateFromPal(Battle->GetOurPal());
	}
	if (MessageText)
	{
		MessageText->SetText(FText::FromString(Battle->GetBattleMessage()));
	}

	const bool bCanAct = Battle->IsPlayerActionPhase();
	const APalCharacter* OurPal = Battle->GetOurPal();
	const UPalAttributeSet* AttributeSet = OurPal ? OurPal->GetAttributeSet() : nullptr;
	const TArray<FName>* SkillSlots = OurPal ? &OurPal->GetSkillRowNames() : nullptr;
	USkillButtonWidget* Buttons[] = {SkillButton0, SkillButton1, SkillButton2, SkillButton3};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Buttons); ++Index)
	{
		USkillButtonWidget* SkillButton = Buttons[Index];
		if (!SkillButton)
		{
			continue;
		}

		FText Label = FText::FromString(TEXT("-"));
		bool bEnabled = false;
		if (OurPal && SkillSlots && SkillSlots->IsValidIndex(Index) && !(*SkillSlots)[Index].IsNone())
		{
			const FPalSkillRow Skill = UPalSkillLibrary::GetSkillRowChecked(OurPal->SkillTable, (*SkillSlots)[Index]);
			Label = FText::FromString(FString::Printf(TEXT("%s(%.0fMP)"), *Skill.DisplayName.ToString(), Skill.MPCost));
			bEnabled = bCanAct && AttributeSet && AttributeSet->GetMP() >= Skill.MPCost;
		}
		SkillButton->SetPresentation(Label, bEnabled);
	}

	if (SwitchButton)
	{
		SwitchButton->SetIsEnabled(bCanAct);
	}
	if (BallButton)
	{
		BallButton->SetIsEnabled(bCanAct);
	}
	if (MedHPButton)
	{
		MedHPButton->SetIsEnabled(bCanAct && Battle->GetHPMedCooldown() == 0);
	}
	if (MedMPButton)
	{
		MedMPButton->SetIsEnabled(bCanAct && Battle->GetMPMedCooldown() == 0);
	}
}

void UTurnBattleMainPanelWidget::HandleSkillClicked(int32 SlotIndex)
{
	OnSkillRequested.Broadcast(SlotIndex);
}

void UTurnBattleMainPanelWidget::HandleSwitchClicked()
{
	OnSwitchRequested.Broadcast();
}

void UTurnBattleMainPanelWidget::HandleBallClicked()
{
	OnBallRequested.Broadcast();
}

void UTurnBattleMainPanelWidget::HandleHPMedicineClicked()
{
	OnHPMedicineRequested.Broadcast();
}

void UTurnBattleMainPanelWidget::HandleMPMedicineClicked()
{
	OnMPMedicineRequested.Broadcast();
}
