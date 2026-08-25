#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/TimerHandle.h"
#include "FinalProjectGameMode.generated.h"

class UDataTable;
class APlayerState;
class AController;

/** Gameplay 世界恢复顺序与主机自动保存的唯一协调器。 */
UCLASS()
class FINALPROJECT_API AFinalProjectGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFinalProjectGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Persistence")
	TObjectPtr<UDataTable> BuildingCatalog;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Persistence", meta = (ClampMin = "30.0"))
	float AutoSaveInterval = 120.f;

	/** 自动保存因战斗或并发写盘被跳过后的重试延迟。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Persistence", meta = (ClampMin = "5.0"))
	float AutoSaveRetryDelay = 15.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;

private:
	void AutoSave();
	void ScheduleAutoSave(float DelaySeconds);
	void TryMarkWorldReady();

	TSet<TWeakObjectPtr<APlayerState>> RestoredPlayerStates;
	TSet<TWeakObjectPtr<AController>> RestoredPawns;
	FTimerHandle AutoSaveTimer;
	float EffectiveAutoSaveInterval = 120.f;
	bool bWorldRestoreComplete = false;
};
