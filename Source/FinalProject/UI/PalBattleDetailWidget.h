#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PalBattleDetailWidget.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;
class UTexture2D;
class APalCharacter;
struct FStoredPalInfo;

/** 战斗/仓库共用的帕鲁详情原子控件；布局与样式全部由 WBP 提供。 */
UCLASS()
class FINALPROJECT_API UPalBattleDetailWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPalBattleDetailWidget(const FObjectInitializer& ObjectInitializer);

	void UpdateFromPal(const APalCharacter* Pal);
	void UpdateFromStoredInfo(const FStoredPalInfo& Info);
	void Clear();

private:
	void SetPortrait(UTexture2D* Texture);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HPBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HPText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> MPBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MPText;
};
