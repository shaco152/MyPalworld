#include "UI/SessionEntryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void USessionEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	JoinButton->OnClicked.RemoveDynamic(this, &USessionEntryWidget::HandleJoinClicked);
	JoinButton->OnClicked.AddDynamic(this, &USessionEntryWidget::HandleJoinClicked);
}

void USessionEntryWidget::NativeDestruct()
{
	if (JoinButton)
	{
		JoinButton->OnClicked.RemoveDynamic(this, &USessionEntryWidget::HandleJoinClicked);
	}
	OnJoin.Clear();
	Super::NativeDestruct();
}

void USessionEntryWidget::SetSessionData(const FPalSessionView& InData)
{
	SessionData = InData;
	if (RoomNameText)
	{
		RoomNameText->SetText(FText::FromString(SessionData.RoomName));
	}
	if (DetailsText)
	{
		DetailsText->SetText(FText::FromString(FString::Printf(TEXT("房主 %s  |  %d/%d  |  %d ms"),
			*SessionData.HostName, SessionData.CurrentPlayers, SessionData.MaxPlayers, SessionData.PingMs)));
	}
}

void USessionEntryWidget::HandleJoinClicked()
{
	OnJoin.Broadcast(SessionData.ResultIndex);
}
