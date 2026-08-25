#include "UI/GameplayMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UGameplayMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResumeGameButton->OnClicked.AddUniqueDynamic(this, &UGameplayMenuWidget::HandleResumeClicked);
	SaveGameButton->OnClicked.AddUniqueDynamic(this, &UGameplayMenuWidget::HandleSaveClicked);
	if (OpenMultiplayerButton)
	{
		OpenMultiplayerButton->OnClicked.AddUniqueDynamic(this, &UGameplayMenuWidget::HandleOpenMultiplayerClicked);
	}
	ReturnToMainMenuButton->OnClicked.AddUniqueDynamic(this, &UGameplayMenuWidget::HandleReturnToMainMenuClicked);
	SetStatus(TEXT("游戏菜单"));
}

void UGameplayMenuWidget::NativeDestruct()
{
	if (ResumeGameButton)
	{
		ResumeGameButton->OnClicked.RemoveDynamic(this, &UGameplayMenuWidget::HandleResumeClicked);
	}
	if (SaveGameButton)
	{
		SaveGameButton->OnClicked.RemoveDynamic(this, &UGameplayMenuWidget::HandleSaveClicked);
	}
	if (OpenMultiplayerButton)
	{
		OpenMultiplayerButton->OnClicked.RemoveDynamic(this, &UGameplayMenuWidget::HandleOpenMultiplayerClicked);
	}
	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->OnClicked.RemoveDynamic(this, &UGameplayMenuWidget::HandleReturnToMainMenuClicked);
	}
	OnResumeRequested.Clear();
	OnSaveRequested.Clear();
	OnOpenMultiplayerRequested.Clear();
	OnReturnToMainMenuRequested.Clear();
	Super::NativeDestruct();
}

void UGameplayMenuWidget::SetStatus(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}

void UGameplayMenuWidget::SetBusy(bool bBusy, const FString& Message)
{
	if (ResumeGameButton)
	{
		ResumeGameButton->SetIsEnabled(!bBusy);
	}
	if (SaveGameButton)
	{
		SaveGameButton->SetIsEnabled(!bBusy);
	}
	if (OpenMultiplayerButton)
	{
		OpenMultiplayerButton->SetIsEnabled(!bBusy);
	}
	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->SetIsEnabled(!bBusy);
	}
	SetStatus(Message);
}

void UGameplayMenuWidget::SetCanOpenMultiplayer(bool bCanOpen)
{
	if (OpenMultiplayerButton)
	{
		OpenMultiplayerButton->SetVisibility(bCanOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		OpenMultiplayerButton->SetIsEnabled(bCanOpen);
	}
}

void UGameplayMenuWidget::HandleResumeClicked()
{
	OnResumeRequested.Broadcast();
}

void UGameplayMenuWidget::HandleSaveClicked()
{
	OnSaveRequested.Broadcast();
}

void UGameplayMenuWidget::HandleOpenMultiplayerClicked()
{
	OnOpenMultiplayerRequested.Broadcast();
}

void UGameplayMenuWidget::HandleReturnToMainMenuClicked()
{
	OnReturnToMainMenuRequested.Broadcast();
}
