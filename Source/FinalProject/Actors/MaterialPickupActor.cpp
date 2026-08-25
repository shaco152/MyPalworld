#include "Actors/MaterialPickupActor.h"

#include "Characters/PlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Items/ItemInventoryComponent.h"
#include "Net/UnrealNetwork.h"

AMaterialPickupActor::AMaterialPickupActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	PickupCollision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
	SetRootComponent(PickupCollision);
	PickupCollision->InitSphereRadius(28.f);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PickupCollision->SetCollisionObjectType(ECC_WorldDynamic);
	PickupCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	PickupCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	PickupCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupCollision->SetSimulatePhysics(true);
	PickupCollision->SetEnableGravity(true);
	PickupCollision->OnComponentBeginOverlap.AddDynamic(this, &AMaterialPickupActor::HandlePickupOverlap);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(PickupCollision);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMaterialPickupActor::BeginPlay()
{
	Super::BeginPlay();
	if (LifeSeconds > 0.f)
	{
		SetLifeSpan(LifeSeconds);
	}

	if (!ItemStack.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] MaterialPickup[%s] 的 ItemStack 无效，检查资源点 ItemId/数量"), *GetName());
	}
}

void AMaterialPickupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMaterialPickupActor, ItemStack);
}

void AMaterialPickupActor::InitializePickup(const FItemStack& InStack)
{
	if (HasAuthority())
	{
		ItemStack = InStack;
	}
}

void AMaterialPickupActor::ApplyDropImpulse(const FVector& Impulse) const
{
	if (PickupCollision && PickupCollision->IsSimulatingPhysics())
	{
		PickupCollision->AddImpulse(Impulse, NAME_None, true);
	}
}

void AMaterialPickupActor::HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !ItemStack.IsValid())
	{
		return;
	}

	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	UItemInventoryComponent* Inventory = Player ? Player->GetItemInventoryComponent() : nullptr;
	if (!Inventory)
	{
		return;
	}

	const int32 Added = Inventory->AddItem(ItemStack.ItemId, ItemStack.Quantity);
	if (Added <= 0)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("材料背包已满"));
		}
		return;
	}

	ItemStack.Quantity -= Added;
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 材料拾取: Player=%s Item=%s Added=%d Left=%d"),
		*GetNameSafe(Player), *ItemStack.ItemId.ToString(), Added, ItemStack.Quantity);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green,
			FString::Printf(TEXT("获得 %s x%d"), *ItemStack.ItemId.ToString(), Added));
	}

	if (ItemStack.Quantity <= 0)
	{
		Destroy();
	}
}
