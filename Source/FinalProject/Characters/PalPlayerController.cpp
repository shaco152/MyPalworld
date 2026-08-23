#include "PalPlayerController.h"
#include "AbilitySystem/CaptureTags.h"
#include "AbilitySystem/GA_PlayerAttack.h"
#include "AbilitySystem/GA_ThrowPalSphere.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Characters/PlayerCharacter.h"
#include "Combat/TurnBattleComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Storage/PalStorageComponent.h"
#include "UI/PalBoxWidget.h"
#include "UI/PalHUDWidget.h"

APalPlayerController::APalPlayerController()
{
}

void APalPlayerController::BeginPlay()
{
	Super::BeginPlay();

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
}

void APalPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] APalPlayerController::OnPossess: Pawn=%s"), *GetNameSafe(InPawn));
	CreateHUDIfNeeded();
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
	if (IsGameplayInputBlocked())
	{
		return; // 仓库打开/回合制战斗中冻结角色操作
	}

	ActivateAbilityByTag(CaptureTags::TAG_InputTag_Attack.GetTag(), UGA_PlayerAttack::StaticClass());
}

void APalPlayerController::OnTurnBattlePressed()
{
	if (bBoxOpen)
	{
		return; // 仓库打开时忽略
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
	if (bBoxOpen)
	{
		return true;
	}
	if (const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn()))
	{
		return PlayerPawn->IsInTurnBattle();
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
	if (IsGameplayInputBlocked())
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
	if (IsGameplayInputBlocked())
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
	if (bBoxOpen)
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
	if (bBoxOpen)
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
	if (bBoxOpen)
	{
		return;
	}

	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
	{
		if (UCharacterMovementComponent* MoveComp = ControlledCharacter->GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed = SprintSpeed;
		}
	}
}

void APalPlayerController::OnSprintReleased()
{
	if (bBoxOpen)
	{
		return;
	}

	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
	{
		if (UCharacterMovementComponent* MoveComp = ControlledCharacter->GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed = WalkSpeed;
		}
	}
}

UPalStorageComponent* APalPlayerController::GetPalStorage() const
{
	const APawn* ControlledPawn = GetPawn();
	return ControlledPawn ? ControlledPawn->FindComponentByClass<UPalStorageComponent>() : nullptr;
}

void APalPlayerController::CreateHUDIfNeeded()
{
	if (HUDWidget || !HUDWidgetClass)
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
	// Esc 只取消回合制切换子页面；仓库继续使用 E，主战斗页 Esc 不结束战斗。
	if (const APlayerCharacter* PlayerPawn = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UTurnBattleComponent* Battle = PlayerPawn->GetTurnBattleComponent(); Battle && Battle->IsSwitchPanelVisible())
		{
			Battle->CancelSwitchSelection();
		}
	}
}
