#include "UI/SaveListWidget.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Persistence/SaveGameSubsystem.h"
#include "UI/SaveSlotWidget.h"

void USaveListWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BackButton->OnClicked.RemoveDynamic(this, &USaveListWidget::HandleBackClicked);
	BackButton->OnClicked.AddDynamic(this, &USaveListWidget::HandleBackClicked);
}

void USaveListWidget::NativeDestruct()
{
	if (BackButton)
	{
		BackButton->OnClicked.RemoveDynamic(this, &USaveListWidget::HandleBackClicked);
	}
	OnSaveChosen.Clear();
	OnBack.Clear();
	Super::NativeDestruct();
}

void USaveListWidget::RefreshSaveList()
{
	// 只清理由代码生成的存档卡片。即使蓝图误把返回按钮或状态文字放进容器，
	// 也不能在刷新时删除静态导航控件，避免空列表页面无法返回。
	for (int32 ChildIndex = SaveListContainer->GetChildrenCount() - 1; ChildIndex >= 0; --ChildIndex)
	{
		if (Cast<USaveSlotWidget>(SaveListContainer->GetChildAt(ChildIndex)))
		{
			SaveListContainer->RemoveChildAt(ChildIndex);
		}
	}
	USaveGameSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveGameSubsystem>();
	const TArray<FWorldSaveMetadata> Entries = Saves
		? Saves->RefreshSaveListFromDisk() : TArray<FWorldSaveMetadata>();
	for (const FWorldSaveMetadata& Metadata : Entries)
	{
		USaveSlotWidget* Entry = CreateWidget<USaveSlotWidget>(GetOwningPlayer(), SaveSlotWidgetClass);
		if (!Entry)
		{
			continue;
		}
		Entry->SetSaveMetadata(Metadata);
		Entry->OnSelected.AddUObject(this, &USaveListWidget::HandleSaveSelected);
		SaveListContainer->AddChild(Entry);
	}
	SetStatus(Entries.IsEmpty() ? TEXT("暂无世界存档") : FString::Printf(TEXT("共 %d 个世界存档"), Entries.Num()));
}

void USaveListWidget::SetStatus(const FString& Message)
{
	StatusText->SetText(FText::FromString(Message));
}

void USaveListWidget::HandleSaveSelected(FGuid WorldId)
{
	OnSaveChosen.Broadcast(WorldId);
}

void USaveListWidget::HandleBackClicked()
{
	OnBack.Broadcast();
}
