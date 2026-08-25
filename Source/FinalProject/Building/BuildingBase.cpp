#include "Building/BuildingBase.h"

#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

ABuildingBase::ABuildingBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PlacementBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("PlacementBounds"));
	PlacementBounds->SetupAttachment(SceneRoot);
	PlacementBounds->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	PlacementBounds->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	PlacementBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlacementBounds->SetHiddenInGame(true);
}

void ABuildingBase::BeginPlay()
{
	Super::BeginPlay();
	if (!bPlacementPreview && HasAuthority() && !PersistentId.IsValid())
	{
		PersistentId = FGuid::NewGuid();
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] BuildingBase BeginPlay: %s TypeId=%s PersistentId=%s Preview=%d"),
		*GetName(), *BuildingTypeId.ToString(), *PersistentId.ToString(), bPlacementPreview);
}

void ABuildingBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABuildingBase, PersistentId);
	DOREPLIFETIME(ABuildingBase, BuildingTypeId);
}

void ABuildingBase::InitializePlacedBuilding(FName InBuildingTypeId)
{
	BuildingTypeId = InBuildingTypeId;
}

void ABuildingBase::SetPlacementPreview(bool bPreview, bool bCanPlace)
{
	bPlacementPreview = bPreview;
	bLastCanPlace = bCanPlace;
	SetActorEnableCollision(!bPreview);
	if (bPreview)
	{
		ApplyPreviewMaterial(bCanPlace);
	}
}

void ABuildingBase::ApplyPreviewMaterial(bool bCanPlace)
{
	UMaterialInterface* PreviewMaterial = bCanPlace ? ValidPreviewMaterial.Get() : InvalidPreviewMaterial.Get();
	TArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* Mesh : MeshComponents)
	{
		if (!Mesh)
		{
			continue;
		}
		Mesh->SetRenderCustomDepth(true);
		Mesh->SetCustomDepthStencilValue(bCanPlace ? 1 : 2);
		if (PreviewMaterial)
		{
			for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
			{
				Mesh->SetMaterial(MaterialIndex, PreviewMaterial);
			}
		}
	}
}

FVector ABuildingBase::GetPlacementBoxExtent() const
{
	return PlacementBounds ? PlacementBounds->GetScaledBoxExtent() : FVector(100.f);
}

FVector ABuildingBase::GetPlacementBoxRelativeLocation() const
{
	return PlacementBounds ? PlacementBounds->GetRelativeLocation() : FVector(0.f, 0.f, 100.f);
}
