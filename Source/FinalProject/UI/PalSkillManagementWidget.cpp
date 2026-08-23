#include "PalSkillManagementWidget.h"

#include "Characters/PalCharacter.h"
#include "Combat/PalSkillLibrary.h"
#include "Components/WrapBox.h"
#include "Components/TextBlock.h"
#include "Storage/PalStorageComponent.h"
#include "UI/SkillButtonWidget.h"

void UPalSkillManagementWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ConfigureSlotButtons();
}

void UPalSkillManagementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 树重建后槽按钮/候选都是新实例：重配置与全量重建（Remove+Add 幂等）
	ConfigureSlotButtons();
	RebuildChoices();
	Refresh();
}

void UPalSkillManagementWidget::InitFromStorage(UPalStorageComponent* InStorage)
{
	Storage = InStorage;
	Refresh();
}

void UPalSkillManagementWidget::SetTargetPal(int32 InPartyIndex)
{
	TargetPartyIndex = InPartyIndex;
	PendingSlotIndex = INDEX_NONE;
	PendingRowName = NAME_None;
	Refresh();
}

void UPalSkillManagementWidget::ConfigureSlotButtons()
{
	USkillButtonWidget* Slots[] = {SkillSlot0, SkillSlot1, SkillSlot2, SkillSlot3};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Slots); ++i)
	{
		// 注意局部变量不能叫 Slot（遮蔽 UWidget::Slot，C4458）
		if (USkillButtonWidget* SlotButton = Slots[i])
		{
			SlotButton->Configure(i);
			SlotButton->OnSkillClicked.RemoveDynamic(this, &UPalSkillManagementWidget::HandleSlotClicked);
			SlotButton->OnSkillClicked.AddDynamic(this, &UPalSkillManagementWidget::HandleSlotClicked);
		}
	}
}

