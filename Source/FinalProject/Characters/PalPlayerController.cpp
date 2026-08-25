#include "PalPlayerController.h"
#include "AbilitySystem/CaptureTags.h"
#include "AbilitySystem/GA_PlayerAttack.h"
#include "AbilitySystem/GA_ThrowPalSphere.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Characters/PlayerCharacter.h"
#include "Combat/TurnBattleComponent.h"
#include "Building/BuildingComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Storage/PalStorageComponent.h"
#include "Items/ItemInventoryComponent.h"
#include "Framework/PlayerDataLibrary.h"
#include "Framework/FinalProjectGameState.h"
#include "Framework/FinalProjectGameInstance.h"
#include "Online/PalSessionSubsystem.h"
#include "Persistence/SaveGameSubsystem.h"
#include "UI/BuildMenuWidget.h"
#include "UI/GameplayMenuWidget.h"
#include "UI/MaterialInventoryWidget.h"
#include "UI/PalBoxWidget.h"
#include "UI/PalHUDWidget.h"
#include "HAL/PlatformTime.h"

APalPlayerController::APalPlayerController()
{
}

void APalPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 从主菜单 OpenLevel 进入 Gameplay 时，视口可能仍保留 UIOnly；新 Gameplay PC 必须显式恢复游戏输入。
	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
		UE_LOG(LogTemp, Warning, TEXT("[诊断] Gameplay PlayerController 已恢复 GameOnly 输入模式"));
	}

	// 注册输入映射上下文（控制器级，不依赖 Pawn 的生成时机）
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 DefaultMappingContext！"));
		}
	}

	// 懒建背包 HUD（此时 Pawn 可能尚未生成，OnPossess 会兜底再试一次）
	CreateHUDIfNeeded();
	BindBuildingComponent();
}

void APalPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr)
	{
		Saves->OnSaveFinished.RemoveDynamic(this, &APalPlayerController::HandleManualSaveFinished);
		Saves->OnSaveFinished.RemoveDynamic(this, &APalPlayerController::HandleOpenMultiplayerSaveFinished);
	}
	if (UPalSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPalSessionSubsystem>() : nullptr)
	{
		Sessions->OnOperationFinished.RemoveDynamic(this, &APalPlayerController::HandleOpenMultiplayerSessionFinished);
	}
	if (GameplayMenuWidget)
	{
		GameplayMenuWidget->RemoveFromParent();
	}
	bGameplayMenuOpen = false;
	bManualSavePending = false;
	bOpenMultiplayerPending = false;
	Super::EndPlay(EndPlayReason);
}

void APalPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] APalPlayerController::OnPossess: Pawn=%s"), *GetNameSafe(InPawn));
	CreateHUDIfNeeded();
	BindBuildingComponent();
}

void APalPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent(); // 5.x 默认创建的就是 UEnhancedInputComponent（UInputSettings 默认类）

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] InputComponent 不是 EnhancedInputComponent！"));
		}
		return;
	}

	if (ThrowAction)
	{
		EIC->BindAction(ThrowAction, ETriggerEvent::Started, this, &APalPlayerController::OnThrowPressed);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 ThrowAction！"));
	}

	if (AttackAction)
	{
		EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &APalPlayerController::OnAttackPressed);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 AttackAction！"));
	}

	if (TurnBattleAction)
	{
		EIC->BindAction(TurnBattleAction, ETriggerEvent::Started, this, &APalPlayerController::OnTurnBattlePressed);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 TurnBattleAction！"));
	}

	// 回合制观战相机：右键按住 + 鼠标 XY 旋转（战斗中生效，控制器转发给战斗组件）
	if (BattleCameraHoldAction)
	{
		EIC->BindAction(BattleCameraHoldAction, ETriggerEvent::Started, this, &APalPlayerController::OnBattleCameraHoldPressed);
		EIC->BindAction(BattleCameraHoldAction, ETriggerEvent::Completed, this, &APalPlayerController::OnBattleCameraHoldReleased);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 BattleCameraHoldAction！"));
	}

	if (BattleCameraRotateAction)
	{
		EIC->BindAction(BattleCameraRotateAction, ETriggerEvent::Triggered, this, &APalPlayerController::OnBattleCameraRotate);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 BattleCameraRotateAction！"));
	}

	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APalPlayerController::OnMove);
	}

	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &APalPlayerController::OnLook);
	}

	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &APalPlayerController::OnJumpPressed);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &APalPlayerController::OnJumpReleased);
	}

	if (SprintAction)
	{
		EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &APalPlayerController::OnSprintPressed);
		EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &APalPlayerController::OnSprintReleased);
	}

	// 帕鲁管理按键：强化输入 IA（Started 每次按键只触发一次，避免旧式 BindKey 长按连发导致轮换过快/仓库反复开关）
	if (BoxAction)
	{
		EIC->BindAction(BoxAction, ETriggerEvent::Started, this, &APalPlayerController::OnToggleBox);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 BoxAction！"));
	}

	if (SummonAction)
	{
		EIC->BindAction(SummonAction, ETriggerEvent::Started, this, &APalPlayerController::OnSummonPressed);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 SummonAction！"));
	}

	if (PrevPalAction)
	{
		EIC->BindAction(PrevPalAction, ETriggerEvent::Started, this, &APalPlayerController::OnPartyPrev);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 PrevPalAction！"));
	}

	if (NextPalAction)
	{
		EIC->BindAction(NextPalAction, ETriggerEvent::Started, this, &APalPlayerController::OnPartyNext);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 NextPalAction！"));
	}

	if (UIBackAction)
	{
		EIC->BindAction(UIBackAction, ETriggerEvent::Started, this, &APalPlayerController::OnUIBackPressed);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 UIBackAction！请创建 IA_UIBack 并映射 Esc"));
	}

	if (MaterialInventoryAction)
	{
		EIC->BindAction(MaterialInventoryAction, ETriggerEvent::Started, this, &APalPlayerController::OnToggleMaterialInventory);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 MaterialInventoryAction（B 键）"));
	}

	if (BuildModeAction)
	{
		EIC->BindAction(BuildModeAction, ETriggerEvent::Started, this, &APalPlayerController::OnToggleBuildMode);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 BuildModeAction（C 键）"));
	}

	if (BuildRotateAction)
	{
		EIC->BindAction(BuildRotateAction, ETriggerEvent::Triggered, this, &APalPlayerController::OnBuildRotate);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 BuildRotateAction（鼠标滚轮 Axis1D）"));
	}
}

void APalPlayerController::OnThrowPressed()
{
	if (IsGameplayInputBlocked())
	{
		return; // 仓库打开/回合制战斗中冻结角色操作
	}

	ActivateAbilityByTag(CaptureTags::TAG_InputTag_Throw.GetTag(), UGA_ThrowPalSphere::StaticClass());
}

void APalPlayerController::OnAttackPressed()
{
	if (UBuildingComponent* Building = GetBuildingComponent(); Building && Building->HasActivePreview())
	{
		Building->ConfirmPlacement(); // 建造预览中复用左键确认，不触发普攻
		return;
	}
	if (IsGameplayInputBlocked())
	{
		return; // 仓库打开/回合制战斗中冻结角色操作
	}

	ActivateAbilityByTag(CaptureTags::TAG_InputTag_Attack.GetTag(), UGA_PlayerAttack::StaticClass());
}

void APalPlayerController::OnTurnBattlePressed()
{
	if (IsGameplayInputBlocked())
	{
		return; // 任一 UI / 建造模式打开时忽略
	}

	if (const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UTurnBattleComponent* Battle = PlayerPawn->GetTurnBattleComponent())
		{
			Battle->TryStartBattle();
		}
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("[诊断] P 键：没有玩家 Pawn！"));
	}
}

void APalPlayerController::OnBattleCameraHoldPressed()
{
	bBattleCameraHeld = true;
}

void APalPlayerController::OnBattleCameraHoldReleased()
{
	bBattleCameraHeld = false;
}

void APalPlayerController::OnBattleCameraRotate(const FInputActionValue& Value)
{
	// 仅战斗中 + 按住右键时旋转观战相机（输入走 PlayerController + IA，符合项目约定）
	if (!bBattleCameraHeld)
	{
		return;
	}
	if (const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UTurnBattleComponent* Battle = PlayerPawn->GetTurnBattleComponent())
		{
			if (Battle->IsActive())
			{
				Battle->RotateBattleCamera(Value.Get<FVector2D>());
			}
		}
	}
}

bool APalPlayerController::IsGameplayInputBlocked() const
{
	if (IsMovementInputBlocked())
	{
		return true;
	}
	if (const UBuildingComponent* Building = GetBuildingComponent())
	{
		return Building->IsBuildModeActive();
	}
	return false;
}

