#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingData.h"
#include "Components/ActorComponent.h"
#include "BuildingComponent.generated.h"

class ABuildingBase;
class UDataTable;
class UItemInventoryComponent;

UENUM(BlueprintType)
enum class EBuildModeState : uint8
{
	Inactive,
	CatalogOpen,
	Previewing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildModeStateChanged, EBuildModeState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlacementValidityChanged, bool, bCanPlace);

/**
 * 玩家建造状态机：目录选择 → 本地虚影 → 服务端校验/消耗/生成。
 * 虚影位置由短间隔 Timer 驱动，滚轮由 PlayerController 转发；全程无 Tick。
 */
UCLASS(ClassGroup = (Building), meta = (BlueprintSpawnableComponent))
class FINALPROJECT_API UBuildingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildingComponent();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	TObjectPtr<UDataTable> BuildingCatalog;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Placement", meta = (ClampMin = "0.02"))
	float PlacementRefreshInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Placement", meta = (ClampMin = "100.0"))
	float GroundTraceHalfHeight = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Placement", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float MaxGroundSlopeDegrees = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Placement", meta = (ClampMin = "1.0"))
	float RotationStepDegrees = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Placement", meta = (ClampMin = "0.0"))
	float ServerPlacementTolerance = 150.f;

	UPROPERTY(BlueprintAssignable, Category = "Building")
	FOnBuildModeStateChanged OnBuildModeStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Building")
	FOnPlacementValidityChanged OnPlacementValidityChanged;

	UFUNCTION(BlueprintCallable, Category = "Building")
	bool EnterBuildMode();

	UFUNCTION(BlueprintCallable, Category = "Building")
	void ExitBuildMode();

	UFUNCTION(BlueprintCallable, Category = "Building")
	bool SelectBuilding(FName BuildingId);

	UFUNCTION(BlueprintCallable, Category = "Building")
	bool ConfirmPlacement();

	void RotatePreview(float WheelDelta);
	void RefreshPreview();

	UFUNCTION(BlueprintPure, Category = "Building")
	EBuildModeState GetBuildModeState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Building")
	bool IsBuildModeActive() const { return State != EBuildModeState::Inactive; }

	UFUNCTION(BlueprintPure, Category = "Building")
	bool HasActivePreview() const { return State == EBuildModeState::Previewing && PreviewActor.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Building")
	bool CanPlacePreview() const { return bPreviewValid; }

	UFUNCTION(BlueprintPure, Category = "Building")
	FName GetActiveBuildingId() const { return ActiveBuildingId; }

	const FBuildingRecipeRow* FindRecipe(FName BuildingId) const;
	TArray<FName> GetRecipeIds() const;
	UItemInventoryComponent* GetInventory() const;
	UDataTable* GetBuildingCatalog() const { return BuildingCatalog; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable)
	void ServerPlaceBuilding(FName BuildingId, FTransform RequestedTransform);

	UFUNCTION(Client, Reliable)
	void ClientPlacementResult(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleInventoryChanged();

private:
	void SetState(EBuildModeState NewState);
	bool SpawnPreview(const FBuildingRecipeRow& Recipe);
	void DestroyPreview();
	bool CalculatePlacementTransform(const FBuildingRecipeRow& Recipe, FTransform& OutTransform) const;
	bool ValidatePlacementTransform(const FBuildingRecipeRow& Recipe, const FTransform& Candidate,
		AActor* ActorToIgnore, FString& OutReason) const;
	bool PlaceBuildingAuthoritative(FName BuildingId, const FTransform& RequestedTransform, FString& OutMessage);
	void SetPreviewValidity(bool bNewValid);

	UPROPERTY()
	TWeakObjectPtr<ABuildingBase> PreviewActor;

	UPROPERTY()
	TObjectPtr<UItemInventoryComponent> CachedInventory;

	EBuildModeState State = EBuildModeState::Inactive;
	FName ActiveBuildingId = NAME_None;
	float PreviewYawOffset = 0.f;
	bool bPreviewValid = false;
	FTimerHandle PreviewRefreshTimer;
};