void UPalSkillManagementWidget::RebuildChoices()
{
	// 解绑并清空旧候选（树重建后 ChoiceGrid 是空的新实例，动态叶子必须全量重建）
	for (UPalSkillChoiceWidget* Choice : ChoiceWidgets)
	{
		if (Choice)
		{
			Choice->OnSkillChoiceClicked.RemoveDynamic(this, &UPalSkillManagementWidget::HandleChoiceClicked);
		}
	}
	ChoiceWidgets.Reset();
	if (ChoiceGrid)
	{
		ChoiceGrid->ClearChildren();
	}

	if (!Storage || !ChoiceWidgetClass || TargetPartyIndex == INDEX_NONE ||
		!Storage->PartyPals.IsValidIndex(TargetPartyIndex) || !Storage->PartyPals[TargetPartyIndex].IsValid())
	{
		return;
	}

	const FStoredPalInfo& Info = Storage->PartyPals[TargetPartyIndex];
	const APalCharacter* PalCDO = Info.PalClass ? Info.PalClass->GetDefaultObject<APalCharacter>() : nullptr;
	const UDataTable* SkillTable = PalCDO ? PalCDO->SkillTable.Get() : nullptr;

	// 可学池按首次出现顺序去重；缺失行/普攻行不进候选
	TArray<FName> UniquePool;
	for (const FName RowName : UPalStorageComponent::GetLearnablePoolFor(Info))
	{
		UniquePool.AddUnique(RowName);
	}

	for (const FName RowName : UniquePool)
	{
		const FPalSkillRow* RowData = UPalSkillLibrary::GetSkillRow(SkillTable, RowName);
		if (!RowData)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] PalSkillManagement: 可学池行 %s 在技能表中不存在，跳过候选"), *RowName.ToString());
			continue;
		}
		if (RowData->bBasicAttack)
		{
			continue; // 普攻行不进学习槽候选
		}

		if (UPalSkillChoiceWidget* Choice = CreateWidget<UPalSkillChoiceWidget>(this, ChoiceWidgetClass))
		{
			const bool bEquipped = Info.SkillRowNames.Contains(RowName);
			Choice->SetChoice(RowName,
				FText::FromString(FString::Printf(TEXT("%s(%.0fMP)"), *RowData->DisplayName.ToString(), RowData->MPCost)),
				bEquipped, !bEquipped);
			Choice->OnSkillChoiceClicked.AddDynamic(this, &UPalSkillManagementWidget::HandleChoiceClicked);
			ChoiceWidgets.Add(Choice);
			if (ChoiceGrid)
			{
				ChoiceGrid->AddChildToWrapBox(Choice);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalSkillManagement: 可学池候选%d个（目标背包槽%d）"), ChoiceWidgets.Num(), TargetPartyIndex);
}

void UPalSkillManagementWidget::Refresh()
{
	USkillButtonWidget* Slots[] = {SkillSlot0, SkillSlot1, SkillSlot2, SkillSlot3};

	if (!Storage || TargetPartyIndex == INDEX_NONE ||
		!Storage->PartyPals.IsValidIndex(TargetPartyIndex) || !Storage->PartyPals[TargetPartyIndex].IsValid())
	{
		TargetPartyIndex = INDEX_NONE;
		PendingSlotIndex = INDEX_NONE;
		PendingRowName = NAME_None;
		for (int32 i = 0; i < UE_ARRAY_COUNT(Slots); ++i)
		{
			if (Slots[i])
			{
				Slots[i]->SetPresentation(i == 0 ? FText::FromString(TEXT("普攻")) : FText::FromString(TEXT("空技能槽")), false);
			}
		}
		RebuildChoices();
		UpdateStatus(FText::FromString(TEXT("点击左侧背包槽选择帕鲁")));
		return;
	}

	const FStoredPalInfo& Info = Storage->PartyPals[TargetPartyIndex];
	const APalCharacter* PalCDO = Info.PalClass ? Info.PalClass->GetDefaultObject<APalCharacter>() : nullptr;
	const UDataTable* SkillTable = PalCDO ? PalCDO->SkillTable.Get() : nullptr;

	for (int32 i = 0; i < UE_ARRAY_COUNT(Slots); ++i)
	{
		if (!Slots[i])
		{
			continue;
		}
		const FName RowName = Info.SkillRowNames.IsValidIndex(i) ? Info.SkillRowNames[i] : NAME_None;
		if (RowName.IsNone())
		{
			Slots[i]->SetPresentation(i == 0 ? FText::FromString(TEXT("普攻")) : FText::FromString(TEXT("空技能槽")), i != 0);
			continue;
		}
		const FPalSkillRow* RowData = UPalSkillLibrary::GetSkillRow(SkillTable, RowName);
		FText Label;
		if (RowData)
		{
			Label = FText::FromString(FString::Printf(TEXT("%s(%.0fMP)"), *RowData->DisplayName.ToString(), RowData->MPCost));
		}
		else
		{
			Label = FText::FromString(FString::Printf(TEXT("%s(缺行)"), *RowName.ToString()));
		}
		// 槽 0 永久禁用；槽 1-3 可点击（含空槽，作为装配目标）
		Slots[i]->SetPresentation(Label, i != 0);
	}

	RebuildChoices();

	if (PendingRowName.IsNone() && PendingSlotIndex == INDEX_NONE)
	{
		UpdateStatus(FText::FromString(TEXT("点候选技能，再点槽位 1-3 装配（顺序不限）")));
	}
	else if (PendingRowName.IsNone())
	{
		UpdateStatus(FText::FromString(TEXT("已选槽位，请点候选技能")));
	}
	else
	{
		UpdateStatus(FText::FromString(TEXT("已选技能，请点目标槽位 1-3")));
	}
}

void UPalSkillManagementWidget::UpdateStatus(const FText& Status)
{
	if (StatusText)
	{
		StatusText->SetText(Status);
	}
}

void UPalSkillManagementWidget::TryCommit()
{
	if (PendingSlotIndex == INDEX_NONE || PendingRowName.IsNone())
	{
		return;
	}
	if (!Storage || TargetPartyIndex == INDEX_NONE)
	{
		return;
	}

	const bool bOk = Storage->SetPalSkill(TargetPartyIndex, PendingSlotIndex, PendingRowName);
	if (bOk)
	{
		PendingSlotIndex = INDEX_NONE;
		PendingRowName = NAME_None;
		// 先刷新到常规引导态，最后写成功反馈——顺序颠倒会被 Refresh 同栈覆盖
		Refresh();
		UpdateStatus(FText::FromString(TEXT("装配成功！")));
	}
	else
	{
		// 拒绝（重复/缺失行/普攻行等）：保留 Pending 供用户纠正，底层 [诊断] 有具体原因；
		// 同样先刷新再写拒绝反馈
		Refresh();
		UpdateStatus(FText::FromString(TEXT("装配被拒绝（重复或不可用），请换选择")));
	}
}

void UPalSkillManagementWidget::HandleSlotClicked(int32 SlotIndex)
{
	if (SlotIndex <= 0 || SlotIndex > 3)
	{
		return; // 槽 0 普攻不可换
	}
	PendingSlotIndex = SlotIndex;
	TryCommit();
}

void UPalSkillManagementWidget::HandleChoiceClicked(FName SkillRowName)
{
	PendingRowName = SkillRowName;
	TryCommit();
}
