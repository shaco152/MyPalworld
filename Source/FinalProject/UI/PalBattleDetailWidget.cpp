#include "PalBattleDetailWidget.h"

#include "AbilitySystem/PalAttributeSet.h"
#include "Characters/PalCharacter.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Storage/PalStorageComponent.h"

UPalBattleDetailWidget::UPalBattleDetailWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPalBattleDetailWidget::UpdateFromPal(const APalCharacter* Pal)
{
	if (!Pal)
	{
		Clear();
		return;
	}

	if (NameText)
	{
		NameText->SetText(FText::FromString(Pal->GetClass()->GetName()));
	}
	SetPortrait(Pal->PortraitIcon);

	const UPalAttributeSet* Set = Pal->GetAttributeSet();
	if (!Set)
	{
		if (LevelText) LevelText->SetText(FText::FromString(TEXT("-")));
		if (HPBar) HPBar->SetPercent(0.f);
		if (HPText) HPText->SetText(FText::FromString(TEXT("HP -")));
		if (MPBar) MPBar->SetPercent(0.f);
		if (MPText) MPText->SetText(FText::FromString(TEXT("MP -")));
		return;
	}

	if (LevelText)
	{
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%.0f"), Set->GetLevel())));
	}
	if (HPBar)
	{
		HPBar->SetPercent(Set->GetMaxHealth() > 0.f ? Set->GetHealth() / Set->GetMaxHealth() : 0.f);
	}
	if (HPText)
	{
		HPText->SetText(FText::FromString(FString::Printf(TEXT("HP %.0f / %.0f"), Set->GetHealth(), Set->GetMaxHealth())));
	}
	if (MPBar)
	{
		MPBar->SetPercent(Set->GetMaxMP() > 0.f ? Set->GetMP() / Set->GetMaxMP() : 0.f);
	}
	if (MPText)
	{
		MPText->SetText(FText::FromString(FString::Printf(TEXT("MP %.0f / %.0f"), Set->GetMP(), Set->GetMaxMP())));
	}
}

void UPalBattleDetailWidget::UpdateFromStoredInfo(const FStoredPalInfo& Info)
{
	if (!Info.IsValid())
	{
		Clear();
		return;
	}

	if (NameText)
	{
		NameText->SetText(FText::FromString(Info.PalClass->GetName()));
	}
	SetPortrait(Info.Icon);
	if (LevelText)
	{
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%.0f"), Info.Level)));
	}
	if (HPBar)
	{
		HPBar->SetPercent(Info.MaxHealth > 0.f ? Info.Health / Info.MaxHealth : 0.f);
	}
	if (HPText)
	{
		HPText->SetText(FText::FromString(FString::Printf(TEXT("HP %.0f / %.0f"), Info.Health, Info.MaxHealth)));
	}
	if (MPBar)
	{
		MPBar->SetPercent(Info.MaxMP > 0.f ? Info.MP / Info.MaxMP : 0.f);
	}
	if (MPText)
	{
		MPText->SetText(FText::FromString(FString::Printf(TEXT("MP %.0f / %.0f"), Info.MP, Info.MaxMP)));
	}
}

void UPalBattleDetailWidget::Clear()
{
	if (NameText) NameText->SetText(FText::FromString(TEXT("-")));
	if (LevelText) LevelText->SetText(FText::FromString(TEXT("-")));
	if (HPBar) HPBar->SetPercent(0.f);
	if (HPText) HPText->SetText(FText::FromString(TEXT("HP -")));
	if (MPBar) MPBar->SetPercent(0.f);
	if (MPText) MPText->SetText(FText::FromString(TEXT("MP -")));
	SetPortrait(nullptr);
}

void UPalBattleDetailWidget::SetPortrait(UTexture2D* Texture)
{
	if (!PortraitImage)
	{
		return;
	}
	if (Texture)
	{
		PortraitImage->SetBrushFromTexture(Texture);
		PortraitImage->SetColorAndOpacity(FLinearColor::White);
	}
	else
	{
		FSlateBrush Solid;
		Solid.DrawAs = ESlateBrushDrawType::RoundedBox;
		Solid.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
		Solid.TintColor = FLinearColor(0.3f, 0.55f, 1.f, 1.f);
		PortraitImage->SetBrush(Solid);
	}
}
