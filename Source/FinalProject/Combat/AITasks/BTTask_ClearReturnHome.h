#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ClearReturnHome.generated.h"

/** 野生 Pal 到达出生点后清除返家请求。 */
UCLASS()
class FINALPROJECT_API UBTTask_ClearReturnHome : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ClearReturnHome();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
