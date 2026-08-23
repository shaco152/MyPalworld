#include "TurnBattleSwitchPanelWidget.h"

#include "Combat/TurnBattleComponent.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Storage/PalStorageComponent.h"
#include "UI/PalBattleDetailWidget.h"
#include "UI/PalSlotWidget.h"

void UTurnBattleSwitchPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ConfigureSlots();
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.RemoveDynamic(this, &UTurnBattleSwitchPanelWidget::HandleConfirmClicked);
		ConfirmButton->OnClicked.AddDynamic(this, &UTurnBattleSwitchPanelWidget::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.RemoveDynamic(this, &UTurnBattleSwitchPanelWidget::HandleCancelClicked);
		CancelButton->OnClicked.AddDynamic(this, &UTurnBattleSwitchPanelWidget::HandleCancelClicked);
	}
}

void UTurnBattleSwitchPanelWidget::NativeDestruct()
{
	for (UPalSlotWidget* SlotWidget : SwitchSlots)
	{
		if (SlotWidget)
		{
			SlotWidget->OnSlotSelected.RemoveDynamic(this, &UTurnBattleSwitchPanelWidget::HandleSlotSelected);
		}
	}
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.RemoveDynamic(this, &UTurnBattleSwitchPanelWidget::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.RemoveDynamic(this, &UTurnBattleSwitchPanelWidget::HandleCancelClicked);
	}

	Super::NativeDestruct();
}

void UTurnBattleSwitchPanelWidget::InitFromBattle(UTurnBattleComponent* InBattle)
{
	Battle = InBattle;
}

void UTurnBattleSwitchPanelWidget::ConfigureSlots()
{
	SwitchSlots.Reset();
	SwitchSlots.Add(SwitchSlot0);
	SwitchSlots.Add(SwitchSlot1);
	SwitchSlots.Add(SwitchSlot2);
	SwitchSlots.Add(SwitchSlot3);
	SwitchSlots.Add(SwitchSlot4);

	for (int32 Index = 0; Index < SwitchSlots.Num(); ++Index)
	{
		if (UPalSlotWidget* SlotWidget = SwitchSlots[Index])
		{
			SlotWidget->ConfigureSlot(true, Index, EPalSlotInteractionMode::Selection);
			SlotWidget->OnSlotSelected.RemoveDynamic(this, &UTurnBattleSwitchPanelWidget::HandleSlotSelected);
			SlotWidget->OnSlotSelected.AddDynamic(this, &UTurnBattleSwitchPanelWidget::HandleSlotSelected);
		}
	}
}

void UTurnBattleSwitchPanelWidget::Open()
{
	const UPalStorageComponent* Storage = Battle ? Battle->GetStorage() : nullptr;
	const int32 StartIndex = Storage ? Storage->GetSummonedPartyIndex() + 1 : 0;
	SelectFrom(StartIndex, 1);
	Refresh();
}

void UTurnBattleSwitchPanelWidget::Refresh()
{
	UPalStorageComponent* Storage = Battle ? Battle->GetStorage() : nullptr;
	if (!Storage)
	{
		SwitchIndex = -1;
		if (SwitchDetail)
		{
			SwitchDetail->Clear();
		}
		return;
	}

	for (int32 Index = 0; Index < SwitchSlots.Num(); ++Index)
	{
		if (!SwitchSlots[Index])
		{
			continue;
		}
		const FStoredPalInfo Info = Storage->PartyPals.IsValidIndex(Index)
			? Storage->PartyPals[Index]
			: FStoredPalInfo();
		const bool bSummoned = Storage->HasSummonedPal() && Storage->GetSummonedPartyIndex() == Index;
		SwitchSlots[Index]->SetSlotData(Info, Index == SwitchIndex, bSummoned);
	}

	if (SwitchDetail)
	{
		if (Storage->PartyPals.IsValidIndex(SwitchIndex))
		{
			SwitchDetail->UpdateFromStoredInfo(Storage->PartyPals[SwitchIndex]);
		}
		else
		{
			SwitchDetail->Clear();
		}
	}

	const bool bCanAct = Battle && Battle->IsPlayerActionPhase();
	if (ConfirmButton)
	{
		ConfirmButton->SetIsEnabled(bCanAct && IsSelectable(SwitchIndex));
	}
	if (CancelButton)
	{
		CancelButton->SetIsEnabled(bCanAct);
	}
}

bool UTurnBattleSwitchPanelWidget::IsSelectable(int32 PartyIndex) const
{
	const UPalStorageComponent* Storage = Battle ? Battle->GetStorage() : nullptr;
	if (!Storage || !Storage->PartyPals.IsValidIndex(PartyIndex))
	{
		return false;
	}

	const FStoredPalInfo& Info = Storage->PartyPals[PartyIndex];
	const bool bCurrent = Storage->HasSummonedPal() && Storage->GetSummonedPartyIndex() == PartyIndex;
	return Info.IsValid() && Info.Health > 0.f && !bCurrent;
}

bool UTurnBattleSwitchPanelWidget::IsBrowsable(int32 PartyIndex) const
{
	const UPalStorageComponent* Storage = Battle ? Battle->GetStorage() : nullptr;
	return Storage && Storage->PartyPals.IsValidIndex(PartyIndex) && Storage->PartyPals[PartyIndex].IsValid();
}

void UTurnBattleSwitchPanelWidget::SelectFrom(int32 StartIndex, int32 Direction)
{
	const int32 Count = SwitchSlots.Num();
	if (Count <= 0)
	{
		SwitchIndex = -1;
		return;
	}

	const int32 Step = Direction < 0 ? -1 : 1;
	for (int32 Offset = 0; Offset < Count; ++Offset)
	{
		const int32 Candidate = ((StartIndex + Offset * Step) % Count + Count) % Count;
		if (IsBrowsable(Candidate))
		{
			SwitchIndex = Candidate;
			return;
		}
	}
	SwitchIndex = -1;
}

void UTurnBattleSwitchPanelWidget::Navigate(int32 Direction)
{
	if (!Battle || !Battle->IsPlayerActionPhase() || Direction == 0)
	{
		return;
	}

	const int32 Step = Direction < 0 ? -1 : 1;
	const int32 StartIndex = SwitchIndex >= 0 ? SwitchIndex + Step : (Step > 0 ? 0 : SwitchSlots.Num() - 1);
	SelectFrom(StartIndex, Step);
	Refresh();
}

void UTurnBattleSwitchPanelWidget::Confirm()
{
	if (!Battle || !Battle->IsPlayerActionPhase())
	{
		return;
	}
	if (!IsSelectable(SwitchIndex))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("没有可切换的帕鲁！"));
		}
		return;
	}
	OnSwitchConfirmed.Broadcast(SwitchIndex);
}

void UTurnBattleSwitchPanelWidget::Cancel()
{
	if (Battle && Battle->IsPlayerActionPhase())
	{
		OnSwitchCancelled.Broadcast();
	}
}

void UTurnBattleSwitchPanelWidget::HandleSlotSelected(int32 PartyIndex)
{
	if (!Battle || !Battle->IsPlayerActionPhase() || !IsBrowsable(PartyIndex))
	{
		return;
	}
	SwitchIndex = PartyIndex;
	Refresh();
}

void UTurnBattleSwitchPanelWidget::HandleConfirmClicked()
{
	Confirm();
}

void UTurnBattleSwitchPanelWidget::HandleCancelClicked()
{
	Cancel();
}
