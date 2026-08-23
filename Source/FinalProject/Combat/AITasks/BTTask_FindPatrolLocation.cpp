#include "Combat/AITasks/BTTask_FindPatrolLocation.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/PalAIController.h"
#include "NavigationSystem.h"

UBTTask_FindPatrolLocation::UBTTask_FindPatrolLocation()
{
	NodeName = TEXT("Find Pal Patrol Location");
	PatrolLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindPatrolLocation, PatrolLocationKey));
}

EBTNodeResult::Type UBTTask_FindPatrolLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const APalAIController* AIController = Cast<APalAIController>(OwnerComp.GetAIOwner());
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!AIController || !Blackboard || PatrolLocationKey.SelectedKeyName.IsNone())
	{
		return EBTNodeResult::Failed;
	}

	const UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(AIController->GetWorld());
	if (!NavigationSystem)
	{
		return EBTNodeResult::Failed;
	}

	FNavLocation PatrolLocation;
	if (!NavigationSystem->GetRandomReachablePointInRadius(
		AIController->GetHomeLocation(), AIController->GetPatrolRadius(), PatrolLocation))
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(PatrolLocationKey.SelectedKeyName, PatrolLocation.Location);
	return EBTNodeResult::Succeeded;
}
