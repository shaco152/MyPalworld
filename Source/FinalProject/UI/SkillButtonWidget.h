#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillButtonWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillClicked, int32, SlotIndex);

/** 技能按钮原子控件：只显示父面板下发的文本/可用性，并广播自身槽号。 */
UCLASS()
class FINALPROJECT_API USkillButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USkillButtonWidget(const FObjectInitializer& ObjectInitializer);

	void Configure(int32 InSlotIndex);
	void SetPresentation(const FText& InLabel, bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category = "Skill")
	FOnSkillClicked OnSkillClicked;

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Label;

	int32 SlotIndex = -1;
};
