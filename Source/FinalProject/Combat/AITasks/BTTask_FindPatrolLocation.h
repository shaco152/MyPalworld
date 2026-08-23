#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_FindPatrolLocation.generated.h"

/** 在帕鲁固定出生点周围选择一个随机可达的巡逻位置。 */
UCLASS()
class FINALPROJECT_API UBTTask_FindPatrolLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindPatrolLocation();

	UPROPERTY(EditAnywhere, Category = "Pal|AI")
	FBlackboardKeySelector PatrolLocationKey;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
