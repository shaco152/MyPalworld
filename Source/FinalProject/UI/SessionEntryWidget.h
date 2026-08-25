#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/PalSessionSubsystem.h"
#include "SessionEntryWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSessionEntryJoinNative, int32);

UCLASS()
class FINALPROJECT_API USessionEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSessionData(const FPalSessionView& InData);
	FOnSessionEntryJoinNative OnJoin;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> JoinButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailsText;

private:
	UFUNCTION()
	void HandleJoinClicked();

	FPalSessionView SessionData;
};