bool APalPlayerController::IsMovementInputBlocked() const
{
	if (const AFinalProjectGameState* ProjectGameState = GetWorld() ? GetWorld()->GetGameState<AFinalProjectGameState>() : nullptr;
		ProjectGameState && ProjectGameState->GetLoadState() != EWorldLoadState::Ready)
	{
		return true;
	}
	if (bBoxOpen || bMaterialInventoryOpen)
	{
		return true;
	}
	if (bGameplayMenuOpen)
	{
		return true;
	}
	if (const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn()))
	{
		if (PlayerPawn->IsDead() || PlayerPawn->IsInTurnBattle())
		{
			return true;
		}
	}
	if (const UBuildingComponent* Building = GetBuildingComponent())
	{
		return Building->GetBuildModeState() == EBuildModeState::CatalogOpen;
	}
	return false;
}

void APalPlayerController::ActivateAbilityByTag(const FGameplayTag& InputTag, const TSubclassOf<UGameplayAbility>& FallbackClass)
{
	// 转发给当前 Pawn 的 ASC：先按输入标签激活，失败按类兜底（不依赖 AbilityTags 继承）
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("[诊断] 按键（%s）按下，但没有 Pawn！"), *InputTag.ToString()));
		}
		return;
	}

	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ControlledPawn);
	if (!ASI || !ASI->GetAbilitySystemComponent())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("[诊断] Pawn[%s] 没有 ASC！"), *ControlledPawn->GetName()));
		}
		return;
	}

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();

	bool bSuccess = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(InputTag));
	if (!bSuccess && FallbackClass)
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA(FallbackClass))
			{
				bSuccess = ASC->TryActivateAbility(Spec.Handle);
				break;
			}
		}
	}

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] ActivateAbilityByTag: 标签 %s 激活失败，已授予能力数=%d"), *InputTag.ToString(), ASC->GetActivatableAbilities().Num());
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
				FString::Printf(TEXT("[诊断] 能力激活失败（查找标签: %s）"), *InputTag.ToString()));
		}
	}
}

void APalPlayerController::OnMove(const FInputActionValue& Value)
{
	if (IsMovementInputBlocked())
	{
		return; // 仓库打开/回合制战斗中冻结角色操作
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	// 以控制器偏航为基准的前/右方向（X=左右, Y=前后）
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	ControlledPawn->AddMovementInput(ForwardDir, Axis.Y);
	ControlledPawn->AddMovementInput(RightDir, Axis.X);
}

void APalPlayerController::OnLook(const FInputActionValue& Value)
{
	if (IsMovementInputBlocked())
	{
		return; // 仓库打开/回合制战斗中冻结角色操作
	}

	// 视角转动：X 偏航 / Y 俯仰（角色与弹簧臂都跟随 PawnControlRotation）
	const FVector2D Axis = Value.Get<FVector2D>();
	AddYawInput(Axis.X);
	AddPitchInput(Axis.Y);
}

void APalPlayerController::OnJumpPressed()
{
	if (IsMovementInputBlocked())
	{
		return;
	}

	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
	{
		ControlledCharacter->Jump();
	}
}

void APalPlayerController::OnJumpReleased()
{
	if (IsMovementInputBlocked())
	{
		return;
	}

	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
	{
		ControlledCharacter->StopJumping();
	}
}

void APalPlayerController::OnSprintPressed()
{
	if (IsMovementInputBlocked())
	{
		return;
	}

	ApplySprintSpeed(true);
	if (!HasAuthority())
	{
		ServerSetSprinting(true);
	}
}

void APalPlayerController::OnSprintReleased()
{
	// 松开必须无条件恢复；若打开 UI/进入战斗导致输入被阻断，不能把冲刺速度遗留在角色上。
	ApplySprintSpeed(false);
	if (!HasAuthority())
	{
		ServerSetSprinting(false);
	}
}

void APalPlayerController::ApplySprintSpeed(bool bSprinting)
{
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
	{
		if (UCharacterMovementComponent* MoveComp = ControlledCharacter->GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
		}
	}
}

void APalPlayerController::ServerSetSprinting_Implementation(bool bSprinting)
{
	// 客户端只提交布尔意图，速度值始终取服务端 BP_PC 配置，避免客户端任意传速。
	ApplySprintSpeed(bSprinting && !IsMovementInputBlocked());
	const ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn());
	const UCharacterMovementComponent* MoveComp = ControlledCharacter ? ControlledCharacter->GetCharacterMovement() : nullptr;
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 服务端同步冲刺: Controller=%s Requested=%d AppliedSpeed=%.0f"),
		*GetName(), static_cast<int32>(bSprinting), MoveComp ? MoveComp->MaxWalkSpeed : 0.f);
}

