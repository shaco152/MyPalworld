#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PalSkillData.h"
#include "PalSkillLibrary.generated.h"

class UDataTable;

/**
 * 技能表工具：读 DataTable 技能行 + 技能槽规范化（4 槽：0 普攻 + 3 学习）。
 * 技能表资产 DT_PalSkills 由用户在编辑器创建，挂到需要读技能的系统上。
 */
UCLASS()
class FINALPROJECT_API UPalSkillLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 技能槽数量（槽 0 普攻不耗 MP，槽 1-3 学习技能）
	static constexpr int32 SkillSlotCount = 4;

	// 读技能表行；表为空或行不存在返回 nullptr（C++ 用）
	static const FPalSkillRow* GetSkillRow(const UDataTable* SkillTable, FName RowName);

	// 兜底读取：表/行不存在时返回内置默认普攻（Power=10, MPCost=0），保证 UI/结算永远有技能可用
	UFUNCTION(BlueprintCallable, Category = "Pal|Skill")
	static FPalSkillRow GetSkillRowChecked(const UDataTable* SkillTable, FName RowName);

	// 保证技能槽数量为 4（不足补空行名 NAME_None，多出截断）
	UFUNCTION(BlueprintCallable, Category = "Pal|Skill")
	static void NormalizeSkillSlots(UPARAM(ref) TArray<FName>& InOutSlots);
};
