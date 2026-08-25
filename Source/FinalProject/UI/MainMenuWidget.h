#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UMultiplayerMenuWidget;
class USaveListWidget;
class UTextBlock;
class UWidgetSwitcher;
class UWidget;

UCLASS()
class FINALPROJECT_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartGameButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ContinueGameButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MultiplayerButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> PageSwitcher;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> RootPage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USaveListWidget> SaveListPage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMultiplayerMenuWidget> MultiplayerPage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleStartGame();

	UFUNCTION()
	void HandleContinueGame();

	UFUNCTION()
	void HandleMultiplayer();

	void HandleSaveChosen(FGuid WorldId);
	void ShowRootPage();
};