UPalStorageComponent* APalPlayerController::GetPalStorage() const
{
	return UPlayerDataLibrary::ResolvePalStorage(this);
}

void APalPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	CreateHUDIfNeeded();
	BindBuildingComponent();
	if (UBuildingComponent* Building = GetBuildingComponent())
	{
		Building->RefreshDataSource();
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PlayerController 收到 PlayerState：%s"), *GetNameSafe(PlayerState.Get()));
}

UItemInventoryComponent* APalPlayerController::GetItemInventory() const
{
	return UPlayerDataLibrary::ResolveItemInventory(this);
}

UBuildingComponent* APalPlayerController::GetBuildingComponent() const
{
	const APawn* ControlledPawn = GetPawn();
	return ControlledPawn ? ControlledPawn->FindComponentByClass<UBuildingComponent>() : nullptr;
}

void APalPlayerController::BindBuildingComponent()
{
	if (!IsLocalController())
	{
		return;
	}
	if (UBuildingComponent* Building = GetBuildingComponent())
	{
		Building->OnBuildModeStateChanged.RemoveDynamic(this, &APalPlayerController::OnBuildModeStateChanged);
		Building->OnBuildModeStateChanged.AddDynamic(this, &APalPlayerController::OnBuildModeStateChanged);
	}
}

void APalPlayerController::CreateHUDIfNeeded()
{
	if (!IsLocalController() || HUDWidget || !HUDWidgetClass)
	{
		return; // 已创建 / 未配置
	}

	UPalStorageComponent* Storage = GetPalStorage();
	if (!Storage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] CreateHUDIfNeeded: Pawn 未就绪或无存储组件，等 OnPossess 再试"));
		return;
	}

	HUDWidget = CreateWidget<UPalHUDWidget>(this, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(0);
		HUDWidget->InitFromStorage(Storage);
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 背包 HUD 已创建: %s"), *HUDWidget->GetName());
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[诊断] HUD 创建失败！请检查 BP_PC 的 HUDWidgetClass"));
	}
}

void APalPlayerController::SetPersistentHUDVisible(bool bVisible)
{
	if (!IsLocalController())
	{
		return;
	}
	if (bVisible)
	{
		CreateHUDIfNeeded();
	}
	if (HUDWidget)
	{
		HUDWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 常驻 PalHUD 已%s"), bVisible ? TEXT("显示") : TEXT("隐藏"));
	}
}

void APalPlayerController::OnToggleBox()
{
	if (bGameplayMenuOpen)
	{
		return;
	}
	if (!bBoxOpen)
	{
		const UBuildingComponent* Building = GetBuildingComponent();
		if (bMaterialInventoryOpen || (Building && Building->IsBuildModeActive()))
		{
			return;
		}
	}
	// 回合制战斗中不能开仓库（关仓库的 E 不受影响）
	if (const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn()))
	{
		if (PlayerPawn->IsInTurnBattle())
		{
			return;
		}
	}

	if (!bBoxOpen)
	{
		// 打开仓库
		UPalStorageComponent* Storage = GetPalStorage();
		if (!Storage)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("[诊断] Pawn 上没有存储组件，无法打开仓库！"));
			}
			return;
		}
		if (!BoxWidgetClass)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("[诊断] BP_PC 未设置 BoxWidgetClass！"));
			}
			return;
		}

		if (!BoxWidget)
		{
			BoxWidget = CreateWidget<UPalBoxWidget>(this, BoxWidgetClass);
			if (BoxWidget)
			{
				BoxWidget->InitFromStorage(Storage);
			}
		}
		if (!BoxWidget)
		{
			return;
		}

		BoxWidget->AddToViewport(0);
		bBoxOpen = true;

		// 仓库页隐藏常驻 HUD：底部背包槽与仓库页背包槽重复，玩家血量等信息在仓库页无效
		SetPersistentHUDVisible(false);

		// 显示鼠标并聚焦仓库界面；移动/视角/投掷/召唤/切换由 bBoxOpen 守卫冻结
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(BoxWidget->TakeWidget());
		// 关键：不锁定鼠标到视口 + 捕获期间不隐藏光标，否则游戏视口捕获鼠标时光标不可见（看不到鼠标）
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		SetShowMouseCursor(true);

		UE_LOG(LogTemp, Warning, TEXT("[诊断] 仓库界面已打开（拖放交换，按 E 关闭）"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("仓库已打开：拖放交换背包与仓库，按 E 关闭"));
		}
	}
	else
	{
		// 关闭仓库
		if (BoxWidget)
		{
			BoxWidget->RemoveFromParent();
		}
		bBoxOpen = false;
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);

		// 关闭仓库恢复常驻 HUD
		SetPersistentHUDVisible(true);

		UE_LOG(LogTemp, Warning, TEXT("[诊断] 仓库界面已关闭，恢复 GameOnly 输入"));
	}
}

