#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "ItemData.generated.h"

class UTexture2D;

/** 统一物品类别。材料背包当前只展示 Material，后续制作/装备仍复用同一 ItemId。 */
UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	Material UMETA(DisplayName = "材料"),
	Consumable UMETA(DisplayName = "消耗品"),
	Equipment UMETA(DisplayName = "装备"),
	Quest UMETA(DisplayName = "任务物品")
};

/** DT_ItemDefinitions 的行结构；DataTable 行名就是稳定 ItemId。 */
USTRUCT(BlueprintType)
struct FINALPROJECT_API FItemDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EItemCategory Category = EItemCategory::Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStackSize = 99;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon;
};

/** 背包中的一个堆叠。空堆叠 = ItemId 为空或 Quantity <= 0。 */
USTRUCT(BlueprintType)
struct FINALPROJECT_API FItemStack : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "0"))
	int32 Quantity = 0;

	bool IsValid() const { return !ItemId.IsNone() && Quantity > 0; }
};

/** 配方消耗条目。 */
USTRUCT(BlueprintType)
struct FINALPROJECT_API FItemAmount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	bool IsValid() const { return !ItemId.IsNone() && Quantity > 0; }
};
