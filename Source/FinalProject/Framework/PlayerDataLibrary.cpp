#include "Framework/PlayerDataLibrary.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Items/ItemInventoryComponent.h"
#include "Storage/PalStorageComponent.h"

APlayerState* UPlayerDataLibrary::ResolvePlayerState(const AActor* ContextActor)
{
	if (!ContextActor)
	{
		return nullptr;
	}
	if (APlayerState* PlayerState = const_cast<APlayerState*>(Cast<APlayerState>(ContextActor)))
	{
		return PlayerState;
	}
	if (const AController* Controller = Cast<AController>(ContextActor))
	{
		return Controller->GetPlayerState<APlayerState>();
	}
	if (const APawn* Pawn = Cast<APawn>(ContextActor))
	{
		return Pawn->GetPlayerState<APlayerState>();
	}
	if (const APawn* Instigator = ContextActor->GetInstigator())
	{
		return Instigator->GetPlayerState<APlayerState>();
	}
	return ContextActor->GetOwner() ? ResolvePlayerState(ContextActor->GetOwner()) : nullptr;
}

UPalStorageComponent* UPlayerDataLibrary::ResolvePalStorage(const AActor* ContextActor)
{
	APlayerState* PlayerState = ResolvePlayerState(ContextActor);
	return PlayerState ? PlayerState->FindComponentByClass<UPalStorageComponent>() : nullptr;
}

UItemInventoryComponent* UPlayerDataLibrary::ResolveItemInventory(const AActor* ContextActor)
{
	APlayerState* PlayerState = ResolvePlayerState(ContextActor);
	return PlayerState ? PlayerState->FindComponentByClass<UItemInventoryComponent>() : nullptr;
}
