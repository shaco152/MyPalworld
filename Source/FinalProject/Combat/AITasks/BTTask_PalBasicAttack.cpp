#include "Combat/AITasks/BTTask_PalBasicAttack.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Characters/PalCharacter.h"
#include "Combat/PalAutoBattleComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"

namespace
{
	float GetAttackCapsuleRadius(const AActor* Actor)
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

UBTTask_PalBasicAttack::UBTTask_PalBasicAttack()
{
	NodeName = TEXT("Pal Basic Attack");
	bCreateNodeInstance = true;
	INIT_TASK_NODE_NOTIFY_FLAGS();
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_PalBasicAttack, BlackboardKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_PalBasicAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	CleanupTimer();

	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	APalCharacter* Pal = AIController ? Cast<APalCharacter>(AIController->GetPawn()) : nullptr;
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(GetSelectedBlackboardKey())) : nullptr;
	UPalAutoBattleComponent* AutoBattle = Pal ? Pal->GetAutoBattleComponent() : nullptr;
	if (!Pal || !IsValid(Target) || !AutoBattle)
	{
		return EBTNodeResult::Failed;
	}

	const float CenterDistance = FVector::Dist2D(Pal->GetActorLocation(), Target->GetActorLocation());
	const float SurfaceDistance = FMath::Max(0.f, CenterDistance - GetAttackCapsuleRadius(Pal) - GetAttackCapsuleRadius(Target));
	if (SurfaceDistance > Pal->AttackRange)
	{
		return EBTNodeResult::Failed;
	}

	const FVector FacingDirection = (Target->GetActorLocation() - Pal->GetActorLocation()).GetSafeNormal2D();
	if (!FacingDirection.IsNearlyZero())
	{
		Pal->SetActorRotation(Pal->GetFacingRotation(FacingDirection));
	}
	AIController->StopMovement();

	if (!AutoBattle->TryBasicAttack(Target))
	{
		return EBTNodeResult::Failed;
	}

	const float Cooldown = FMath::Max(0.f, Pal->AttackInterval);
	if (Cooldown <= KINDA_SMALL_NUMBER)
	{
		return EBTNodeResult::Succeeded;
	}

	UWorld* World = Pal->GetWorld();
	if (!World)
	{
		return EBTNodeResult::Failed;
	}

	ActiveOwnerComp = &OwnerComp;
	ActiveWorld = World;
	World->GetTimerManager().SetTimer(AttackTimer, this, &UBTTask_PalBasicAttack::HandleCooldownFinished, Cooldown, false);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] BT 普攻冷却: %s %.2fs"), *Pal->GetName(), Cooldown);
	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_PalBasicAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	CleanupTimer();
	return EBTNodeResult::Aborted;
}

void UBTTask_PalBasicAttack::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	CleanupTimer();
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_PalBasicAttack::HandleCooldownFinished()
{
	UBehaviorTreeComponent* OwnerComp = ActiveOwnerComp.Get();
	CleanupTimer();
	if (OwnerComp)
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
	}
}

void UBTTask_PalBasicAttack::CleanupTimer()
{
	if (UWorld* World = ActiveWorld.Get())
	{
		World->GetTimerManager().ClearTimer(AttackTimer);
	}
	AttackTimer.Invalidate();
	ActiveWorld.Reset();
	ActiveOwnerComp.Reset();
}
