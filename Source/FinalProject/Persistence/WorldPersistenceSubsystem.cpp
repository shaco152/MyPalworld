#include "Persistence/WorldPersistenceSubsystem.h"

#include "Building/BuildingBase.h"
#include "Building/BuildingData.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

bool UWorldPersistenceSubsystem::RegisterBuilding(ABuildingBase* Building)
{
	if (!Building || !Building->HasAuthority() || Building->IsPlacementPreview() || !Building->GetPersistentId().IsValid())
	{
		return false;
	}
	const FGuid Id = Building->GetPersistentId();
	if (const TWeakObjectPtr<ABuildingBase>* Existing = BuildingsById.Find(Id); Existing && Existing->IsValid() && Existing->Get() != Building)
	{
		UE_LOG(LogTemp, Error, TEXT("[诊断] 建筑 PersistentId 重复：%s，保留 %s，销毁 %s"),
			*Id.ToString(), *GetNameSafe(Existing->Get()), *GetNameSafe(Building));
		Building->Destroy();
		return false;
	}
	BuildingsById.Add(Id, Building);
	return true;
}

void UWorldPersistenceSubsystem::UnregisterBuilding(ABuildingBase* Building)
{
	if (!Building || !Building->GetPersistentId().IsValid())
	{
		return;
	}
	if (const TWeakObjectPtr<ABuildingBase>* Existing = BuildingsById.Find(Building->GetPersistentId());
		Existing && Existing->Get() == Building)
	{
		BuildingsById.Remove(Building->GetPersistentId());
	}
}

void UWorldPersistenceSubsystem::CaptureBuildings(TArray<FBuildingSaveRecord>& OutRecords) const
{
	OutRecords.Reset();
	for (const TPair<FGuid, TWeakObjectPtr<ABuildingBase>>& Pair : BuildingsById)
	{
		const ABuildingBase* Building = Pair.Value.Get();
		if (!Building || Building->IsPlacementPreview() || Building->GetBuildingTypeId().IsNone())
		{
			continue;
		}
		FBuildingSaveRecord& Record = OutRecords.AddDefaulted_GetRef();
		Record.PersistentId = Pair.Key;
		Record.BuildingTypeId = Building->GetBuildingTypeId();
		Record.Transform = Building->GetActorTransform();
		Record.OwnerPlayerId = Building->GetOwnerPlayerId();
	}
}

bool UWorldPersistenceSubsystem::RestoreBuildings(const TArray<FBuildingSaveRecord>& Records, UDataTable* BuildingCatalog)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return false;
	}
	if (!BuildingCatalog)
	{
		if (!Records.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("[诊断] 世界包含 %d 条建筑，但 GameMode 未配置 BuildingCatalog，拒绝标记加载完成"), Records.Num());
			return false;
		}
		return true;
	}
	for (const FBuildingSaveRecord& Record : Records)
	{
		if (!Record.PersistentId.IsValid() || Record.BuildingTypeId.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 跳过无效建筑存档条目"));
			continue;
		}
		if (const TWeakObjectPtr<ABuildingBase>* Existing = BuildingsById.Find(Record.PersistentId); Existing && Existing->IsValid())
		{
			continue;
		}
		const FBuildingRecipeRow* Recipe = BuildingCatalog->FindRow<FBuildingRecipeRow>(Record.BuildingTypeId,
			TEXT("RestoreBuildings"), false);
		if (!Recipe || !Recipe->BuildingClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 建筑恢复失败：BuildingTypeId=%s 无配方或类"), *Record.BuildingTypeId.ToString());
			continue;
		}
		ABuildingBase* Building = World->SpawnActorDeferred<ABuildingBase>(Recipe->BuildingClass, Record.Transform,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Building)
		{
			continue;
		}
		Building->InitializePersistentBuilding(Record.BuildingTypeId, Record.PersistentId, Record.OwnerPlayerId);
		Building->SetPlacementPreview(false, true);
		Building->FinishSpawning(Record.Transform);
		if (!IsValid(Building) || !BuildingsById.Contains(Record.PersistentId))
		{
			if (IsValid(Building))
			{
				Building->Destroy();
			}
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 建筑恢复事务回滚：%s"), *Record.PersistentId.ToString());
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 世界建筑恢复完成：存档=%d 注册=%d"), Records.Num(), BuildingsById.Num());
	return true;
}
