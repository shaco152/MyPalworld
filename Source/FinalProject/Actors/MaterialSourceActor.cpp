#include "Actors/MaterialSourceActor.h"

#include "Actors/MaterialPickupActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AMaterialSourceActor::AMaterialSourceActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SourceMesh"));
	SetRootComponent(SourceMesh);
	SourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SourceMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PickupClass = AMaterialPickupActor::StaticClass();
}

void AMaterialSourceActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		RemainingHits = FMath::Max(1, HitsUntilDepleted);
	}
	OnRep_Depleted();

	UE_LOG(LogTemp, Warning, TEXT("[诊断] MaterialSource 初始化: %s ItemId=%s Hits=%d Pickup=%s"),
		*GetName(), *MaterialItemId.ToString(), RemainingHits, *GetNameSafe(PickupClass));
}

void AMaterialSourceActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMaterialSourceActor, bDepleted);
	DOREPLIFETIME(AMaterialSourceActor, RemainingHits);
}

void AMaterialSourceActor::ReceiveHitReact_Implementation(AActor* HitInstigator, const FHitResult& HitResult)
{
	if (!HasAuthority() || bDepleted || MaterialItemId.IsNone() || !PickupClass)
	{
		if (HasAuthority() && (MaterialItemId.IsNone() || !PickupClass))
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] MaterialSource[%s] 配置不完整：ItemId=%s Pickup=%s"),
				*GetName(), *MaterialItemId.ToString(), *GetNameSafe(PickupClass));
		}
		return;
	}

	OnHitReactVisual();
	SpawnMaterialDrop(HitResult);
	--RemainingHits;

	UE_LOG(LogTemp, Warning, TEXT("[诊断] 资源命中反应: Source=%s Instigator=%s RemainingHits=%d"),
		*GetName(), *GetNameSafe(HitInstigator), RemainingHits);
	if (RemainingHits <= 0)
	{
		Deplete();
	}
}

void AMaterialSourceActor::SpawnMaterialDrop(const FHitResult& HitResult)
{
	if (!GetWorld() || !PickupClass)
	{
		return;
	}

	const float RandomAngle = FMath::FRandRange(0.f, 2.f * PI);
	const float RandomRadius = FMath::FRandRange(0.f, FMath::Max(0.f, DropSpawnRadius));
	const FVector RadialOffset(FMath::Cos(RandomAngle) * RandomRadius, FMath::Sin(RandomAngle) * RandomRadius, 50.f);
	FVector DropOrigin = GetActorLocation();
	if (!HitResult.ImpactPoint.IsNearlyZero())
	{
		DropOrigin = HitResult.ImpactPoint;
	}
	const FVector SpawnLocation = DropOrigin + RadialOffset;
	const FTransform SpawnTransform(GetActorRotation(), SpawnLocation);

	AMaterialPickupActor* Pickup = GetWorld()->SpawnActorDeferred<AMaterialPickupActor>(PickupClass, SpawnTransform,
		this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Pickup)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] MaterialSource[%s] 生成 Pickup 失败"), *GetName());
		return;
	}

	FItemStack DropStack;
	DropStack.ItemId = MaterialItemId;
	DropStack.Quantity = FMath::RandRange(FMath::Max(1, DropQuantityMin), FMath::Max(DropQuantityMin, DropQuantityMax));
	Pickup->InitializePickup(DropStack);
	Pickup->FinishSpawning(SpawnTransform);

	const FVector HorizontalImpulse = FVector(FMath::Cos(RandomAngle), FMath::Sin(RandomAngle), 0.f) * 80.f;
	Pickup->ApplyDropImpulse(HorizontalImpulse + FVector::UpVector * DropUpwardImpulse);
}

void AMaterialSourceActor::Deplete()
{
	bDepleted = true;
	OnRep_Depleted();
	OnDepletedVisual();

	UE_LOG(LogTemp, Warning, TEXT("[诊断] MaterialSource[%s] 已耗尽，RespawnDelay=%.1f"), *GetName(), RespawnDelay);
	if (RespawnDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMaterialSourceActor::RespawnSource, RespawnDelay, false);
	}
}

void AMaterialSourceActor::RespawnSource()
{
	if (!HasAuthority())
	{
		return;
	}
	bDepleted = false;
	RemainingHits = FMath::Max(1, HitsUntilDepleted);
	OnRep_Depleted();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] MaterialSource[%s] 已重生，Hits=%d"), *GetName(), RemainingHits);
}

void AMaterialSourceActor::OnRep_Depleted()
{
	SetActorHiddenInGame(bDepleted);
	SetActorEnableCollision(!bDepleted);
}
