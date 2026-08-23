#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatLibrary.generated.h"

class AActor;

/**
 * 战斗工具：统一伤害入口。
 * 自由战斗与回合制都走 ApplyDamage：按目标类型选 GE（帕鲁/玩家），
 * 应用 SetByCaller.Damage 伤害；击杀时若目标不在回合制（无 Battling 标签）则派发死亡处理。
 */
UCLASS()
class FINALPROJECT_API UCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 对目标造成伤害；返回是否击杀（目标 HP 归零）。
	// 自由战斗中击杀立即派发死亡（帕鲁销毁/玩家重生流程）；回合制目标由战斗流程处理退场
	UFUNCTION(BlueprintCallable, Category = "Combat")
	static bool ApplyDamage(AActor* Source, AActor* Target, float Amount);

	// 击杀后的死亡派发（帕鲁 → HandleDeath；玩家 → HandleDeath）
	static void HandleTargetKilled(AActor* Target);
};