void APalPlayerController::OnSummonPressed()
{
	// 回合制切换页优先复用 F：确认选择；主战斗页仍由战斗守卫冻结召唤。
	if (const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UTurnBattleComponent* Battle = PlayerPawn->GetTurnBattleComponent(); Battle && Battle->IsSwitchPanelVisible())
		{
			Battle->ConfirmSwitchSelection();
			return;
		}
	}

	if (IsGameplayInputBlocked())
	{
		return; // 仓库打开/回合制战斗中忽略
	}

	UPalStorageComponent* Storage = GetPalStorage();
	if (!Storage)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("[诊断] Pawn 上没有存储组件，无法召唤！"));
		}
		return;
	}

	Storage->SummonOrRecallActivePal(); // 召唤/收回切换式：空槽提示、收回、先收再换均在组件内部处理
}

void APalPlayerController::OnPartyPrev()
{
	if (const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UTurnBattleComponent* Battle = PlayerPawn->GetTurnBattleComponent(); Battle && Battle->IsSwitchPanelVisible())
		{
			Battle->NavigateSwitchSelection(-1);
			return;
		}
	}

	if (IsGameplayInputBlocked())
	{
		return; // 仓库打开时用拖放切换 / 战斗中不可切换
	}

	if (UPalStorageComponent* Storage = GetPalStorage())
	{
		Storage->CycleActiveIndex(-1);
	}
}

void APalPlayerController::OnPartyNext()
{
	if (const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UTurnBattleComponent* Battle = PlayerPawn->GetTurnBattleComponent(); Battle && Battle->IsSwitchPanelVisible())
		{
			Battle->NavigateSwitchSelection(1);
			return;
		}
	}

	if (IsGameplayInputBlocked())
	{
		return; // 与 Prev 统一：仓库打开或回合制主页面均不可偷偷切换 ActivePartyIndex
	}

	if (UPalStorageComponent* Storage = GetPalStorage())
	{
		Storage->CycleActiveIndex(1);
	}
}

void APalPlayerController::OnUIBackPressed()
{
	// Esc 按优先级关闭当前子界面；没有子界面时打开游戏菜单。
	if (bGameplayMenuOpen)
	{
		CloseGameplayMenu();
		return;
	}
	if (UBuildingComponent* Building = GetBuildingComponent(); Building && Building->IsBuildModeActive())
	{
		Building->ExitBuildMode();
		return;
	}
	if (bMaterialInventoryOpen)
	{
		OnToggleMaterialInventory();
		return;
	}
	if (bBoxOpen)
	{
		OnToggleBox();
		return;
	}
	if (const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UTurnBattleComponent* Battle = PlayerPawn->GetTurnBattleComponent(); Battle && Battle->IsSwitchPanelVisible())
		{
			Battle->CancelSwitchSelection();
			return;
		}
	}
	OpenGameplayMenu();
}

void APalPlayerController::OnToggleMaterialInventory()
{
	if (bGameplayMenuOpen)
	{
		return;
	}
	if (!bMaterialInventoryOpen)
	{
		const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn());
		const UBuildingComponent* Building = GetBuildingComponent();
		if (bBoxOpen || (PlayerPawn && PlayerPawn->IsInTurnBattle()) || (Building && Building->IsBuildModeActive()))
		{
			return;
		}
		UItemInventoryComponent* Inventory = GetItemInventory();
		if (!Inventory || !MaterialInventoryWidgetClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 打开材料背包失败: Inventory=%s WidgetClass=%s"),
				*GetNameSafe(Inventory), *GetNameSafe(MaterialInventoryWidgetClass));
			return;
		}

		if (!MaterialInventoryWidget)
		{
			MaterialInventoryWidget = CreateWidget<UMaterialInventoryWidget>(this, MaterialInventoryWidgetClass);
		}
		if (!MaterialInventoryWidget)
		{
			return;
		}
		MaterialInventoryWidget->InitFromInventory(Inventory);
		MaterialInventoryWidget->AddToViewport(10);
		bMaterialInventoryOpen = true;
		SetPersistentHUDVisible(false);

		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(MaterialInventoryWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 材料背包已打开（B/Esc 关闭）"));
	}
	else
	{
		if (MaterialInventoryWidget)
		{
			MaterialInventoryWidget->RemoveFromParent();
		}
		bMaterialInventoryOpen = false;
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		SetPersistentHUDVisible(true);
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 材料背包已关闭"));
	}
}

