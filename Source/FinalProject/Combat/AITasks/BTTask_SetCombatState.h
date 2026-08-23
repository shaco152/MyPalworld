#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SetCombatState.generated.h"

/** 同步、幂等切换组件交战状态。 */
UCLASS()
class FINALPROJECT_API UBTTask_SetCombatState : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SetCombatState();

	UPROPERTY(EditAnywhere, Category = "Pal|AI")
	bool bInCombat = false;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
