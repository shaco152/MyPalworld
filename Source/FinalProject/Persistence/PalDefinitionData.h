#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PalDefinitionData.generated.h"

class APalCharacter;

/** DT_PalDefinitions：行名是稳定 PalDefinitionId，类引用只负责运行时生成。 */
USTRUCT(BlueprintType)
struct FINALPROJECT_API FPalDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal")
	TSoftClassPtr<APalCharacter> PalClass;
};
