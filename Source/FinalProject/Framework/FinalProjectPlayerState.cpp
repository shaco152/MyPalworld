#include "Framework/FinalProjectPlayerState.h"

#include "Items/ItemInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Storage/PalStorageComponent.h"

AFinalProjectPlayerState::AFinalProjectPlayerState()
{
	NetUpdateFrequency = 10.f;
	PalStorage = CreateDefaultSubobject<UPalStorageComponent>(TEXT("PalStorage"));
	ItemInventory = CreateDefaultSubobject<UItemInventoryComponent>(TEXT("ItemInventory"));
}

void AFinalProjectPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFinalProjectPlayerState, PlayerPersistentId, COND_OwnerOnly);
}

void AFinalProjectPlayerState::EnsurePersistentId()
{
	if (HasAuthority() && !PlayerPersistentId.IsValid())
	{
		PlayerPersistentId = FGuid::NewGuid();
		ForceNetUpdate();
		UE_LOG(LogTemp, Warning, TEXT("[诊断] PlayerState 生成 PlayerPersistentId=%s Player=%s"),
			*PlayerPersistentId.ToString(), *GetPlayerName());
	}
}

void AFinalProjectPlayerState::SetPlayerPersistentIdForRestore(const FGuid& InId)
{
	if (HasAuthority() && InId.IsValid())
	{
		PlayerPersistentId = InId;
		ForceNetUpdate();
	}
}
