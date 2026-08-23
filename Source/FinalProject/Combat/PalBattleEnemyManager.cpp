#include "Combat/PalBattleEnemyManager.h"

#include "AbilitySystem/CaptureTags.h"
#include "AbilitySystemComponent.h"
#include "Characters/PalCharacter.h"
#include "Combat/PalAutoBattleComponent.h"

bool UPalBattleEnemyManager::SetupRoster(const TArray<APalCharacter*>& Candidates,
	const FVector& InBattleLocation, const FVector& InFacingDirection)
{
	Clear();
	BattleLocation = InBattleLocation;
	FacingDirection = InFacingDirection.GetSafeNormal2D();
	if (FacingDirection.IsNearlyZero())
	{
		FacingDirection = FVector(-1.f, 0.f, 0.f);
	}

	for (APalCharacter* Candidate : Candidates)
	{
		if (!IsValid(Candidate) || Candidate->IsDead())
		{
			continue;
		}

		FEnemyEntry& Entry = Roster.AddDefaulted_GetRef();
		Entry.Pal = Candidate;
		Entry.OriginalTransform = Candidate->GetActorTransform();
		Entry.bWasHidden = Candidate->IsHidden();
		Entry.bHadCollision = Candidate->GetActorEnableCollision();
		PrepareAsWaiting(Entry);
	}

	ActiveIndex = Roster.IsEmpty() ? INDEX_NONE : 0;
	APalCharacter* FirstEnemy = ActivateNextValidEnemy();
	if (!FirstEnemy)
	{
		RestoreWorld();
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 敌方名单接管失败：没有可激活的有效帕鲁"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] 敌方名单接管完成：总数=%d，当前=%s，候场=%d"),
		Roster.Num(), *GetNameSafe(FirstEnemy), FMath::Max(0, GetRemainingEnemyCount() - 1));
	return true;
}

APalCharacter* UPalBattleEnemyManager::GetActiveEnemy() const
{
	return Roster.IsValidIndex(ActiveIndex) ? Roster[ActiveIndex].Pal.Get() : nullptr;
}

int32 UPalBattleEnemyManager::GetRemainingEnemyCount() const
{
	if (ActiveIndex == INDEX_NONE)
	{
		return 0;
	}

	int32 Count = 0;
	for (int32 Index = ActiveIndex; Index < Roster.Num(); ++Index)
	{
		if (Roster[Index].Pal.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

APalCharacter* UPalBattleEnemyManager::AdvanceRoster(bool bCaptured)
{
	APalCharacter* DepartingEnemy = GetActiveEnemy();
	if (IsValid(DepartingEnemy))
	{
		BattleLocation = DepartingEnemy->GetActorLocation();
		DepartingEnemy->SetHPBarForced(false);
		if (UAbilitySystemComponent* ASC = DepartingEnemy->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag());
		}
		DepartingEnemy->Destroy();
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 敌方退场：%s，原因=%s"), *GetNameSafe(DepartingEnemy),
			bCaptured ? TEXT("捕捉") : TEXT("击败"));
	}

	if (Roster.IsValidIndex(ActiveIndex))
	{
		Roster[ActiveIndex].Pal.Reset();
	}
	ActiveIndex = ActiveIndex == INDEX_NONE ? INDEX_NONE : ActiveIndex + 1;
	return ActivateNextValidEnemy();
}

void UPalBattleEnemyManager::RestoreWorld()
{
	for (FEnemyEntry& Entry : Roster)
	{
		RestoreEntry(Entry);
	}
	Clear();
}

void UPalBattleEnemyManager::Clear()
{
	Roster.Reset();
	ActiveIndex = INDEX_NONE;
	BattleLocation = FVector::ZeroVector;
	FacingDirection = FVector(-1.f, 0.f, 0.f);
}

void UPalBattleEnemyManager::PrepareAsWaiting(FEnemyEntry& Entry)
{
	APalCharacter* Pal = Entry.Pal.Get();
	if (!IsValid(Pal))
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = Pal->GetAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag());
	}
	if (UPalAutoBattleComponent* AutoBattle = Pal->GetAutoBattleComponent())
	{
		AutoBattle->Pause();
	}
	Pal->SetHPBarForced(false);
	Pal->SetActorEnableCollision(false);
	Pal->SetActorHiddenInGame(true);
}

APalCharacter* UPalBattleEnemyManager::ActivateNextValidEnemy()
{
	while (Roster.IsValidIndex(ActiveIndex))
	{
		APalCharacter* Pal = Roster[ActiveIndex].Pal.Get();
		if (IsValid(Pal) && !Pal->IsDead())
		{
			Pal->SetActorHiddenInGame(false);
			Pal->SetActorEnableCollision(true);
			Pal->SetActorLocation(BattleLocation, false, nullptr, ETeleportType::TeleportPhysics);
			Pal->SetActorRotation(Pal->GetFacingRotation(FacingDirection));
			Pal->SetHPBarForced(true);
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 敌方上场：%s → %s，剩余=%d"),
				*Pal->GetName(), *BattleLocation.ToString(), GetRemainingEnemyCount());
			return Pal;
		}
		++ActiveIndex;
	}

	ActiveIndex = INDEX_NONE;
	return nullptr;
}

void UPalBattleEnemyManager::RestoreEntry(FEnemyEntry& Entry)
{
	APalCharacter* Pal = Entry.Pal.Get();
	if (!IsValid(Pal))
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = Pal->GetAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag());
	}
	Pal->SetActorTransform(Entry.OriginalTransform, false, nullptr, ETeleportType::TeleportPhysics);
	Pal->SetActorHiddenInGame(Entry.bWasHidden);
	Pal->SetActorEnableCollision(Entry.bHadCollision);
	Pal->SetHPBarForced(false);
	if (UPalAutoBattleComponent* AutoBattle = Pal->GetAutoBattleComponent())
	{
		AutoBattle->Resume();
		Pal->SetHPBarVisible(AutoBattle->IsInCombat());
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 敌方名单恢复：%s → 原出生位 %s"),
		*Pal->GetName(), *Entry.OriginalTransform.GetLocation().ToString());
}
