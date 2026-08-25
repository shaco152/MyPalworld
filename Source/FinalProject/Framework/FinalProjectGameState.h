#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "FinalProjectGameState.generated.h"

UENUM(BlueprintType)
enum class EWorldLoadState : uint8
{
	Restoring,
	Ready,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldLoadStateChanged, EWorldLoadState, NewState);

UCLASS()
class FINALPROJECT_API AFinalProjectGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Persistence")
	FOnWorldLoadStateChanged OnLoadStateChanged;

	UFUNCTION(BlueprintPure, Category = "Persistence")
	EWorldLoadState GetLoadState() const { return LoadState; }

	void SetLoadState(EWorldLoadState NewState);

private:
	UPROPERTY(ReplicatedUsing = OnRep_LoadState)
	EWorldLoadState LoadState = EWorldLoadState::Restoring;

	UFUNCTION()
	void OnRep_LoadState();
};
