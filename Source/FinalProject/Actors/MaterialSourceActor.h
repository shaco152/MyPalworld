#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/HitReactInterface.h"
#include "MaterialSourceActor.generated.h"

class AMaterialPickupActor;
class UStaticMeshComponent;

/** 可采集资源点：收到 HitReact 时掉落材料；耐久耗尽后隐藏并可用 Timer 重生。 */
UCLASS()
class FINALPROJECT_API AMaterialSourceActor : public AActor, public IHitReactInterface
{
	GENERATED_BODY()

public:
	AMaterialSourceActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ReceiveHitReact_Implementation(AActor* HitInstigator, const FHitResult& HitResult) override;

protected:
	virtual void BeginPlay() override;

	/** BP_MaterialSource 子类在此挂树/矿石网格，并确保 Visibility 阻挡。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MaterialSource")
	TObjectPtr<UStaticMeshComponent> SourceMesh;

	/** DT_ItemDefinitions 的稳定行名。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MaterialSource")
	FName MaterialItemId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MaterialSource", meta = (ClampMin = "1"))
	int32 DropQuantityMin = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MaterialSource", meta = (ClampMin = "1"))
	int32 DropQuantityMax = 3;

	/** 每次 HitReact 掉一次；达到次数后资源点耗尽。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MaterialSource", meta = (ClampMin = "1"))
	int32 HitsUntilDepleted = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MaterialSource")
	TSubclassOf<AMaterialPickupActor> PickupClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MaterialSource", meta = (ClampMin = "0.0"))
	float DropSpawnRadius = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MaterialSource", meta = (ClampMin = "0.0"))
	float DropUpwardImpulse = 220.f;

	/** <=0 表示耗尽后不重生。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MaterialSource", meta = (ClampMin = "0.0"))
	float RespawnDelay = 30.f;

	/** BP 只实现震动、音效、粒子等表现，不承载掉落逻辑。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "MaterialSource", meta = (DisplayName = "On Hit React Visual"))
	void OnHitReactVisual();

	UFUNCTION(BlueprintImplementableEvent, Category = "MaterialSource", meta = (DisplayName = "On Depleted Visual"))
	void OnDepletedVisual();

	UPROPERTY(ReplicatedUsing = OnRep_Depleted)
	bool bDepleted = false;

	UPROPERTY(Replicated)
	int32 RemainingHits = 0;

	UFUNCTION()
	void OnRep_Depleted();

private:
	void SpawnMaterialDrop(const FHitResult& HitResult);
	void Deplete();
	void RespawnSource();

	FTimerHandle RespawnTimer;
};
