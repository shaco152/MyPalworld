#include "Actors/PalEcologySpawner.h"

#include "Characters/PalCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

APalEcologySpawner::APalEcologySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpawnArea = CreateDefaultSubobject<USphereComponent>(TEXT("SpawnArea"));
	SpawnArea->SetupAttachment(SceneRoot);
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnArea->SetGenerateOverlapEvents(false);
	SpawnArea->SetHiddenInGame(true);
	SpawnArea->SetSphereRadius(SpawnRadius);
}

void APalEcologySpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (SpawnArea)
	{
		SpawnArea->SetSphereRadius(FMath::Max(100.f, SpawnRadius));
	}
}

void APalEcologySpawner::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}

	if (bPopulateOnBeginPlay)
	{
		CheckPopulation();
	}
	else
	{
		ScheduleNextPopulationCheck();
	}
}

void APalEcologySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PopulationCheckTimer);
	for (const TWeakObjectPtr<APalCharacter>& PalPtr : ManagedPals)
	{
		if (APalCharacter* Pal = PalPtr.Get())
		{
			Pal->OnEndPlay.RemoveDynamic(this, &APalEcologySpawner::HandleManagedPalEndPlay);
		}
	}
	ManagedPals.Reset();
	Super::EndPlay(EndPlayReason);
}

int32 APalEcologySpawner::GetManagedPalCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<APalCharacter>& Pal : ManagedPals)
	{
		if (Pal.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

void APalEcologySpawner::ForcePopulationCheck()
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(PopulationCheckTimer);
		CheckPopulation();
	}
}

void APalEcologySpawner::CheckPopulation()
{
	if (!HasAuthority())
	{
		return;
	}

	CompactManagedPals();
	const int32 CurrentCount = GetManagedPalCount();
	const int32 Capacity = FMath::Max(0, MaxAlivePals - CurrentCount);
	int32 SpawnedCount = 0;

	if (Capacity > 0 && !PalClassPool.IsEmpty())
	{
		const int32 BatchMin = FMath::Max(1, FMath::Min(MinSpawnBatch, MaxSpawnBatch));
		const int32 BatchMax = FMath::Max(BatchMin, FMath::Max(MinSpawnBatch, MaxSpawnBatch));
		const int32 RequestedCount = FMath::Min(Capacity, FMath::RandRange(BatchMin, BatchMax));
		for (int32 Index = 0; Index < RequestedCount; ++Index)
		{
			if (TrySpawnOnePal())
			{
				++SpawnedCount;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] 生态检查: %s 有效=%d/%d，本次生成=%d，池大小=%d"),
		*GetName(), GetManagedPalCount(), MaxAlivePals, SpawnedCount, PalClassPool.Num());
	ScheduleNextPopulationCheck();
}

void APalEcologySpawner::ScheduleNextPopulationCheck()
{
	if (!HasAuthority())
	{
		return;
	}

	const float IntervalMin = FMath::Max(0.1f, FMath::Min(MinCheckInterval, MaxCheckInterval));
	const float IntervalMax = FMath::Max(IntervalMin, FMath::Max(MinCheckInterval, MaxCheckInterval));
	const float NextDelay = FMath::FRandRange(IntervalMin, IntervalMax);
	GetWorldTimerManager().SetTimer(PopulationCheckTimer, this,
		&APalEcologySpawner::CheckPopulation, NextDelay, false);
	UE_LOG(LogTemp, Verbose, TEXT("[诊断] 生态检查调度: %s 将在 %.1f 秒后再次检查"), *GetName(), NextDelay);
}

bool APalEcologySpawner::TrySpawnOnePal()
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Navigation = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!World || !Navigation || PalClassPool.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 生态生成失败: %s 缺少 World、NavMesh 或 PalClassPool"), *GetName());
		return false;
	}

	TArray<TSubclassOf<APalCharacter>> ValidClasses;
	for (const TSubclassOf<APalCharacter>& PalClass : PalClassPool)
	{
		if (PalClass)
		{
			ValidClasses.Add(PalClass);
		}
	}
	if (ValidClasses.IsEmpty())
	{
		return false;
	}

	for (int32 Attempt = 0; Attempt < FMath::Max(1, MaxSpawnAttemptsPerPal); ++Attempt)
	{
		FNavLocation NavLocation;
		if (!Navigation->GetRandomReachablePointInRadius(GetActorLocation(), SpawnRadius, NavLocation))
		{
			continue;
		}

		const TSubclassOf<APalCharacter> ChosenClass = ValidClasses[FMath::RandRange(0, ValidClasses.Num() - 1)];
		const FVector SpawnLocation = NavLocation.Location + FVector(0.f, 0.f, SpawnHeightOffset);
		const FRotator SpawnRotation(0.f, FMath::FRandRange(-180.f, 180.f), 0.f);
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
		APalCharacter* SpawnedPal = World->SpawnActor<APalCharacter>(ChosenClass, SpawnLocation, SpawnRotation, Params);
		if (SpawnedPal)
		{
			AdoptManagedPal(SpawnedPal);
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 生态生成成功: %s → %s"),
				*GetNameSafe(SpawnedPal), *SpawnedPal->GetActorLocation().ToString());
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] 生态生成失败: %s 在 %d 次导航/碰撞尝试后仍无可用点"),
		*GetName(), FMath::Max(1, MaxSpawnAttemptsPerPal));
	return false;
}

void APalEcologySpawner::CompactManagedPals()
{
	ManagedPals.RemoveAll([](const TWeakObjectPtr<APalCharacter>& Pal)
	{
		return !Pal.IsValid();
	});
}

void APalEcologySpawner::AdoptManagedPal(APalCharacter* Pal)
{
	if (!IsValid(Pal))
	{
		return;
	}
	ManagedPals.AddUnique(Pal);
	Pal->OnEndPlay.AddUniqueDynamic(this, &APalEcologySpawner::HandleManagedPalEndPlay);
}

void APalEcologySpawner::HandleManagedPalEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	ManagedPals.RemoveAll([Actor](const TWeakObjectPtr<APalCharacter>& Pal)
	{
		return !Pal.IsValid() || Pal.Get() == Actor;
	});
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 生态指针释放: %s，原因=%d，当前有效=%d"),
		*GetNameSafe(Actor), static_cast<int32>(EndPlayReason), GetManagedPalCount());
}
