#include "Online/PalSessionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Framework/FinalProjectGameInstance.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "Persistence/SaveGameSubsystem.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

const FName UPalSessionSubsystem::ProjectTagKey(TEXT("PROJECT_TAG"));
const FString UPalSessionSubsystem::ProjectTagValue(TEXT("shacoPal"));
const FName UPalSessionSubsystem::RoomNameKey(TEXT("ROOM_NAME"));
const FName UPalSessionSubsystem::WorldIdKey(TEXT("WORLD_ID"));
const FName UPalSessionSubsystem::SaveRevisionKey(TEXT("SAVE_REVISION"));

void UPalSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UPalSessionSubsystem::HandlePostLoadMap);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalSessionSubsystem 初始化：OSS=%s"),
		IOnlineSubsystem::Get() ? *IOnlineSubsystem::Get()->GetSubsystemName().ToString() : TEXT("None"));
}

void UPalSessionSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}
	ClearTimeout();
	ClearOnlineDelegates();
	ActiveSearch.Reset();
	FilteredResults.Reset();
	Super::Deinitialize();
}

IOnlineSessionPtr UPalSessionSubsystem::GetSessions() const
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	return OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
}

void UPalSessionSubsystem::CreateRoom(const FString& RoomName, int32 MaxPlayers)
{
	BeginCreateRoomRequest(RoomName, MaxPlayers, false);
}

void UPalSessionSubsystem::CreateRoomFromCurrentWorld(const FString& RoomName, int32 MaxPlayers)
{
	BeginCreateRoomRequest(RoomName, MaxPlayers, true);
}

void UPalSessionSubsystem::BeginCreateRoomRequest(const FString& RoomName, int32 MaxPlayers, bool bUseCurrentWorld)
{
	if (Operation != EPalSessionOperation::Idle)
	{
		OnOperationFinished.Broadcast(false, TEXT("已有联机操作正在进行"));
		return;
	}
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		OnOperationFinished.Broadcast(false, TEXT("在线 Session 服务不可用"));
		return;
	}

	USaveGameSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveGameSubsystem>();
	const UWorldSaveGame* ActiveSave = Saves ? Saves->GetActiveSave() : nullptr;
	if (bUseCurrentWorld)
	{
		UWorld* World = GetWorld();
		if (!World || World->GetNetMode() != NM_Standalone || !ActiveSave ||
			!ActiveSave->Metadata.WorldId.IsValid() || ActiveSave->Metadata.MapPath.IsEmpty())
		{
			OnOperationFinished.Broadcast(false, TEXT("只有已保存的 Standalone 世界可以开放联机"));
			return;
		}
	}

	bUseCurrentWorldSave = bUseCurrentWorld;
	const FString DefaultRoomName = ActiveSave && !ActiveSave->Metadata.DisplayName.IsEmpty()
		? ActiveSave->Metadata.DisplayName : TEXT("shacoPal 房间");
	PendingRoomName = RoomName.TrimStartAndEnd().IsEmpty() ? DefaultRoomName : RoomName.TrimStartAndEnd();
	PendingMaxPlayers = FMath::Clamp(MaxPlayers, 2, 16);
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		bCreateAfterDestroy = true;
		Operation = EPalSessionOperation::Destroying;
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UPalSessionSubsystem::HandleDestroyComplete));
		if (!Sessions->DestroySession(NAME_GameSession))
		{
			FailAndCleanup(TEXT("旧房间清理请求失败"), false);
		}
		else
		{
			BeginTimeout();
		}
		return;
	}
	StartCreateAfterCleanup();
}

void UPalSessionSubsystem::StartCreateAfterCleanup()
{
	IOnlineSessionPtr Sessions = GetSessions();
	ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!Sessions.IsValid() || !LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		FailAndCleanup(TEXT("本地在线身份不可用，无法创建房间"), false);
		return;
	}
	USaveGameSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveGameSubsystem>();
	if (!bUseCurrentWorldSave)
	{
		if (!Saves || !Saves->CreateNewWorld(PendingRoomName).IsValid())
		{
			FailAndCleanup(TEXT("创建联机世界档失败"), false);
			return;
		}
	}

	const UWorldSaveGame* ActiveSave = Saves ? Saves->GetActiveSave() : nullptr;
	const UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance());
	PendingTravelMapPath = ActiveSave && !ActiveSave->Metadata.MapPath.IsEmpty()
		? ActiveSave->Metadata.MapPath
		: (GameInstance ? GameInstance->GameplayMapPath : FString());
	if (PendingTravelMapPath.IsEmpty())
	{
		FailAndCleanup(TEXT("Gameplay 地图路径无效"), false);
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == FName(TEXT("NULL"));
	Settings.NumPublicConnections = PendingMaxPlayers;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.Set(ProjectTagKey, ProjectTagValue, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(RoomNameKey, PendingRoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(SETTING_MAPNAME, PendingTravelMapPath, EOnlineDataAdvertisementType::ViaOnlineService);
	if (ActiveSave)
	{
		Settings.Set(WorldIdKey, ActiveSave->Metadata.WorldId.ToString(EGuidFormats::Digits),
			EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		Settings.Set(SaveRevisionKey, LexToString(ActiveSave->Metadata.SaveRevision),
			EOnlineDataAdvertisementType::ViaOnlineService);
	}

	Operation = EPalSessionOperation::Creating;
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UPalSessionSubsystem::HandleCreateComplete));
	if (!Sessions->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Settings))
	{
		FailAndCleanup(TEXT("创建 Session 请求提交失败"), false);
		return;
	}
	BeginTimeout();
}

