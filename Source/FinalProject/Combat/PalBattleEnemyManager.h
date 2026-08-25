#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PalBattleEnemyManager.generated.h"

class APalCharacter;

/**
 * 回合制敌方名单管理器：候场帕鲁保留原 Actor，仅暂停、隐藏并关闭碰撞；
 * 任一时刻只激活一只。这样原 AI 出生点与生态生成器持有的有效指针都不会在候场时丢失。
 */
UCLASS()
class FINALPROJECT_API UPalBattleEnemyManager : public UObject
{
	GENERATED_BODY()

public:
	// 接管候选名单并激活第一只；失败时自动恢复已接管 Actor。
	bool SetupRoster(const TArray<APalCharacter*>& Candidates, const FVector& InBattleLocation,
		const FVector& InFacingDirection);

	APalCharacter* GetActiveEnemy() const;
	int32 GetRemainingEnemyCount() const;
	bool HasRemainingEnemy() const { return GetRemainingEnemyCount() > 0; }
	bool HasRoster() const { return !Roster.IsEmpty(); }

	// 当前敌人真正退场，随后将下一只激活在同一战斗位置。
	APalCharacter* AdvanceRoster(bool bCaptured);

	// 战败/异常结束：所有仍存活的当前及候场 Actor 回原位并恢复自由 AI。
	void RestoreWorld();

	// 胜利后清空运行时名单；正常情况下全部敌人已经退场销毁。
	void Clear();

private:
	struct FEnemyEntry
	{
		TWeakObjectPtr<APalCharacter> Pal;
		FTransform OriginalTransform = FTransform::Identity;
		bool bWasHidden = false;
		bool bHadCollision = true;
	};

	void PrepareAsWaiting(FEnemyEntry& Entry);
	APalCharacter* ActivateNextValidEnemy();
	void RestoreEntry(FEnemyEntry& Entry);

	TArray<FEnemyEntry> Roster;
	int32 ActiveIndex = INDEX_NONE;
	FVector BattleLocation = FVector::ZeroVector;
	FVector FacingDirection = FVector(-1.f, 0.f, 0.f);
};
