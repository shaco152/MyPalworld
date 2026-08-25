#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "Building/BuildingComponent.h"
#include "PalPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UPalHUDWidget;
class UPalBoxWidget;
class UPalStorageComponent;
class UGameplayAbility;
class UItemInventoryComponent;
class UMaterialInventoryWidget;
class UBuildMenuWidget;
class UGameplayMenuWidget;

/**
 * 玩家控制器：负责 EnhancedInput 的绑定（官方 GAS 输入推荐挂点）。
 * 按 IA_Throw → 转发给当前 Pawn 的 ASC 激活 InputTag.Throw 能力。
 * 帕鲁管理：E 开关仓库界面 / F 召唤当前帕鲁 / 左右方向键切换当前背包帕鲁（强化输入 IA 资产，Started 触发避免长按连发）。
 */
UCLASS()
class FINALPROJECT_API APalPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APalPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_PlayerState() override;

	// 回合制战斗期间隐藏常驻背包/玩家血量 HUD，战斗结束恢复。
	void SetPersistentHUDVisible(bool bVisible);

protected:
	// 输入资产在 BP_PalPlayerController 里设置（C++ 无法创建资产）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ThrowAction;

	// 玩家普攻（鼠标左键，BP_PC 里设置；Q 键是投球）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	// 回合制战斗（P 键，BP_PC 里设置）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> TurnBattleAction;

	// 回合制观战相机旋转：右键按住（Digital）+ 鼠标 XY（Axis2D），战斗中生效
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> BattleCameraHoldAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> BattleCameraRotateAction;

	// 移动（WASD，Axis2D）与视角（鼠标 XY，Axis2D）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// 跳跃 / 冲刺（Digital bool，按下触发、松开恢复）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// 冲刺速度与常态行走速度（蓝图中可调）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 600.f;

	// 常驻背包 HUD 与仓库界面（E 键）的控件类，BP_PC 里设置
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalUI")
	TSubclassOf<UPalHUDWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalUI")
	TSubclassOf<UPalBoxWidget> BoxWidgetClass;

	// 帕鲁管理输入资产（BP_PC 里设置，IMC_Player 里映射：E 仓库 / F 召唤 / 左右方向键切换当前帕鲁）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalUI")
	TObjectPtr<UInputAction> BoxAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalUI")
	TObjectPtr<UInputAction> SummonAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalUI")
	TObjectPtr<UInputAction> PrevPalAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalUI")
	TObjectPtr<UInputAction> NextPalAction;

	// IA_UIBack（IMC 映射 Esc）：优先关闭子页面，无子页面时开关游戏菜单。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalUI")
	TObjectPtr<UInputAction> UIBackAction;

	// 材料背包/建造输入：B 开材料背包、C 切建造模式、滚轮旋转虚影
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BuildUI|Input")
	TObjectPtr<UInputAction> MaterialInventoryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BuildUI|Input")
	TObjectPtr<UInputAction> BuildModeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BuildUI|Input")
	TObjectPtr<UInputAction> BuildRotateAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BuildUI")
	TSubclassOf<UMaterialInventoryWidget> MaterialInventoryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BuildUI")
	TSubclassOf<UBuildMenuWidget> BuildMenuWidgetClass;

	/** Esc 游戏菜单；复用 UIBackAction，不需要新增 IA。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameplayMenu")
	TSubclassOf<UGameplayMenuWidget> GameplayMenuWidgetClass;

	/** 从当前世界开放 Listen Session 时允许的总玩家数（含主机）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameplayMenu|Online", meta = (ClampMin = "2", ClampMax = "16"))
	int32 OpenWorldMaxPlayers = 4;

private:
	// 按输入标签激活当前 Pawn 的能力（投掷/攻击共用）；失败按类兜底
	void ActivateAbilityByTag(const FGameplayTag& InputTag, const TSubclassOf<UGameplayAbility>& FallbackClass);

	void OnThrowPressed();
	void OnAttackPressed();
	void OnTurnBattlePressed();
	void OnBattleCameraHoldPressed();
	void OnBattleCameraHoldReleased();
	void OnBattleCameraRotate(const FInputActionValue& Value);
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpPressed();
	void OnJumpReleased();
	void OnSprintPressed();
	void OnSprintReleased();
	void ApplySprintSpeed(bool bSprinting);

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bSprinting);

	// 帕鲁管理：E 仓库开关 / F 召唤当前帕鲁 / 左右方向键切换当前背包帕鲁
	void OnToggleBox();
	void OnSummonPressed();
	void OnPartyPrev();
	void OnPartyNext();
	void OnUIBackPressed();
	void OnToggleMaterialInventory();
	void OnToggleBuildMode();
	void OnBuildRotate(const FInputActionValue& Value);
	void OpenGameplayMenu();
	void CloseGameplayMenu();
	void HandleManualSaveRequested();
	void HandleOpenMultiplayerRequested();
	void HandleReturnToMainMenuRequested();
	void BeginManualSaveOnServer();
	void DeliverManualSaveResult(bool bSuccess, const FString& Message);
	void ApplyManualSaveResult(bool bSuccess, const FString& Message);

	UFUNCTION(Server, Reliable)
	void ServerRequestManualSave();

	UFUNCTION(Client, Reliable)
	void ClientManualSaveResult(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleManualSaveFinished(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleOpenMultiplayerSaveFinished(bool bSuccess, const FString& Message);

	UFUNCTION()
	void HandleOpenMultiplayerSessionFinished(bool bSuccess, const FString& Message);

	UFUNCTION()
	void OnBuildModeStateChanged(EBuildModeState NewState);

	// 懒建 HUD（BeginPlay 与 OnPossess 都调用，幂等；WorldPartition 地图下二者先后不定）
	void CreateHUDIfNeeded();

	// 取当前 Pawn 上的存储组件（可能为空）
	UPalStorageComponent* GetPalStorage() const;
	UItemInventoryComponent* GetItemInventory() const;
	UBuildingComponent* GetBuildingComponent() const;
	void BindBuildingComponent();

	// 玩法输入是否应冻结（仓库打开 / 回合制战斗中）
	bool IsGameplayInputBlocked() const;
	bool IsMovementInputBlocked() const;

	UPROPERTY()
	TObjectPtr<UPalHUDWidget> HUDWidget;

	UPROPERTY()
	TObjectPtr<UPalBoxWidget> BoxWidget;

	UPROPERTY()
	TObjectPtr<UMaterialInventoryWidget> MaterialInventoryWidget;

	UPROPERTY()
	TObjectPtr<UBuildMenuWidget> BuildMenuWidget;

	UPROPERTY()
	TObjectPtr<UGameplayMenuWidget> GameplayMenuWidget;

	bool bBoxOpen = false;
	bool bMaterialInventoryOpen = false;
	bool bGameplayMenuOpen = false;
	bool bManualSavePending = false;
	bool bOpenMultiplayerPending = false;
	double LastManualSaveRequestRealTime = -1000.0;

	// 观战相机旋转是否按住右键中
	bool bBattleCameraHeld = false;
};