void UPalSessionSubsystem::HandleCreateComplete(FName SessionName, bool bSuccess)
{
	ClearTimeout();
	if (IOnlineSessionPtr Sessions = GetSessions(); Sessions.IsValid() && CreateHandle.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		CreateHandle.Reset();
	}
	if (!bSuccess)
	{
		FailAndCleanup(TEXT("创建房间失败"), false);
		return;
	}
	UWorld* World = GetWorld();
	if (!World || PendingTravelMapPath.IsEmpty())
	{
		FailAndCleanup(TEXT("创建成功但 Gameplay 地图路径无效"), true);
		return;
	}
	Operation = EPalSessionOperation::Travelling;
	const FString TravelUrl = PendingTravelMapPath + TEXT("?listen");
	if (!World->ServerTravel(TravelUrl))
	{
		FailAndCleanup(TEXT("创建成功但 Listen ServerTravel 启动失败"), true);
		return;
	}
	BeginTimeout();
	OnOperationFinished.Broadcast(true, bUseCurrentWorldSave
		? TEXT("当前世界已开放联机，正在重载为 Listen Server")
		: TEXT("房间创建成功，正在进入世界"));
}

void UPalSessionSubsystem::SearchRooms()
{
	if (Operation != EPalSessionOperation::Idle)
	{
		OnOperationFinished.Broadcast(false, TEXT("已有联机操作正在进行"));
		return;
	}
	IOnlineSessionPtr Sessions = GetSessions();
	ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!Sessions.IsValid() || !LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		OnOperationFinished.Broadcast(false, TEXT("在线身份不可用"));
		return;
	}
	ActiveSearch = MakeShared<FOnlineSessionSearch>();
	ActiveSearch->MaxSearchResults = 100;
	ActiveSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == FName(TEXT("NULL"));
	ActiveSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
	ActiveSearch->QuerySettings.Set(ProjectTagKey, ProjectTagValue, EOnlineComparisonOp::Equals);
	Operation = EPalSessionOperation::Searching;
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UPalSessionSubsystem::HandleFindComplete));
	if (!Sessions->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), ActiveSearch.ToSharedRef()))
	{
		FailAndCleanup(TEXT("搜索房间请求提交失败"), false);
		return;
	}
	BeginTimeout();
}

void UPalSessionSubsystem::HandleFindComplete(bool bSuccess)
{
	ClearTimeout();
	if (IOnlineSessionPtr Sessions = GetSessions(); Sessions.IsValid() && FindHandle.IsValid())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		FindHandle.Reset();
	}
	FilteredResults.Reset();
	TArray<FPalSessionView> Views;
	if (bSuccess && ActiveSearch.IsValid())
	{
		for (const FOnlineSessionSearchResult& Result : ActiveSearch->SearchResults)
		{
			FString Tag;
			if (!Result.Session.SessionSettings.Get(ProjectTagKey, Tag) || Tag != ProjectTagValue)
			{
				continue;
			}
			const int32 Index = FilteredResults.Add(Result);
			FPalSessionView& View = Views.AddDefaulted_GetRef();
			View.ResultIndex = Index;
			Result.Session.SessionSettings.Get(RoomNameKey, View.RoomName);
			if (View.RoomName.IsEmpty())
			{
				View.RoomName = TEXT("shacoPal 房间");
			}
			View.HostName = Result.Session.OwningUserName;
			View.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			View.CurrentPlayers = View.MaxPlayers - Result.Session.NumOpenPublicConnections;
			View.PingMs = Result.PingInMs;
		}
	}
	Operation = EPalSessionOperation::Idle;
	ActiveSearch.Reset();
	OnSearchFinished.Broadcast(Views);
	OnOperationFinished.Broadcast(bSuccess, bSuccess ? TEXT("房间搜索完成") : TEXT("房间搜索失败"));
	UE_LOG(LogTemp, Warning, TEXT("[诊断] shacoPal 房间搜索：Success=%d Results=%d"), bSuccess, Views.Num());
}