void APalPlayerController::OnToggleBuildMode()
{
	if (bGameplayMenuOpen)
	{
		return;
	}
	UBuildingComponent* Building = GetBuildingComponent();
	if (!Building)
	{
		return;
	}
	if (Building->IsBuildModeActive())
	{
		Building->ExitBuildMode();
		return;
	}

	const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn());
	if (bBoxOpen || bMaterialInventoryOpen || (PlayerPawn && PlayerPawn->IsInTurnBattle()))
	{
		return;
	}
	Building->EnterBuildMode();
}

void APalPlayerController::OnBuildRotate(const FInputActionValue& Value)
{
	if (UBuildingComponent* Building = GetBuildingComponent())
	{
		Building->RotatePreview(Value.Get<float>());
	}
}

void APalPlayerController::OnBuildModeStateChanged(EBuildModeState NewState)
{
	if (!IsLocalController())
	{
		return;
	}
	if (NewState == EBuildModeState::CatalogOpen)
	{
		UBuildingComponent* Building = GetBuildingComponent();
		if (!Building || !BuildMenuWidgetClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 建造目录 UI 未配置: Component=%s WidgetClass=%s"),
				*GetNameSafe(Building), *GetNameSafe(BuildMenuWidgetClass));
			if (Building)
			{
				Building->ExitBuildMode();
			}
			return;
		}
		if (!BuildMenuWidget)
		{
			BuildMenuWidget = CreateWidget<UBuildMenuWidget>(this, BuildMenuWidgetClass);
		}
		if (!BuildMenuWidget)
		{
			Building->ExitBuildMode();
			return;
		}
		BuildMenuWidget->InitFromBuildingComponent(Building);
		BuildMenuWidget->AddToViewport(10);
		SetPersistentHUDVisible(false);

		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(BuildMenuWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
		return;
	}

	if (BuildMenuWidget)
	{
		BuildMenuWidget->RemoveFromParent();
	}
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
	if (NewState == EBuildModeState::Inactive)
	{
		SetPersistentHUDVisible(true);
	}
	else
	{
		SetPersistentHUDVisible(false); // Previewing：保留干净的建造视野
	}
}

void APalPlayerController::OpenGameplayMenu()
{
	if (!IsLocalController() || bGameplayMenuOpen)
	{
		return;
	}
	if (!GameplayMenuWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[诊断] Esc 游戏菜单未配置：请在 BP_PlayerController 设置 GameplayMenuWidgetClass"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red,
				TEXT("[诊断] BP_PlayerController 未设置 GameplayMenuWidgetClass"));
		}
		return;
	}
	if (!GameplayMenuWidget)
	{
		GameplayMenuWidget = CreateWidget<UGameplayMenuWidget>(this, GameplayMenuWidgetClass);
	}
	if (!GameplayMenuWidget)
	{
		return;
	}

	GameplayMenuWidget->OnResumeRequested.RemoveAll(this);
	GameplayMenuWidget->OnSaveRequested.RemoveAll(this);
	GameplayMenuWidget->OnOpenMultiplayerRequested.RemoveAll(this);
	GameplayMenuWidget->OnReturnToMainMenuRequested.RemoveAll(this);
	GameplayMenuWidget->OnResumeRequested.AddUObject(this, &APalPlayerController::CloseGameplayMenu);
	GameplayMenuWidget->OnSaveRequested.AddUObject(this, &APalPlayerController::HandleManualSaveRequested);
	GameplayMenuWidget->OnOpenMultiplayerRequested.AddUObject(this, &APalPlayerController::HandleOpenMultiplayerRequested);
	GameplayMenuWidget->OnReturnToMainMenuRequested.AddUObject(this, &APalPlayerController::HandleReturnToMainMenuRequested);
	GameplayMenuWidget->AddToViewport(100);
	bGameplayMenuOpen = true;
	GameplayMenuWidget->SetCanOpenMultiplayer(GetNetMode() == NM_Standalone);
	const bool bMenuBusy = bManualSavePending || bOpenMultiplayerPending;
	GameplayMenuWidget->SetBusy(bMenuBusy,
		bOpenMultiplayerPending ? TEXT("正在开放当前世界联机...") :
		(bManualSavePending ? TEXT("正在保存世界...") : TEXT("游戏菜单")));
	SetPersistentHUDVisible(false);

	FInputModeGameAndUI Mode;
	// 根 UserWidget 通常不可聚焦；鼠标菜单无需强制 Focus，避免 Non-Focusable 输入错误。
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] Esc 游戏菜单已打开"));
}

