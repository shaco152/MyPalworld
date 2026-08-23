#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "Engine/TimerHandle.h"
#include "BTTask_PalBasicAttack.generated.h"

/** 执行技能槽 0，并用每只 Pal 的 AttackInterval 一次性 Timer 潜伏等待。 */
UCLASS()
class FINALPROJECT_API UBTTask_PalBasicAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_PalBasicAttack();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private:
	void HandleCooldownFinished();
	void CleanupTimer();

	TWeakObjectPtr<UBehaviorTreeComponent> ActiveOwnerComp;
	TWeakObjectPtr<UWorld> ActiveWorld;
	FTimerHandle AttackTimer;
};