void UPalSessionSubsystem::JoinRoom(int32 ResultIndex)
{
	if (Operation != EPalSessionOperation::Idle || !FilteredResults.IsValidIndex(ResultIndex))
	{
		OnOperationFinished.Broadcast(false, TEXT("房间结果已失效，请重新搜索"));
		return;
	}
	IOnlineSessionPtr Sessions = GetSessions();
	ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	if (!Sessions.IsValid() || !LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		OnOperationFinished.Broadcast(false, TEXT("在线身份不可用"));
		return;
	}
	Operation = EPalSessionOperation::Joining;
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UPalSessionSubsystem::HandleJoinComplete));
	if (!Sessions->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, FilteredResults[ResultIndex]))
	{
		FailAndCleanup(TEXT("加入房间请求提交失败"), false);
		return;
	}
	BeginTimeout();
}

void UPalSessionSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	ClearTimeout();
	IOnlineSessionPtr Sessions = GetSessions();
	if (Sessions.IsValid() && JoinHandle.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		JoinHandle.Reset();
	}
	FString ConnectString;
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions.IsValid() ||
		!Sessions->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		FailAndCleanup(TEXT("加入房间失败或无法解析连接地址"), false);
		return;
	}
	if (const USaveGameSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveGameSubsystem>())
	{
		const FGuid ProfileId = Saves->GetLocalPlayerProfileId();
		if (ProfileId.IsValid())
		{
			ConnectString += FString::Printf(TEXT("?PlayerProfileId=%s"),
				*ProfileId.ToString(EGuidFormats::Digits));
		}
	}
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if (!PlayerController)
	{
		FailAndCleanup(TEXT("加入成功但本地 PlayerController 不存在"), true);
		return;
	}
	Operation = EPalSessionOperation::Travelling;
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
	BeginTimeout();
	OnOperationFinished.Broadcast(true, TEXT("已加入房间，正在连接 Listen Server"));
}

void UPalSessionSubsystem::DestroyRoom()
{
	if (Operation != EPalSessionOperation::Idle)
	{
		OnOperationFinished.Broadcast(false, TEXT("已有联机操作正在进行"));
		return;
	}
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
	{
		Operation = EPalSessionOperation::Idle;
		return;
	}
	Operation = EPalSessionOperation::Destroying;
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UPalSessionSubsystem::HandleDestroyComplete));
	if (!Sessions->DestroySession(NAME_GameSession))
	{
		FailAndCleanup(TEXT("销毁房间请求提交失败"), false);
		return;
	}
	BeginTimeout();
}

bool UPalSessionSubsystem::LeaveToMainMenu()
{
	if (Operation != EPalSessionOperation::Idle)
	{
		OnOperationFinished.Broadcast(false, TEXT("已有联机操作正在进行，暂时无法返回主界面"));
		return false;
	}

	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
	{
		if (UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance()))
		{
			GameInstance->ReturnToMainMenu();
			return true;
		}
		return false;
	}

	bReturnAfterDestroy = true;
	bReturnAfterDestroyIsFailure = false;
	PendingFailureReason.Reset();
	Operation = EPalSessionOperation::Destroying;
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UPalSessionSubsystem::HandleDestroyComplete));
	if (!Sessions->DestroySession(NAME_GameSession))
	{
		if (DestroyHandle.IsValid())
		{
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
			DestroyHandle.Reset();
		}
		Operation = EPalSessionOperation::Idle;
		bReturnAfterDestroy = false;
		bReturnAfterDestroyIsFailure = true;
		if (UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance()))
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 主动离开时 Session 销毁请求失败，执行本地返回兜底"));
			GameInstance->ReturnToMainMenu();
			return true;
		}
		return false;
	}
	BeginTimeout();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 主动离开：正在销毁当前 Session"));
	return true;
}

void UPalSessionSubsystem::HandleDestroyComplete(FName SessionName, bool bSuccess)
{
	ClearTimeout();
	if (IOnlineSessionPtr Sessions = GetSessions(); Sessions.IsValid() && DestroyHandle.IsValid())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		DestroyHandle.Reset();
	}
	Operation = EPalSessionOperation::Idle;
	if (bCreateAfterDestroy)
	{
		bCreateAfterDestroy = false;
		if (bSuccess)
		{
			StartCreateAfterCleanup();
		}
		else
		{
			OnOperationFinished.Broadcast(false, TEXT("旧房间销毁失败"));
		}
		return;
	}
	if (bReturnAfterDestroy)
	{
		bReturnAfterDestroy = false;
		if (UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance()))
		{
			if (bReturnAfterDestroyIsFailure)
			{
				GameInstance->ReturnToMainMenuWithReason(PendingFailureReason.IsEmpty() ? TEXT("联机旅行已回滚") : PendingFailureReason);
			}
			else
			{
				GameInstance->ReturnToMainMenu();
			}
		}
		bReturnAfterDestroyIsFailure = true;
		PendingFailureReason.Reset();
	}
	OnOperationFinished.Broadcast(bSuccess, bSuccess ? TEXT("房间已销毁") : TEXT("房间销毁失败"));
}

