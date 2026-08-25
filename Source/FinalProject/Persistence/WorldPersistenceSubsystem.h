#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Persistence/WorldSaveGame.h"
#include "WorldPersistenceSubsystem.generated.h"

class ABuildingBase;
class UDataTable;

/** Authority 世界建筑注册表；事件注册，无 Tick。 */
UCLASS()
class FINALPROJECT_API UWorldPersistenceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	bool RegisterBuilding(ABuildingBase* Building);
	void UnregisterBuilding(ABuildingBase* Building);
	void CaptureBuildings(TArray<FBuildingSaveRecord>& OutRecords) const;
	bool RestoreBuildings(const TArray<FBuildingSaveRecord>& Records, UDataTable* BuildingCatalog);

private:
	TMap<FGuid, TWeakObjectPtr<ABuildingBase>> BuildingsById;
};
