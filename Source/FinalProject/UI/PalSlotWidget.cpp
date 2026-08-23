#include "PalSlotWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Storage/PalStorageComponent.h"
#include "UI/PalBoxWidget.h"

namespace
{
	// 无贴图时给控件一个 RoundedBox 纯色刷子兜底（RoundedBox 不需要纹理资源即可渲染填充）
	void EnsureSolidBrush(UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}
		UBorder* Border = Cast<UBorder>(Widget);
		UImage* Image = Cast<UImage>(Widget);
		// UBorder 没有 GetBrush()（用公开成员 Background）；UImage 的 Brush 成员已弃用，走 GetBrush()
		const FSlateBrush* Current = Border ? &Border->Background : (Image ? &Image->GetBrush() : nullptr);
		if (Current && Current->GetResourceObject() == nullptr)
		{
			FSlateBrush Solid;
			Solid.DrawAs = ESlateBrushDrawType::RoundedBox;
			Solid.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
			Solid.TintColor = FLinearColor::White;
			if (Border)
			{
				Border->SetBrush(Solid);
			}
			else if (Image)
			{
				Image->SetBrush(Solid);
			}
		}
	}
}

UPalSlotWidget::UPalSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 可聚焦保证接收鼠标按键事件（拖放检测/选择点击入口）
	SetIsFocusable(true);
}

void UPalSlotWidget::ConfigureSlot(bool bInPartySlot, int32 InSlotIndex, EPalSlotInteractionMode InMode, UPalBoxWidget* InOwnerBox)
{
	bPartySlot = bInPartySlot;
	SlotIndex = InSlotIndex;
	InteractionMode = InMode;
	OwnerBox = InOwnerBox;
}

void UPalSlotWidget::SetSlotData(const FStoredPalInfo& Info, bool bActive, bool bSummoned)
{
	bHasPal = Info.IsValid();
	bIsSummonedSlot = bSummoned;
	CachedInfo = Info;

	if (NameText)
	{
		if (bHasPal)
		{
			// 显示 类名 + 等级（如 "BP_Dragon Lv.3"），出战时追加标识
			NameText->SetText(FText::FromString(FString::Printf(TEXT("%s Lv.%.0f%s"),
				*Info.PalClass->GetName(), Info.Level, bSummoned ? TEXT("（出战）") : TEXT(""))));
		}
		else
		{
			NameText->SetText(FText::FromString(TEXT("空")));
		}
	}

	// 图标：优先显示帕鲁类的头像贴图（PortraitIcon → FStoredPalInfo.Icon），无贴图/空槽时纯色块兜底
	if (Icon)
	{
		if (bHasPal && Info.Icon)
		{
			Icon->SetBrushFromTexture(Info.Icon);
			Icon->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			FSlateBrush Solid;
			Solid.DrawAs = ESlateBrushDrawType::RoundedBox;
			Solid.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
			Solid.TintColor = FLinearColor::White;
			Icon->SetBrush(Solid);
			Icon->SetColorAndOpacity(bHasPal ? FLinearColor(0.35f, 0.6f, 1.f, 1.f) : FLinearColor(0.15f, 0.15f, 0.15f, 1.f));
		}
	}

	// 高亮边框：Highlight 可能是槽的根控件——绝不能 SetVisibility(Hidden)，否则整个槽（图标+文字）一起消失。
	// 改用边框颜色区分当前选中槽（金色 = 当前，深灰 = 非当前）
	if (Highlight)
	{
		EnsureSolidBrush(Highlight);
		Highlight->SetVisibility(ESlateVisibility::Visible);
		Highlight->SetBrushColor(bActive ? FLinearColor(1.f, 0.85f, 0.2f, 0.9f) : FLinearColor(0.05f, 0.05f, 0.05f, 0.6f));
	}
}

void UPalSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	// 悬浮进入：广播槽数据（仓库界面右侧详情面板显示血量/MP/头像/等级）
	if (bHasPal)
	{
		OnSlotHovered.Broadcast(this, CachedInfo);
	}
}

void UPalSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	OnSlotUnhovered.Broadcast();
}