void UPalSessionSubsystem::HandleExternalTravelFailure(const FString& Reason)
{
	FailAndCleanup(Reason, true);
}

void UPalSessionSubsystem::HandleGameplayTravelSucceeded()
{
	if (Operation == EPalSessionOperation::Travelling)
	{
		ClearTimeout();
		Operation = EPalSessionOperation::Idle;
		bUseCurrentWorldSave = false;
		PendingTravelMapPath.Reset();
		UE_LOG(LogTemp, Warning, TEXT("[诊断] Session 旅行完成，进入 Idle"));
	}
}

void UPalSessionSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// GameMode 只存在于服务器；客户端必须通过全局地图加载回调结束 Travelling 状态。
	if (LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance() &&
		Operation == EPalSessionOperation::Travelling)
	{
		HandleGameplayTravelSucceeded();
	}
}

void UPalSessionSubsystem::BeginTimeout()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		// GameInstance 自有 TimerManager 跨 World Travel 存活，避免旧世界销毁后旅行超时失效。
		GameInstance->GetTimerManager().SetTimer(TimeoutTimer, this, &UPalSessionSubsystem::HandleTimeout, 20.f, false);
	}
}

void UPalSessionSubsystem::ClearTimeout()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(TimeoutTimer);
	}
}

void UPalSessionSubsystem::HandleTimeout()
{
	if (Operation == EPalSessionOperation::Destroying)
	{
		const bool bShouldReturnToMenu = bReturnAfterDestroy;
		const bool bReturnIsFailure = bReturnAfterDestroyIsFailure;
		const FString ReturnReason = PendingFailureReason.IsEmpty() ? TEXT("销毁房间超时") : PendingFailureReason;
		ClearOnlineDelegates();
		bCreateAfterDestroy = false;
		bUseCurrentWorldSave = false;
		PendingTravelMapPath.Reset();
		bReturnAfterDestroy = false;
		bReturnAfterDestroyIsFailure = true;
		PendingFailureReason.Reset();
		Operation = EPalSessionOperation::Idle;
		OnOperationFinished.Broadcast(false, TEXT("销毁房间超时，已结束本地等待"));
		if (bShouldReturnToMenu)
		{
			if (UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance()))
			{
				if (bReturnIsFailure)
				{
					GameInstance->ReturnToMainMenuWithReason(ReturnReason);
				}
				else
				{
					GameInstance->ReturnToMainMenu();
				}
			}
		}
		return;
	}
	FailAndCleanup(TEXT("联机操作超时"), Operation == EPalSessionOperation::Travelling);
}

void UPalSessionSubsystem::ClearOnlineDelegates()
{
	if (IOnlineSessionPtr Sessions = GetSessions(); Sessions.IsValid())
	{
		if (CreateHandle.IsValid()) Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		if (FindHandle.IsValid()) Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		if (JoinHandle.IsValid()) Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		if (DestroyHandle.IsValid()) Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	CreateHandle.Reset();
	FindHandle.Reset();
	JoinHandle.Reset();
	DestroyHandle.Reset();
}

void UPalSessionSubsystem::FailAndCleanup(const FString& Reason, bool bReturnToMenu)
{
	ClearTimeout();
	ClearOnlineDelegates();
	ActiveSearch.Reset();
	FilteredResults.Reset();
	bCreateAfterDestroy = false;
	bUseCurrentWorldSave = false;
	PendingTravelMapPath.Reset();
	IOnlineSessionPtr Sessions = GetSessions();
	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession))
	{
		bReturnAfterDestroy = bReturnToMenu;
		bReturnAfterDestroyIsFailure = true;
		PendingFailureReason = Reason;
		Operation = EPalSessionOperation::Destroying;
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UPalSessionSubsystem::HandleDestroyComplete));
		if (Sessions->DestroySession(NAME_GameSession))
		{
			BeginTimeout();
			OnOperationFinished.Broadcast(false, Reason);
			return;
		}
		if (DestroyHandle.IsValid())
		{
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
			DestroyHandle.Reset();
		}
		bReturnAfterDestroy = false;
	}
	Operation = EPalSessionOperation::Idle;
	OnOperationFinished.Broadcast(false, Reason);
	if (bReturnToMenu)
	{
		if (UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance()))
		{
			GameInstance->ReturnToMainMenuWithReason(Reason);
		}
	}
	PendingFailureReason.Reset();
}
