#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_PalChase.generated.h"

class UPathFollowingComponent;
struct FPathFollowingResult;

/** 潜伏追击：等待本次 PathFollowing 请求结束，不使用 TickTask。 */
UCLASS()
class FINALPROJECT_API UBTTask_PalChase : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_PalChase();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private:
	void HandleMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
	void CleanupMove(bool bAbortActiveRequest);

	TWeakObjectPtr<UBehaviorTreeComponent> ActiveOwnerComp;
	TWeakObjectPtr<UPathFollowingComponent> ActivePathFollowing;
	FAIRequestID ActiveRequestID;
	FDelegateHandle MoveFinishedHandle;
};
