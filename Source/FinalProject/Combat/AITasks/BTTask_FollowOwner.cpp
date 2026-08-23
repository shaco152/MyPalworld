#include "Combat/AITasks/BTTask_FollowOwner.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Characters/PalCharacter.h"
#include "Combat/PalAutoBattleComponent.h"
#include "Components/CapsuleComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

namespace
{
	float GetFollowCapsuleRadius(const AActor* Actor)
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

UBTTask_FollowOwner::UBTTask_FollowOwner()
{
	NodeName = TEXT("Pal Follow Owner");
	bCreateNodeInstance = true;
	INIT_TASK_NODE_NOTIFY_FLAGS();
}

EBTNodeResult::Type UBTTask_FollowOwner::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	CleanupState(true);

	AAIController* AIController = OwnerComp.GetAIOwner();
	APalCharacter* Pal = AIController ? Cast<APalCharacter>(AIController->GetPawn()) : nullptr;
	AActor* OwnerActor = Pal ? Pal->GetOwner() : nullptr;
	UPalAutoBattleComponent* AutoBattle = Pal ? Pal->GetAutoBattleComponent() : nullptr;
	UPathFollowingComponent* PathFollowing = AIController ? AIController->GetPathFollowingComponent() : nullptr;
	if (!Pal || !IsValid(OwnerActor) || !AutoBattle || !PathFollowing)
	{
		return EBTNodeResult::Failed;
	}

	const float AcceptanceRadius = GetFollowCapsuleRadius(Pal) + GetFollowCapsuleRadius(OwnerActor) + AutoBattle->GetFollowDistance();
	FAIMoveRequest MoveRequest(OwnerActor);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(false);
	MoveRequest.SetProjectGoalLocation(true);
	MoveRequest.SetCanStrafe(false);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetReachTestIncludesGoalRadius(false);

	ActiveOwnerComp = &OwnerComp;
	ActivePathFollowing = PathFollowing;
	MoveFinishedHandle = PathFollowing->OnRequestFinished.AddUObject(this, &UBTTask_FollowOwner::HandleMoveFinished);

	const FPathFollowingRequestResult RequestResult = AIController->MoveTo(MoveRequest);
	if (RequestResult.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		ActiveRequestID = RequestResult.MoveId;
		return EBTNodeResult::InProgress;
	}

	CleanupPath(false);
	if (RequestResult.Code == EPathFollowingRequestResult::AlreadyAtGoal && StartRefreshDelay())
	{
		return EBTNodeResult::InProgress;
	}

	CleanupState(false);
	return EBTNodeResult::Failed;
}

EBTNodeResult::Type UBTTask_FollowOwner::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	CleanupState(true);
	return EBTNodeResult::Aborted;
}

void UBTTask_FollowOwner::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	CleanupState(false);
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_FollowOwner::HandleMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (!ActiveRequestID.IsEquivalent(RequestID))
	{
		return;
	}

	UBehaviorTreeComponent* OwnerComp = ActiveOwnerComp.Get();
	CleanupPath(false);
	if (OwnerComp && Result.IsSuccess() && StartRefreshDelay())
	{
		return;
	}

	CleanupState(false);
	if (OwnerComp)
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
	}
}

void UBTTask_FollowOwner::HandleRefreshFinished()
{
	UBehaviorTreeComponent* OwnerComp = ActiveOwnerComp.Get();
	CleanupState(false);
	if (OwnerComp)
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
	}
}

bool UBTTask_FollowOwner::StartRefreshDelay()
{
	UBehaviorTreeComponent* OwnerComp = ActiveOwnerComp.Get();
	UWorld* World = OwnerComp ? OwnerComp->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	ActiveWorld = World;
	World->GetTimerManager().SetTimer(RefreshTimer, this, &UBTTask_FollowOwner::HandleRefreshFinished,
		FMath::Max(0.05f, FollowRefreshInterval), false);
	return true;
}

void UBTTask_FollowOwner::CleanupPath(bool bAbortActiveRequest)
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
}

void UBTTask_FollowOwner::CleanupTimer()
{
	if (UWorld* World = ActiveWorld.Get())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	RefreshTimer.Invalidate();
	ActiveWorld.Reset();
}

void UBTTask_FollowOwner::CleanupState(bool bAbortActiveRequest)
{
	CleanupPath(bAbortActiveRequest);
	CleanupTimer();
	ActiveOwnerComp.Reset();
}
