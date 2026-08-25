#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaveListWidget.generated.h"

class UButton;
class UPanelWidget;
class USaveSlotWidget;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSaveChosenNative, FGuid);
DECLARE_MULTICAST_DELEGATE(FOnSaveListBackNative);

UCLASS()
class FINALPROJECT_API USaveListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void RefreshSaveList();
	void SetStatus(const FString& Message);
	FOnSaveChosenNative OnSaveChosen;
	FOnSaveListBackNative OnBack;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> SaveListContainer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(EditDefaultsOnly, Category = "SaveUI")
	TSubclassOf<USaveSlotWidget> SaveSlotWidgetClass;

private:
	void HandleSaveSelected(FGuid WorldId);

	UFUNCTION()
	void HandleBackClicked();
};
