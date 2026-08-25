#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PlayerDataLibrary.generated.h"

class UItemInventoryComponent;
class UPalStorageComponent;
class APlayerState;

/** 解析玩家长期数据的唯一窄入口，避免玩法代码依赖 Pawn/PlayerState 生命周期顺序。 */
UCLASS()
class FINALPROJECT_API UPlayerDataLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "PlayerData")
	static UPalStorageComponent* ResolvePalStorage(const AActor* ContextActor);

	UFUNCTION(BlueprintPure, Category = "PlayerData")
	static UItemInventoryComponent* ResolveItemInventory(const AActor* ContextActor);

private:
	static APlayerState* ResolvePlayerState(const AActor* ContextActor);
};
