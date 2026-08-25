#include "UI/MainMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Persistence/SaveGameSubsystem.h"
#include "UI/MultiplayerMenuWidget.h"
#include "UI/SaveListWidget.h"

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	StartGameButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::HandleStartGame);
	ContinueGameButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::HandleContinueGame);
	MultiplayerButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::HandleMultiplayer);
	SaveListPage->OnSaveChosen.AddUObject(this, &UMainMenuWidget::HandleSaveChosen);
	SaveListPage->OnBack.AddUObject(this, &UMainMenuWidget::ShowRootPage);
	MultiplayerPage->OnBack.AddUObject(this, &UMainMenuWidget::ShowRootPage);
	ShowRootPage();
}

void UMainMenuWidget::NativeDestruct()
{
	if (StartGameButton) StartGameButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleStartGame);
	if (ContinueGameButton) ContinueGameButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleContinueGame);
	if (MultiplayerButton) MultiplayerButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::HandleMultiplayer);
	if (SaveListPage)
	{
		SaveListPage->OnSaveChosen.Clear();
		SaveListPage->OnBack.Clear();
	}
	if (MultiplayerPage)
	{
		MultiplayerPage->OnBack.Clear();
	}
	Super::NativeDestruct();
}

void UMainMenuWidget::HandleStartGame()
{
	if (USaveGameSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveGameSubsystem>())
	{
		if (Saves->CreateNewWorld(TEXT("")).IsValid())
		{
			Saves->TravelToActiveWorld(false);
		}
	}
}

void UMainMenuWidget::HandleContinueGame()
{
	SaveListPage->RefreshSaveList();
	PageSwitcher->SetActiveWidget(SaveListPage);
}

void UMainMenuWidget::HandleMultiplayer()
{
	PageSwitcher->SetActiveWidget(MultiplayerPage);
}

void UMainMenuWidget::HandleSaveChosen(FGuid WorldId)
{
	if (USaveGameSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveGameSubsystem>())
	{
		if (Saves->LoadWorld(WorldId))
		{
			Saves->TravelToActiveWorld(false);
		}
		else
		{
			SaveListPage->SetStatus(TEXT("该世界存档损坏，A/B 缓冲均不可读取"));
		}
	}
}

void UMainMenuWidget::ShowRootPage()
{
	PageSwitcher->SetActiveWidget(RootPage);
}
