#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingBase.h"
#include "Engine/DataTable.h"
#include "Items/ItemData.h"
#include "BuildingData.generated.h"

class UTexture2D;

/** DT_BuildingRecipes 行结构；DataTable 行名就是稳定 BuildingId。 */
USTRUCT(BlueprintType)
struct FINALPROJECT_API FBuildingRecipeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	TObjectPtr<UTexture2D> Icon;

	/** C++ ABuildingBase 的 BP 子类，只挂网格、材质、音效等资产。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	TSubclassOf<ABuildingBase> BuildingClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	TArray<FItemAmount> MaterialCosts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "100.0"))
	float PlacementDistance = 400.f;

	/** 建筑 Actor 原点相对命中地面的 Z 偏移；模型枢轴在底部时保持 0。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	float PlacementZOffset = 0.f;
};
