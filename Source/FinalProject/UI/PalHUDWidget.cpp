#include "PalHUDWidget.h"
#include "AbilitySystem/PlayerAttributeSet.h"
#include "Characters/PlayerCharacter.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Storage/PalStorageComponent.h"
#include "UI/PalSlotWidget.h"

void UPalHUDWidget::InitFromStorage(UPalStorageComponent* InStorage)
{
	// 只保存引用并绑定刷新委托（本函数可能被多次调用）。
	// 注意：PC 创建 HUD 的顺序是 AddToViewport → 本函数，首次 NativeConstruct 时 Storage 未就绪，
	// 所以绑定必须在这里补一次（BindStorage 幂等），槽配置在 NativeConstruct 每次重做
	Storage = InStorage;
	BindStorage();

	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalHUDWidget::InitFromStorage: 存储组件=%s, PartySlot0=%s, PlayerHPBar=%s, PlayerHPText=%s"),
		*GetNameSafe(Storage), *GetNameSafe(PartySlot0), *GetNameSafe(PlayerHPBar), *GetNameSafe(PlayerHPText));

	Refresh();
}

void UPalHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 每次 AddToViewport 重触发：控件树已按 WBP 模板重建（预摆槽是新实例），全量重配置 + 重绑委托
	ConfigurePartySlots();
	BindStorage();
	BindPlayerHealth();
	Refresh();
}

void UPalHUDWidget::NativeDestruct()
{
	UnbindStorage();
	UnbindPlayerHealth();
	PartySlots.Empty();

	Super::NativeDestruct();
}

void UPalHUDWidget::ConfigurePartySlots()
{
	PartySlots.Reset();
	PartySlots.Add(PartySlot0);
	PartySlots.Add(PartySlot1);
	PartySlots.Add(PartySlot2);
	PartySlots.Add(PartySlot3);
	PartySlots.Add(PartySlot4);

	if (!PartySlot0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] PalHUDWidget::ConfigurePartySlots: PartySlot0 未绑定！请检查 WBP_PalHUD 是否预摆了 PartySlot0-4（WBP_PalSlot 实例）"));
	}

	// DisplayOnly：不拖放、不拦截鼠标（与旧 OwnerBox 为空的行为一致）
	for (int32 i = 0; i < PartySlots.Num(); ++i)
	{
		if (UPalSlotWidget* PalSlot = PartySlots[i])
		{
			PalSlot->ConfigureSlot(true, i, EPalSlotInteractionMode::DisplayOnly);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalHUDWidget::ConfigurePartySlots: 预摆背包槽%d个"), PartySlots.Num());
}

void UPalHUDWidget::BindStorage()
{
	if (Storage)
	{
		Storage->OnStorageChanged.RemoveDynamic(this, &UPalHUDWidget::Refresh);
		Storage->OnStorageChanged.AddDynamic(this, &UPalHUDWidget::Refresh);
	}
}

void UPalHUDWidget::UnbindStorage()
{
	if (Storage)
	{
		Storage->OnStorageChanged.RemoveDynamic(this, &UPalHUDWidget::Refresh);
	}
}

void UPalHUDWidget::BindPlayerHealth()
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwningPlayerPawn());
	UPlayerAttributeSet* Set = Player ? Player->GetAttributeSet() : nullptr;
	if (!Set)
	{
		return;
	}

	// 换绑（OnPossess 重建等）：先解绑旧的
	if (BoundPlayerSet.IsValid() && BoundPlayerSet.Get() != Set)
	{
		BoundPlayerSet->OnHealthChanged.RemoveDynamic(this, &UPalHUDWidget::OnPlayerHealthChanged);
		BoundPlayerSet = nullptr;
	}

	if (!BoundPlayerSet.IsValid())
	{
		BoundPlayerSet = Set;
		Set->OnHealthChanged.AddDynamic(this, &UPalHUDWidget::OnPlayerHealthChanged);
	}

	// 初始填充一次（事件驱动：之后只在受伤/治疗结算时刷新）
	OnPlayerHealthChanged(Set->GetHealth(), Set->GetMaxHealth());
}

void UPalHUDWidget::UnbindPlayerHealth()
{
	if (BoundPlayerSet.IsValid())
	{
		BoundPlayerSet->OnHealthChanged.RemoveDynamic(this, &UPalHUDWidget::OnPlayerHealthChanged);
	}
	BoundPlayerSet = nullptr;
}

void UPalHUDWidget::OnPlayerHealthChanged(float Health, float MaxHealth)
{
	if (PlayerHPBar)
	{
		PlayerHPBar->SetPercent(MaxHealth > 0.f ? FMath::Clamp(Health, 0.f, MaxHealth) / MaxHealth : 0.f);
	}
	if (PlayerHPText)
	{
		PlayerHPText->SetText(FText::FromString(FString::Printf(TEXT("HP %.0f / %.0f"), Health, MaxHealth)));
	}
}

void UPalHUDWidget::Refresh()
{
	if (!Storage)
	{
		return;
	}

	for (int32 i = 0; i < PartySlots.Num(); ++i)
	{
		if (!PartySlots[i])
		{
			continue;
		}
		const FStoredPalInfo Info = Storage->PartyPals.IsValidIndex(i) ? Storage->PartyPals[i] : FStoredPalInfo();
		const bool bSummoned = Storage->HasSummonedPal() && Storage->GetSummonedPartyIndex() == i;
		PartySlots[i]->SetSlotData(Info, i == Storage->ActivePartyIndex, bSummoned);
	}
}
