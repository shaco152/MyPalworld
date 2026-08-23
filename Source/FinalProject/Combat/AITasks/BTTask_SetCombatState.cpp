#include "Combat/AITasks/BTTask_SetCombatState.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Characters/PalCharacter.h"
#include "Combat/PalAutoBattleComponent.h"

UBTTask_SetCombatState::UBTTask_SetCombatState()
{
	NodeName = TEXT("Set Pal Combat State");
}

EBTNodeResult::Type UBTTask_SetCombatState::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	const APalCharacter* Pal = AIController ? Cast<APalCharacter>(AIController->GetPawn()) : nullptr;
	UPalAutoBattleComponent* AutoBattle = Pal ? Pal->GetAutoBattleComponent() : nullptr;
	if (!AutoBattle)
	{
		return EBTNodeResult::Failed;
	}

	AutoBattle->SetCombatState(bInCombat);
	return EBTNodeResult::Succeeded;
}
