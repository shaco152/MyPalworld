#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PalSessionSubsystem.generated.h"

UENUM(BlueprintType)
enum class EPalSessionOperation : uint8
{
	Idle,
	Creating,
	Searching,
	Joining,
	Destroying,
	Travelling
};

USTRUCT(BlueprintType)
struct FINALPROJECT_API FPalSessionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Online")
	int32 ResultIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Online")
	FString RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "Online")
	FString HostName;

	UPROPERTY(BlueprintReadOnly, Category = "Online")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Online")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Online")
	int32 PingMs = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPalSessionOperationFinished, bool, bSuccess, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPalSessionSearchFinished, const TArray<FPalSessionView>&, Results);

/** shacoPal Session 创建/搜索/加入与失败回滚；Listen Server，不承载玩法真相。 */
UCLASS()
class FINALPROJECT_API UPalSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static const FName ProjectTagKey;
	static const FString ProjectTagValue;
	static const FName RoomNameKey;
	static const FName WorldIdKey;
	static const FName SaveRevisionKey;

	UPROPERTY(BlueprintAssignable, Category = "Online")
	FOnPalSessionOperationFinished OnOperationFinished;

	UPROPERTY(BlueprintAssignable, Category = "Online")
	FOnPalSessionSearchFinished OnSearchFinished;

	UFUNCTION(BlueprintCallable, Category = "Online")
	void CreateRoom(const FString& RoomName, int32 MaxPlayers = 4);

	/** 将当前已保存的 Standalone 世界原样重载为 Listen Server，不创建新世界档。 */
	UFUNCTION(BlueprintCallable, Category = "Online")
	void CreateRoomFromCurrentWorld(const FString& RoomName, int32 MaxPlayers = 4);

	UFUNCTION(BlueprintCallable, Category = "Online")
	void SearchRooms();

	UFUNCTION(BlueprintCallable, Category = "Online")
	void JoinRoom(int32 ResultIndex);

	UFUNCTION(BlueprintCallable, Category = "Online")
	void DestroyRoom();

	/** 若存在 Session，先异步销毁本地 Session，再安全返回主界面。 */
	UFUNCTION(BlueprintCallable, Category = "Online")
	bool LeaveToMainMenu();

	UFUNCTION(BlueprintPure, Category = "Online")
	EPalSessionOperation GetOperation() const { return Operation; }

	void HandleExternalTravelFailure(const FString& Reason);
	void HandleGameplayTravelSucceeded();

private:
	void BeginCreateRoomRequest(const FString& RoomName, int32 MaxPlayers, bool bUseCurrentWorld);
	void StartCreateAfterCleanup();
	void HandleCreateComplete(FName SessionName, bool bSuccess);
	void HandleFindComplete(bool bSuccess);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroyComplete(FName SessionName, bool bSuccess);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void BeginTimeout();
	void ClearTimeout();
	void HandleTimeout();
	void ClearOnlineDelegates();
	void FailAndCleanup(const FString& Reason, bool bReturnToMenu);
	IOnlineSessionPtr GetSessions() const;

	TSharedPtr<FOnlineSessionSearch> ActiveSearch;
	TArray<FOnlineSessionSearchResult> FilteredResults;
	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle PostLoadMapHandle;
	FTimerHandle TimeoutTimer;
	EPalSessionOperation Operation = EPalSessionOperation::Idle;
	FString PendingRoomName;
	FString PendingTravelMapPath;
	int32 PendingMaxPlayers = 4;
	bool bUseCurrentWorldSave = false;
	bool bCreateAfterDestroy = false;
	bool bReturnAfterDestroy = false;
	bool bReturnAfterDestroyIsFailure = true;
	FString PendingFailureReason;
};
