#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PalEcologySpawner.generated.h"

class APalCharacter;
class USceneComponent;
class USphereComponent;

/**
 * 可放置的区域生态生成器：无 Tick，按 30–60 秒的一次性随机 Timer 检查自身管理的有效 Pal 指针。
 * 捕捉/击杀导致 Actor EndPlay 后自动释放指针；捕捉失败时原 Actor 仍有效并继续计数。
 */
UCLASS(Blueprintable)
class FINALPROJECT_API APalEcologySpawner : public AActor
{
	GENERATED_BODY()

public:
	APalEcologySpawner();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintPure, Category = "Pal|Ecology")
	int32 GetManagedPalCount() const;

	// PIE 调试入口：立即执行一次数量检查，并从此刻重新随机下一次检查时间。
	UFUNCTION(BlueprintCallable, Category = "Pal|Ecology")
	void ForcePopulationCheck();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pal|Ecology")
	TObjectPtr<USceneComponent> SceneRoot;

	// 仅作编辑器范围可视化，不参与碰撞检测。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pal|Ecology")
	TObjectPtr<USphereComponent> SpawnArea;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Ecology|Population")
	TArray<TSubclassOf<APalCharacter>> PalClassPool;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Ecology|Population", meta = (ClampMin = "1"))
	int32 MaxAlivePals = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Ecology|Population", meta = (ClampMin = "1"))
	int32 MinSpawnBatch = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Ecology|Population", meta = (ClampMin = "1"))
	int32 MaxSpawnBatch = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Ecology|Timing", meta = (ClampMin = "0.1"))
	float MinCheckInterval = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Ecology|Timing", meta = (ClampMin = "0.1"))
	float MaxCheckInterval = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Ecology|Timing")
	bool bPopulateOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Ecology|Placement", meta = (ClampMin = "100.0"))
	float SpawnRadius = 2000.f;

	// 从导航点上方生成，避免胶囊体嵌入地面。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Ecology|Placement", meta = (ClampMin = "0.0"))
	float SpawnHeightOffset = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Ecology|Placement", meta = (ClampMin = "1"))
	int32 MaxSpawnAttemptsPerPal = 8;

private:
	void CheckPopulation();
	void ScheduleNextPopulationCheck();
	bool TrySpawnOnePal();
	void CompactManagedPals();
	void AdoptManagedPal(APalCharacter* Pal);

	UFUNCTION()
	void HandleManagedPalEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<APalCharacter>> ManagedPals;

	FTimerHandle PopulationCheckTimer;
};
