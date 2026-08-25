#include "UI/MultiplayerMenuWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "UI/SessionEntryWidget.h"

void UMultiplayerMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CreateRoomButton->OnClicked.AddUniqueDynamic(this, &UMultiplayerMenuWidget::HandleCreateClicked);
	SearchRoomButton->OnClicked.AddUniqueDynamic(this, &UMultiplayerMenuWidget::HandleSearchClicked);
	BackButton->OnClicked.AddUniqueDynamic(this, &UMultiplayerMenuWidget::HandleBackClicked);
	if (UPalSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnOperationFinished.AddUniqueDynamic(this, &UMultiplayerMenuWidget::HandleOperationFinished);
		Sessions->OnSearchFinished.AddUniqueDynamic(this, &UMultiplayerMenuWidget::HandleSearchFinished);
	}
}

void UMultiplayerMenuWidget::NativeDestruct()
{
	if (CreateRoomButton) CreateRoomButton->OnClicked.RemoveDynamic(this, &UMultiplayerMenuWidget::HandleCreateClicked);
	if (SearchRoomButton) SearchRoomButton->OnClicked.RemoveDynamic(this, &UMultiplayerMenuWidget::HandleSearchClicked);
	if (BackButton) BackButton->OnClicked.RemoveDynamic(this, &UMultiplayerMenuWidget::HandleBackClicked);
	if (UPalSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnOperationFinished.RemoveDynamic(this, &UMultiplayerMenuWidget::HandleOperationFinished);
		Sessions->OnSearchFinished.RemoveDynamic(this, &UMultiplayerMenuWidget::HandleSearchFinished);
	}
	OnBack.Clear();
	Super::NativeDestruct();
}

void UMultiplayerMenuWidget::HandleCreateClicked()
{
	if (UPalSessionSubsystem* Sessions = GetSessions())
	{
		StatusText->SetText(FText::FromString(TEXT("正在创建房间...")));
		Sessions->CreateRoom(RoomNameInput->GetText().ToString(), MaxPlayersInput ? FMath::RoundToInt(MaxPlayersInput->GetValue()) : 4);
	}
}

void UMultiplayerMenuWidget::HandleSearchClicked()
{
	SessionListContainer->ClearChildren();
	StatusText->SetText(FText::FromString(TEXT("正在搜索 shacoPal 房间...")));
	if (UPalSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->SearchRooms();
	}
}

void UMultiplayerMenuWidget::HandleBackClicked()
{
	OnBack.Broadcast();
}

void UMultiplayerMenuWidget::HandleOperationFinished(bool bSuccess, const FString& Message)
{
	StatusText->SetText(FText::FromString(Message));
}

void UMultiplayerMenuWidget::HandleSearchFinished(const TArray<FPalSessionView>& Results)
{
	SessionListContainer->ClearChildren();
	for (const FPalSessionView& Result : Results)
	{
		USessionEntryWidget* Entry = CreateWidget<USessionEntryWidget>(GetOwningPlayer(), SessionEntryWidgetClass);
		if (!Entry)
		{
			continue;
		}
		Entry->SetSessionData(Result);
		Entry->OnJoin.AddUObject(this, &UMultiplayerMenuWidget::HandleJoinRequested);
		SessionListContainer->AddChild(Entry);
	}
	StatusText->SetText(FText::FromString(Results.IsEmpty() ? TEXT("未找到 shacoPal 房间") :
		FString::Printf(TEXT("找到 %d 个房间"), Results.Num())));
}

void UMultiplayerMenuWidget::HandleJoinRequested(int32 ResultIndex)
{
	StatusText->SetText(FText::FromString(TEXT("正在加入房间...")));
	if (UPalSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->JoinRoom(ResultIndex);
	}
}

UPalSessionSubsystem* UMultiplayerMenuWidget::GetSessions() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UPalSessionSubsystem>() : nullptr;
}