void APalPlayerController::CloseGameplayMenu()
{
	if (!bGameplayMenuOpen)
	{
		return;
	}
	if (GameplayMenuWidget)
	{
		GameplayMenuWidget->RemoveFromParent();
	}
	bGameplayMenuOpen = false;
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);

	bool bShouldShowHUD = true;
	if (const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn()))
	{
		bShouldShowHUD = !PlayerPawn->IsInTurnBattle();
	}
	if (const UBuildingComponent* Building = GetBuildingComponent(); Building && Building->IsBuildModeActive())
	{
		bShouldShowHUD = false;
	}
	SetPersistentHUDVisible(bShouldShowHUD);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] Esc 游戏菜单已关闭，恢复 GameOnly 输入"));
}

void APalPlayerController::HandleManualSaveRequested()
{
	if (!bGameplayMenuOpen || bManualSavePending || bOpenMultiplayerPending)
	{
		return;
	}
	bManualSavePending = true;
	if (GameplayMenuWidget)
	{
		GameplayMenuWidget->SetBusy(true, GetNetMode() == NM_Client
			? TEXT("正在请求主机保存世界...") : TEXT("正在保存世界..."));
	}
	if (HasAuthority())
	{
		BeginManualSaveOnServer();
	}
	else
	{
		ServerRequestManualSave();
	}
}

void APalPlayerController::HandleOpenMultiplayerRequested()
{
	if (!bGameplayMenuOpen || bManualSavePending || bOpenMultiplayerPending)
	{
		return;
	}
	if (!HasAuthority() || GetNetMode() != NM_Standalone)
	{
		if (GameplayMenuWidget)
		{
			GameplayMenuWidget->SetBusy(false, TEXT("只有 Standalone 主机可以开放当前世界联机"));
		}
		return;
	}

	USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr;
	FString FailureReason;
	if (!Saves || !Saves->CanSaveActiveWorld(this, FailureReason))
	{
		if (GameplayMenuWidget)
		{
			GameplayMenuWidget->SetBusy(false, Saves ? FailureReason : TEXT("存档子系统不可用"));
		}
		return;
	}

	bOpenMultiplayerPending = true;
	if (GameplayMenuWidget)
	{
		GameplayMenuWidget->SetBusy(true, TEXT("正在保存当前世界并创建联机房间..."));
	}
	Saves->OnSaveFinished.AddUniqueDynamic(this, &APalPlayerController::HandleOpenMultiplayerSaveFinished);
	if (!Saves->SaveActiveWorld(this) && bOpenMultiplayerPending)
	{
		Saves->OnSaveFinished.RemoveDynamic(this, &APalPlayerController::HandleOpenMultiplayerSaveFinished);
		bOpenMultiplayerPending = false;
		if (GameplayMenuWidget)
		{
			GameplayMenuWidget->SetBusy(false, TEXT("开放联机前保存失败，当前世界保持 Standalone"));
		}
	}
}

void APalPlayerController::HandleOpenMultiplayerSaveFinished(bool bSuccess, const FString& Message)
{
	if (!bOpenMultiplayerPending)
	{
		return;
	}
	USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr;
	if (Saves)
	{
		Saves->OnSaveFinished.RemoveDynamic(this, &APalPlayerController::HandleOpenMultiplayerSaveFinished);
	}
	if (!bSuccess || !Saves || !Saves->GetActiveSave())
	{
		bOpenMultiplayerPending = false;
		if (GameplayMenuWidget)
		{
			GameplayMenuWidget->SetBusy(false, bSuccess ? TEXT("活动世界档无效") : Message);
		}
		return;
	}

	UPalSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UPalSessionSubsystem>();
	if (!Sessions)
	{
		bOpenMultiplayerPending = false;
		if (GameplayMenuWidget)
		{
			GameplayMenuWidget->SetBusy(false, TEXT("Session 子系统不可用"));
		}
		return;
	}
	Sessions->OnOperationFinished.RemoveDynamic(this, &APalPlayerController::HandleOpenMultiplayerSessionFinished);
	Sessions->OnOperationFinished.AddUniqueDynamic(this, &APalPlayerController::HandleOpenMultiplayerSessionFinished);
	Sessions->CreateRoomFromCurrentWorld(Saves->GetActiveSave()->Metadata.DisplayName, OpenWorldMaxPlayers);
}

