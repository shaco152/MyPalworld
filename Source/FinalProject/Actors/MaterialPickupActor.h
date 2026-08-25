#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/ItemData.h"
#include "MaterialPickupActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/** 地面材料掉落物：物理落地，玩家 Pawn Overlap 时自动装入统一物品背包。 */
UCLASS()
class FINALPROJECT_API AMaterialPickupActor : public AActor
{
	GENERATED_BODY()

public:
	AMaterialPickupActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 资源点延迟生成时写入本次掉落堆叠。 */
	void InitializePickup(const FItemStack& InStack);

	/** 资源点生成后施加一次物理冲量。 */
	void ApplyDropImpulse(const FVector& Impulse) const;

	UFUNCTION(BlueprintPure, Category = "MaterialPickup")
	FItemStack GetItemStack() const { return ItemStack; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MaterialPickup")
	TObjectPtr<USphereComponent> PickupCollision;

	/** BP_Pickup 子类只需在这里挂材料外观网格。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MaterialPickup")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MaterialPickup", meta = (ClampMin = "0.0"))
	float LifeSeconds = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "MaterialPickup", meta = (ExposeOnSpawn = "true"))
	FItemStack ItemStack;

	UFUNCTION()
	void HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
