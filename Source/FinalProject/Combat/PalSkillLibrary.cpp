#include "PalSkillLibrary.h"
#include "Engine/DataTable.h"

const FPalSkillRow* UPalSkillLibrary::GetSkillRow(const UDataTable* SkillTable, FName RowName)
{
	if (!SkillTable || RowName.IsNone())
	{
		return nullptr;
	}
	return SkillTable->FindRow<FPalSkillRow>(RowName, TEXT("PalSkillLibrary"));
}

FPalSkillRow UPalSkillLibrary::GetSkillRowChecked(const UDataTable* SkillTable, FName RowName)
{
	if (const FPalSkillRow* Row = GetSkillRow(SkillTable, RowName))
	{
		return *Row;
	}

	// 技能表缺失/行名不存在 → 默认普攻兜底
	FPalSkillRow Fallback;
	Fallback.DisplayName = FText::FromString(TEXT("普攻（默认）"));
	Fallback.Power = 10.f;
	Fallback.MPCost = 0.f;
	Fallback.bBasicAttack = true;
	Fallback.RangeType = EPalSkillRangeType::Melee;
	Fallback.Description = FText::FromString(TEXT("技能表缺失或行不存在，使用默认普攻"));
	return Fallback;
}

void UPalSkillLibrary::NormalizeSkillSlots(TArray<FName>& InOutSlots)
{
	// SetNum 缩减截断、扩张补默认构造的 FName（NAME_None = 空槽）
	InOutSlots.SetNum(SkillSlotCount);
}
