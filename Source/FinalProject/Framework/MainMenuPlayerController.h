#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

class UMainMenuWidget;

UCLASS()
class FINALPROJECT_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu")
	TSubclassOf<UMainMenuWidget> MainMenuWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UMainMenuWidget> MainMenuWidget;
};
