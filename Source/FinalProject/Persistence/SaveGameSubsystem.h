#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Persistence/WorldSaveGame.h"
#include "SaveGameSubsystem.generated.h"

class AFinalProjectPlayerState;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldSaveOperationFinished, bool, bSuccess, const FString&, Message);

/** 版本化世界档、A/B 双缓冲与旅行前活动快照。 */
UCLASS()
class FINALPROJECT_API USaveGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "Save")
	FOnWorldSaveOperationFinished OnSaveFinished;

	UFUNCTION(BlueprintCallable, Category = "Save")
	FGuid CreateNewWorld(const FString& DisplayName);

	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadWorld(const FGuid& WorldId);

	UFUNCTION(BlueprintCallable, Category = "Save")
	void TravelToActiveWorld(bool bListenServer);

	UFUNCTION(BlueprintCallable, Category = "Save", meta = (WorldContext = "WorldContextObject"))
	bool SaveActiveWorld(UObject* WorldContextObject);

	/** 不产生写盘副作用，用于 UI 在提交存档请求前给出明确失败原因。 */
	bool CanSaveActiveWorld(const UObject* WorldContextObject, FString& OutFailureReason) const;

	UFUNCTION(BlueprintPure, Category = "Save")
	bool IsSaveInFlight() const { return bSaveInFlight; }

	UFUNCTION(BlueprintPure, Category = "Save")
	TArray<FWorldSaveMetadata> GetSaveList() const;

	/** 从磁盘重新读取索引，剔除 A/B 缓冲均不可读取的悬空条目，并返回最新列表。 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	TArray<FWorldSaveMetadata> RefreshSaveListFromDisk();

	UWorldSaveGame* GetActiveSave() const { return ActiveWorldSave; }
	/** 本机稳定玩家身份；仅用于向权威主机认领该世界中的玩家记录。 */
	FGuid GetLocalPlayerProfileId() const;
	const FPlayerSaveRecord* FindPlayerRecord(const AFinalProjectPlayerState* PlayerState) const;
	bool RestorePlayerState(AFinalProjectPlayerState* PlayerState);
	bool RestorePawn(AFinalProjectPlayerState* PlayerState, APawn* Pawn) const;

	void ClearActiveWorld();

private:
	static const FString IndexSlotName;
	static FString MakeBufferSlot(const FGuid& WorldId, const FString& Buffer);
	static FString MakePlayerKey(const AFinalProjectPlayerState* PlayerState);

	void LoadIndex();
	void LoadOrCreateLocalProfile();
	static FString MakeLocalProfileSlotName();
	void PersistIndex();
	UWorldSaveGame* LoadBestBuffer(const FGuid& WorldId, FString& OutBuffer) const;
	bool ValidateWorldForSave(const UWorld* World, FString& OutFailureReason) const;
	bool CaptureWorld(UWorld* World, UWorldSaveGame* TargetSave);
	bool ConvertStoredPalToSave(const struct FStoredPalInfo& Info, FPalSaveRecord& OutRecord) const;
	struct FStoredPalInfo ConvertSavePalToStored(const FPalSaveRecord& Record) const;
	void HandleAsyncSaveFinished(const FString& SlotName, const int32 UserIndex, bool bSuccess);
	FWorldSaveIndexEntry* FindIndexEntry(const FGuid& WorldId);
	const FWorldSaveIndexEntry* FindIndexEntry(const FGuid& WorldId) const;

	UPROPERTY()
	TObjectPtr<UWorldSaveIndex> SaveIndexObject;

	UPROPERTY()
	TObjectPtr<ULocalPlayerProfileSave> LocalPlayerProfile;

	FString LocalPlayerProfileSlotName;

	UPROPERTY()
	TObjectPtr<UWorldSaveGame> ActiveWorldSave;

	/** 异步回调前强持有，禁止裸 AddToRoot。 */
	UPROPERTY()
	TObjectPtr<UWorldSaveGame> PendingSaveObject;

	FString PendingBuffer;
	FDateTime ActiveLoadedAtUtc;
	bool bSaveInFlight = false;
};
