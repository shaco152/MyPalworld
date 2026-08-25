#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Persistence/WorldSaveGame.h"
#include "SaveSlotWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSaveSlotSelectedNative, FGuid);

UCLASS()
class FINALPROJECT_API USaveSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSaveMetadata(const FWorldSaveMetadata& InMetadata);
	FOnSaveSlotSelectedNative OnSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WorldNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailsText;

private:
	UFUNCTION()
	void HandleClicked();

	FWorldSaveMetadata Metadata;
};
