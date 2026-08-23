#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Engine/TimerHandle.h"
#include "BTTask_FollowOwner.generated.h"

class UPathFollowingComponent;
struct FPathFollowingResult;

/** 召唤 Pal 潜伏跟随 Owner；目标出现时由上层 Blackboard Decorator 中止。 */
UCLASS()
class FINALPROJECT_API UBTTask_FollowOwner : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FollowOwner();

	// 到达主人后以一次性 Timer 节流，再让树重新判断主人是否已移动。
	UPROPERTY(EditAnywhere, Category = "Pal|AI", meta = (ClampMin = "0.05"))
	float FollowRefreshInterval = 0.5f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private:
	void HandleMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
	void HandleRefreshFinished();
	bool StartRefreshDelay();
	void CleanupPath(bool bAbortActiveRequest);
	void CleanupTimer();
	void CleanupState(bool bAbortActiveRequest);

	TWeakObjectPtr<UBehaviorTreeComponent> ActiveOwnerComp;
	TWeakObjectPtr<UPathFollowingComponent> ActivePathFollowing;
	TWeakObjectPtr<UWorld> ActiveWorld;
	FAIRequestID ActiveRequestID;
	FDelegateHandle MoveFinishedHandle;
	FTimerHandle RefreshTimer;
};
