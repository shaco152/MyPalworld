#include "Framework/MainMenuPlayerController.h"

#include "UI/MainMenuWidget.h"
#include "Framework/FinalProjectGameInstance.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance()))
	{
		GameInstance->NotifyMainMenuReady();
	}
	if (!IsLocalController() || !MainMenuWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 主菜单未创建：Local=%d WidgetClass=%s"), IsLocalController(), *GetNameSafe(MainMenuWidgetClass));
		return;
	}
	MainMenuWidget = CreateWidget<UMainMenuWidget>(this, MainMenuWidgetClass);
	if (!MainMenuWidget)
	{
		return;
	}
	MainMenuWidget->AddToViewport();
	FInputModeUIOnly InputMode;
	// 根 UserWidget 默认不可聚焦；鼠标菜单无需强行 Focus，否则会产生 Non-Focusable 日志错误。
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}
