#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingBase.generated.h"

class UBoxComponent;
class UMaterialInterface;
class USceneComponent;

/**
 * 所有可放置建筑的稳定公共边界。
 * BP 子类只挂模型/音效并调整 PlacementBounds；箱子、门、床、制作台后续以组件组合扩展。
 */
UCLASS()
class FINALPROJECT_API ABuildingBase : public AActor
{
	GENERATED_BODY()

public:
	ABuildingBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializePlacedBuilding(FName InBuildingTypeId);
	void InitializePersistentBuilding(FName InBuildingTypeId, const FGuid& InPersistentId, const FGuid& InOwnerPlayerId);
	void SetPlacementPreview(bool bPreview, bool bCanPlace);

	UFUNCTION(BlueprintPure, Category = "Building")
	FName GetBuildingTypeId() const { return BuildingTypeId; }

	UFUNCTION(BlueprintPure, Category = "Building")
	FGuid GetPersistentId() const { return PersistentId; }
	FGuid GetOwnerPlayerId() const { return OwnerPlayerId; }

	UFUNCTION(BlueprintPure, Category = "Building")
	bool IsPlacementPreview() const { return bPlacementPreview; }

	FVector GetPlacementBoxExtent() const;
	FVector GetPlacementBoxRelativeLocation() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 放置阻挡检测体；BP 中按模型占地调整范围和相对位置，不参与实际碰撞。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	TObjectPtr<UBoxComponent> PlacementBounds;

	/** 有效/无效虚影材质；建议使用半透明或抖动 Mask 材质。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Preview")
	TObjectPtr<UMaterialInterface> ValidPreviewMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Preview")
	TObjectPtr<UMaterialInterface> InvalidPreviewMaterial;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Building|Persistence")
	FGuid PersistentId;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Building|Persistence")
	FName BuildingTypeId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Building|Persistence")
	FGuid OwnerPlayerId;

private:
	void ApplyPreviewMaterial(bool bCanPlace);

	bool bPlacementPreview = false;
	bool bLastCanPlace = false;
};
