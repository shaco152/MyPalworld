#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayMenuWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FOnGameplayMenuActionNative);

/**
 * 游戏世界内的 Esc 菜单。
 * 只负责 UMG 展示和按钮事件，存档、Session 清理与地图旅行由 PlayerController 协调。
 */
UCLASS()
class FINALPROJECT_API UGameplayMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FOnGameplayMenuActionNative OnResumeRequested;
	FOnGameplayMenuActionNative OnSaveRequested;
	FOnGameplayMenuActionNative OnOpenMultiplayerRequested;
	FOnGameplayMenuActionNative OnReturnToMainMenuRequested;

	void SetStatus(const FString& Message);
	void SetBusy(bool bBusy, const FString& Message);
	void SetCanOpenMultiplayer(bool bCanOpen);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ResumeGameButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SaveGameButton;

	/** 旧版 WBP 可暂时缺少；添加同名 Button 后自动启用，无需蓝图事件连线。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> OpenMultiplayerButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ReturnToMainMenuButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleOpenMultiplayerClicked();

	UFUNCTION()
	void HandleReturnToMainMenuClicked();
};
