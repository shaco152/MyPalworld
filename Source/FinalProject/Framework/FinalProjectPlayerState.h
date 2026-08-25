#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "FinalProjectPlayerState.generated.h"

class UItemInventoryComponent;
class UPalStorageComponent;

/** 玩家跨 Pawn、跨死亡与跨旅行的权威长期数据宿主。 */
UCLASS()
class FINALPROJECT_API AFinalProjectPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AFinalProjectPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPalStorageComponent* GetPalStorage() const { return PalStorage; }
	UItemInventoryComponent* GetItemInventory() const { return ItemInventory; }

	UFUNCTION(BlueprintPure, Category = "Persistence")
	FGuid GetPlayerPersistentId() const { return PlayerPersistentId; }

	void EnsurePersistentId();
	void SetPlayerPersistentIdForRestore(const FGuid& InId);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerData")
	TObjectPtr<UPalStorageComponent> PalStorage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerData")
	TObjectPtr<UItemInventoryComponent> ItemInventory;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Persistence")
	FGuid PlayerPersistentId;
};