FReply UPalSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	// Selection 模式（回合制切换页）：槽内有帕鲁时广播选择（有效性过滤由切换面板负责），无拖放
	if (InteractionMode == EPalSlotInteractionMode::Selection)
	{
		if (bHasPal)
		{
			OnSlotSelected.Broadcast(SlotIndex);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	// StorageDragDrop 模式（仓库界面）
	if (InteractionMode == EPalSlotInteractionMode::StorageDragDrop && OwnerBox)
	{
		bDragDetected = false;
		if (bHasPal)
		{
			if (bIsSummonedSlot)
			{
				// 出战的帕鲁不能拖出（需先按 F 收回），但技能管理必须可点选它：
				// 不启动 DetectDrag，捕获鼠标保证 MouseUp 路由回本槽广播点击
				UE_LOG(LogTemp, Warning, TEXT("[诊断] 出战槽 %d：不启动拖拽，等待点击（技能管理可点选出战帕鲁）"), SlotIndex);
				return FReply::Handled().CaptureMouse(TakeWidget());
			}
			return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
		}
		// 空槽：可点击（清空锁定），捕获鼠标等待 MouseUp
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	// DisplayOnly（HUD）：不拦截鼠标
	return FReply::Unhandled();
}

FReply UPalSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 仅 StorageDragDrop 模式：未发生拖拽的左键抬起 = 仓库点击（锁定/清空/提示语义由 PalBoxWidget 决定）
	if (InteractionMode == EPalSlotInteractionMode::StorageDragDrop && OwnerBox &&
		InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !bDragDetected)
	{
		OnStorageSlotClicked.Broadcast(this, bPartySlot, SlotIndex, CachedInfo);
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

void UPalSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	bDragDetected = true; // 已达拖拽阈值：MouseUp 不再广播点击

	UPalDragPayload* Payload = NewObject<UPalDragPayload>();
	Payload->bFromParty = bPartySlot;
	Payload->SourceIndex = SlotIndex;

	UDragDropOperation* DragOp = NewObject<UDragDropOperation>();
	DragOp->Payload = Payload;
	DragOp->Pivot = EDragPivot::CenterCenter;

	// 拖拽视觉：新建一个同类槽控件作为"复制体"跟随鼠标（直接用自身作视觉会被 Slate 重挂载导致源槽消失）。
	// 视觉尺寸由 WBP_PalSlot 设计器根 SizeBox 保证（不再用 C++ SizeBox 包装——禁止 C++ 排版）
	if (UPalSlotWidget* DragVisual = CreateWidget<UPalSlotWidget>(this, GetClass()))
	{
		DragVisual->SetSlotData(CachedInfo, false);
		DragVisual->SetRenderOpacity(0.7f); // 略微透明
		DragOp->DefaultDragVisual = DragVisual;

		UE_LOG(LogTemp, Warning, TEXT("[诊断] 拖拽视觉已创建: %s（尺寸由 WBP_PalSlot 根 SizeBox 保证）"), *DragVisual->GetName());
	}
	else
	{
		DragOp->DefaultDragVisual = this; // 兜底：用源槽自身作视觉
	}

	OutOperation = DragOp;
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 拖拽开始: %s第%d槽, Operation=%s"), bPartySlot ? TEXT("背包") : TEXT("仓库"), SlotIndex, *DragOp->GetName());
}

bool UPalSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// 关键：返回 true 声明本控件是有效放置目标，否则 NativeOnDrop 不会触发
	return true;
}

bool UPalSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UE_LOG(LogTemp, Warning, TEXT("[诊断] NativeOnDrop(槽): 目标=%s第%d槽, Operation=%s, OwnerBox=%s, Payload类=%s"),
		bPartySlot ? TEXT("背包") : TEXT("仓库"), SlotIndex, *GetNameSafe(InOperation), *GetNameSafe(OwnerBox),
		InOperation ? *GetNameSafe(InOperation->Payload) : TEXT("无"));

	if (!OwnerBox || !InOperation)
	{
		return false;
	}

	// 出战的槽不能作为交换目标（放别的帕鲁进来会把出战帕鲁挤走）
	if (bIsSummonedSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] NativeOnDrop(槽): 目标是出战帕鲁的槽 %d，已阻止（先按 F 收回）"), SlotIndex);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("出战中的槽位不能交换，先按 F 收回！"));
		}
		return false;
	}

	const UPalDragPayload* Payload = Cast<UPalDragPayload>(InOperation->Payload);
	if (!Payload)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] NativeOnDrop(槽): Payload 不是 UPalDragPayload，忽略"));
		return false;
	}

	// 交给仓库界面做实际数据交换（交换成功后存储组件广播 → UI 自动刷新）
	return OwnerBox->HandleSlotDrop(Payload->bFromParty, Payload->SourceIndex, bPartySlot, SlotIndex);
}