void APalPlayerController::HandleOpenMultiplayerSessionFinished(bool bSuccess, const FString& Message)
{
	if (!bOpenMultiplayerPending)
	{
		return;
	}
	if (UPalSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPalSessionSubsystem>() : nullptr)
	{
		Sessions->OnOperationFinished.RemoveDynamic(this, &APalPlayerController::HandleOpenMultiplayerSessionFinished);
	}
	if (!bSuccess)
	{
		bOpenMultiplayerPending = false;
	}
	if (GameplayMenuWidget && bGameplayMenuOpen)
	{
		GameplayMenuWidget->SetBusy(bSuccess, Message);
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 当前世界开放联机：Success=%d Message=%s"), bSuccess, *Message);
}

void APalPlayerController::ServerRequestManualSave_Implementation()
{
	BeginManualSaveOnServer();
}

void APalPlayerController::BeginManualSaveOnServer()
{
	if (!HasAuthority())
	{
		DeliverManualSaveResult(false, TEXT("手动存档请求没有到达主机"));
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - LastManualSaveRequestRealTime < 2.0)
	{
		DeliverManualSaveResult(false, TEXT("存档请求过于频繁，请稍后再试"));
		return;
	}
	LastManualSaveRequestRealTime = Now;

	USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr;
	if (!Saves)
	{
		DeliverManualSaveResult(false, TEXT("存档子系统不可用"));
		return;
	}
	FString FailureReason;
	if (!Saves->CanSaveActiveWorld(this, FailureReason))
	{
		DeliverManualSaveResult(false, FailureReason);
		return;
	}

	bManualSavePending = true;
	Saves->OnSaveFinished.AddUniqueDynamic(this, &APalPlayerController::HandleManualSaveFinished);
	if (!Saves->SaveActiveWorld(this) && bManualSavePending)
	{
		Saves->OnSaveFinished.RemoveDynamic(this, &APalPlayerController::HandleManualSaveFinished);
		bManualSavePending = false;
		DeliverManualSaveResult(false, TEXT("世界快照创建失败，上一份存档仍然安全"));
	}
}

void APalPlayerController::HandleManualSaveFinished(bool bSuccess, const FString& Message)
{
	if (!bManualSavePending)
	{
		return;
	}
	if (USaveGameSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveGameSubsystem>() : nullptr)
	{
		Saves->OnSaveFinished.RemoveDynamic(this, &APalPlayerController::HandleManualSaveFinished);
	}
	bManualSavePending = false;
	DeliverManualSaveResult(bSuccess, bSuccess ? TEXT("手动存档完成") : Message);
}

void APalPlayerController::DeliverManualSaveResult(bool bSuccess, const FString& Message)
{
	bManualSavePending = false;
	if (IsLocalController())
	{
		ApplyManualSaveResult(bSuccess, Message);
	}
	else
	{
		ClientManualSaveResult(bSuccess, Message);
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 手动存档结果：Success=%d Message=%s"),
		bSuccess, *Message);
}

void APalPlayerController::ClientManualSaveResult_Implementation(bool bSuccess, const FString& Message)
{
	ApplyManualSaveResult(bSuccess, Message);
}

void APalPlayerController::ApplyManualSaveResult(bool bSuccess, const FString& Message)
{
	bManualSavePending = false;
	if (GameplayMenuWidget && bGameplayMenuOpen)
	{
		GameplayMenuWidget->SetBusy(false, Message);
	}
}

void APalPlayerController::HandleReturnToMainMenuRequested()
{
	if (!bGameplayMenuOpen || bManualSavePending || bOpenMultiplayerPending)
	{
		return;
	}
	if (GameplayMenuWidget)
	{
		GameplayMenuWidget->SetBusy(true, TEXT("正在返回主界面..."));
	}
	if (UPalSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPalSessionSubsystem>() : nullptr)
	{
		if (Sessions->LeaveToMainMenu())
		{
			return;
		}
	}
	if (UFinalProjectGameInstance* GameInstance = Cast<UFinalProjectGameInstance>(GetGameInstance()))
	{
		GameInstance->ReturnToMainMenu();
		return;
	}
	if (GameplayMenuWidget)
	{
		GameplayMenuWidget->SetBusy(false, TEXT("返回主界面失败：GameInstance 配置无效"));
	}
}
