#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/PalSessionSubsystem.h"
#include "MultiplayerMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UPanelWidget;
class USessionEntryWidget;
class USpinBox;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FOnMultiplayerBackNative);

UCLASS()
class FINALPROJECT_API UMultiplayerMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FOnMultiplayerBackNative OnBack;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CreateRoomButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SearchRoomButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> RoomNameInput;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USpinBox> MaxPlayersInput;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> SessionListContainer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(EditDefaultsOnly, Category = "OnlineUI")
	TSubclassOf<USessionEntryWidget> SessionEntryWidgetClass;

private:
	UFUNCTION()
	void HandleCreateClicked();

	UFUNCTION()
	void HandleSearchClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleOperationFinished(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleSearchFinished(const TArray<FPalSessionView>& Results);

	void HandleJoinRequested(int32 ResultIndex);
	UPalSessionSubsystem* GetSessions() const;
};
