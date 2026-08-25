#include "Framework/MainMenuGameMode.h"

#include "Framework/MainMenuPlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	PlayerControllerClass = AMainMenuPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}
