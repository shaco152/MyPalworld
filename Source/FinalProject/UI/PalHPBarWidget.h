#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PalHPBarWidget.generated.h"

class UProgressBar;

/** 帕鲁头顶血条；WBP 提供布局和笔刷，C++ 更新百分比与绿/黄/红数据驱动色阶。 */
UCLASS()
class FINALPROJECT_API UPalHPBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPalHPBarWidget(const FObjectInitializer& ObjectInitializer);
	void UpdateBar(float Percent);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Bar;
};
