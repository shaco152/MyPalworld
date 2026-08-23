#include "Combat/AITasks/BTTask_ClearReturnHome.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

namespace
{
	const FName ReturnHomeKey(TEXT("bReturnHomeRequested"));
}

UBTTask_ClearReturnHome::UBTTask_ClearReturnHome()
{
	NodeName = TEXT("Clear Pal Return Home");
}

EBTNodeResult::Type UBTTask_ClearReturnHome::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsBool(ReturnHomeKey, false);
	return EBTNodeResult::Succeeded;
}
