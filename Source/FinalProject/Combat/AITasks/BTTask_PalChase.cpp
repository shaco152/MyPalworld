#include "Combat/AITasks/BTTask_PalChase.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Characters/PalCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Navigation/PathFollowingComponent.h"

namespace
{
	float GetChaseCapsuleRadius(const AActor* Actor)
	{
		if (const ACharacter* Character = Cast<ACharacter>(Actor))
		{
			if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
			{
				return Capsule->GetScaledCapsuleRadius();
			}
		}
		return 0.f;
	}
}

UBTTask_PalChase::UBTTask_PalChase()
{
	NodeName = TEXT("Pal Chase Target");
	bCreateNodeInstance = true;
	INIT_TASK_NODE_NOTIFY_FLAGS();
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_PalChase, BlackboardKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_PalChase::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	CleanupMove(true);

	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	APalCharacter* Pal = AIController ? Cast<APalCharacter>(AIController->GetPawn()) : nullptr;
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(GetSelectedBlackboardKey())) : nullptr;
	UPathFollowingComponent* PathFollowing = AIController ? AIController->GetPathFollowingComponent() : nullptr;
	if (!Pal || !IsValid(Target) || !PathFollowing)
	{
		return EBTNodeResult::Failed;
	}

	const float AcceptanceRadius = GetChaseCapsuleRadius(Pal) + GetChaseCapsuleRadius(Target) + 60.f;
	FAIMoveRequest MoveRequest(Target);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(false);
	MoveRequest.SetProjectGoalLocation(true);
	MoveRequest.SetCanStrafe(false);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetReachTestIncludesGoalRadius(false);

	ActiveOwnerComp = &OwnerComp;
	ActivePathFollowing = PathFollowing;
	MoveFinishedHandle = PathFollowing->OnRequestFinished.AddUObject(this, &UBTTask_PalChase::HandleMoveFinished);

	const FPathFollowingRequestResult RequestResult = AIController->MoveTo(MoveRequest);
	if (RequestResult.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		ActiveRequestID = RequestResult.MoveId;
		UE_LOG(LogTemp, Warning, TEXT("[诊断] BT 追击开始: %s → %s Request=%s 接受半径=%.0f"),
			*Pal->GetName(), *Target->GetName(), *ActiveRequestID.ToString(), AcceptanceRadius);
		return EBTNodeResult::InProgress;
	}

	CleanupMove(false);
	return RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal
		? EBTNodeResult::Succeeded
		: EBTNodeResult::Failed;
}

EBTNodeResult::Type UBTTask_PalChase::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	CleanupMove(true);
	return EBTNodeResult::Aborted;
}

void UBTTask_PalChase::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	CleanupMove(false);
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_PalChase::HandleMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (!ActiveRequestID.IsEquivalent(RequestID))
	{
		return;
	}

	UBehaviorTreeComponent* OwnerComp = ActiveOwnerComp.Get();
	CleanupMove(false);
	if (OwnerComp)
	{
		FinishLatentTask(*OwnerComp, Result.IsSuccess() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
	}
}

void UBTTask_PalChase::CleanupMove(bool bAbortActiveRequest)
{
	UPathFollowingComponent* PathFollowing = ActivePathFollowing.Get();
	if (PathFollowing && MoveFinishedHandle.IsValid())
	{
		PathFollowing->OnRequestFinished.Remove(MoveFinishedHandle);
	}
	MoveFinishedHandle.Reset();

	if (bAbortActiveRequest && PathFollowing && ActiveRequestID.IsValid() &&
		PathFollowing->GetCurrentRequestId().IsEquivalent(ActiveRequestID))
	{
		PathFollowing->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, ActiveRequestID,
			EPathFollowingVelocityMode::Reset);
	}

	ActiveRequestID = FAIRequestID::InvalidRequest;
	ActivePathFollowing.Reset();
	ActiveOwnerComp.Reset();
}
