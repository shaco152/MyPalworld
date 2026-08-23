#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PalSkillData.generated.h"

class UPalSkillExecutor;
class UParticleSystem;
class USoundBase;

/** 技能演出类型：本阶段只选择近战/远程蒙太奇，不改变距离判定。 */
UENUM(BlueprintType)
enum class EPalSkillRangeType : uint8
{
	Melee,
	Ranged,
};

/**
 * 帕鲁技能 DataTable 行结构（用户在编辑器创建 DT_PalSkills 时选它作为 Row Structure）：
 * 行名 = 技能 ID，数值全部可在表格里调。
 */
USTRUCT(BlueprintType)
struct FINALPROJECT_API FPalSkillRow : public FTableRowBase
{
	GENERATED_BODY()

	// 显示名（回合制技能按钮文字）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FText DisplayName;

	// 基础威力（伤害公式：Power + 等级加成）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	float Power = 20.f;

	// MP 消耗（普攻为 0）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	float MPCost = 0.f;

	// 是否普攻：槽 0 固定普攻，不耗 MP，自由战斗中帕鲁只用它
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	bool bBasicAttack = false;

	// 描述（UI 悬浮提示）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FText Description;

	// 近战/远程分类：执行器据此选择帕鲁类上的两套攻击蒙太奇
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	EPalSkillRangeType RangeType = EPalSkillRangeType::Melee;

	// 每级伤害加成；默认 2 保持现有回合制公式，槽 0 也按用户确认使用 2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0.0"))
	float DamagePerLevel = 2.f;

	// 自定义执行器；为空时统一回退到 UPalSkillDirectDamageExecutor
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<UPalSkillExecutor> ExecutorClass;

	// 可选命中特效（目标位置）与音效（施法者位置）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Presentation")
	TObjectPtr<UParticleSystem> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Presentation")
	TObjectPtr<USoundBase> Sound;
};
