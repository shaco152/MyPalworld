#include "PalBoxWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/HorizontalBox.h"
#include "Components/WrapBox.h"
#include "Engine/Engine.h"
#include "Input/DragAndDrop.h"
#include "Storage/PalStorageComponent.h"
#include "UI/PalBattleDetailWidget.h"
#include "UI/PalSkillManagementWidget.h"
#include "UI/PalSlotWidget.h"

void UPalBoxWidget::InitFromStorage(UPalStorageComponent* InStorage)
{
	// 只保存引用并绑定刷新委托（本函数可能被多次调用，Remove+Add 防重复）。
	// 槽配置与动态建槽在 NativeConstruct 做（每次 AddToViewport 重触发，覆盖树重建）
	Storage = InStorage;
	BindStorage();

	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalBoxWidget::InitFromStorage: 存储组件=%s, PartyPanel=%s, BoxGrid=%s, DetailWidget=%s, SlotWidgetClass=%s"),
		*GetNameSafe(Storage), *GetNameSafe(PartyPanel), *GetNameSafe(BoxGrid), *GetNameSafe(DetailWidget), *GetNameSafe(SlotWidgetClass));

	Refresh();
}

void UPalBoxWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 每次 AddToViewport 重触发：控件树已按 WBP 模板重建（预摆槽是新实例、BoxGrid 为空），
	// 必须全量重配置 + 重建动态仓库槽 + 重绑外部委托（Remove+Add 幂等）
	SelectedPartyIndex = INDEX_NONE; // 重开界面从无锁定状态开始
	ConfigurePartySlots();
	EnsureBoxSlots();
	BindStorage();
	BindSlotClicks();

	if (SkillManagementWidget)
	{
		SkillManagementWidget->InitFromStorage(Storage); // 幂等；Box 流程 InitFromStorage 先于首构，Storage 已就绪
	}
	ApplySkillTarget();
	UpdateDetailFromSelection();
	Refresh();
}

void UPalBoxWidget::NativeDestruct()
{
	UnbindSlotClicks();
	UnbindSlotHover();
	UnbindStorage();

	// 界面关闭时清掉槽引用（槽的 UWidget 随父控件销毁）
	PartySlots.Empty();
	BoxSlots.Empty();
	SelectedPartyIndex = INDEX_NONE;

	Super::NativeDestruct();
}

void UPalBoxWidget::ConfigurePartySlots()
{
	PartySlots.Reset();
	PartySlots.Add(PartySlot0);
	PartySlots.Add(PartySlot1);
	PartySlots.Add(PartySlot2);
	PartySlots.Add(PartySlot3);
	PartySlots.Add(PartySlot4);

	if (!PartySlot0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] PalBoxWidget::ConfigurePartySlots: PartySlot0 未绑定！请检查 WBP_PalBox 是否预摆了 PartySlot0-4（WBP_PalSlot 实例）"));
	}

	for (int32 i = 0; i < PartySlots.Num(); ++i)
	{
		if (UPalSlotWidget* PalSlot = PartySlots[i])
		{
			PalSlot->ConfigureSlot(true, i, EPalSlotInteractionMode::StorageDragDrop, this);
		}
	}

	BindSlotHover();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalBoxWidget::ConfigurePartySlots: 预摆背包槽%d个"), PartySlots.Num());
}

void UPalBoxWidget::EnsureBoxSlots()
{
	UnbindSlotHover();

	// 树重建后 BoxGrid 是空的新实例，动态子控件不在模板里 → 每次 NativeConstruct 全量重建
	BoxSlots.Reset();
	if (BoxGrid)
	{
		BoxGrid->ClearChildren();
	}

	if (!BoxGrid || !SlotWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] PalBoxWidget::EnsureBoxSlots: BoxGrid=%s SlotWidgetClass=%s 有缺失！请检查 WBP_PalBox 控件树与类默认值"),
			*GetNameSafe(BoxGrid), *GetNameSafe(SlotWidgetClass));
		BindSlotHover(); // 背包槽悬停委托不能因仓库侧缺失而丢
		return;
	}

	// 右侧：仓库槽（容量 = 存储组件的 BoxPals 数量；槽尺寸由 WBP_PalSlot 根 SizeBox 保证，
	// 间距由 BoxGrid 的 InnerSlotPadding 在设计器设置——C++ 不写尺寸/间距）
	const int32 BoxCount = Storage ? Storage->BoxPals.Num() : 0;
	for (int32 i = 0; i < BoxCount; ++i)
	{
		if (UPalSlotWidget* PalSlot = CreateWidget<UPalSlotWidget>(this, SlotWidgetClass))
		{
			PalSlot->ConfigureSlot(false, i, EPalSlotInteractionMode::StorageDragDrop, this);
			BoxSlots.Add(PalSlot);
			BoxGrid->AddChildToWrapBox(PalSlot);
		}
	}

	BindSlotHover();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalBoxWidget::EnsureBoxSlots: 动态仓库槽%d个"), BoxSlots.Num());
}

void UPalBoxWidget::BindStorage()
{
	if (Storage)
	{
		Storage->OnStorageChanged.RemoveDynamic(this, &UPalBoxWidget::Refresh);
		Storage->OnStorageChanged.AddDynamic(this, &UPalBoxWidget::Refresh);
	}
}

void UPalBoxWidget::UnbindStorage()
{
	if (Storage)
	{
		Storage->OnStorageChanged.RemoveDynamic(this, &UPalBoxWidget::Refresh);
	}
}

void UPalBoxWidget::BindSlotHover()
{
	for (UPalSlotWidget* PalSlot : PartySlots)
	{
		if (PalSlot)
		{
			PalSlot->OnSlotHovered.RemoveDynamic(this, &UPalBoxWidget::OnSlotHovered);
			PalSlot->OnSlotHovered.AddDynamic(this, &UPalBoxWidget::OnSlotHovered);
			PalSlot->OnSlotUnhovered.RemoveDynamic(this, &UPalBoxWidget::OnSlotUnhovered);
			PalSlot->OnSlotUnhovered.AddDynamic(this, &UPalBoxWidget::OnSlotUnhovered);
		}
	}
	for (UPalSlotWidget* PalSlot : BoxSlots)
	{
		if (PalSlot)
		{
			PalSlot->OnSlotHovered.RemoveDynamic(this, &UPalBoxWidget::OnSlotHovered);
			PalSlot->OnSlotHovered.AddDynamic(this, &UPalBoxWidget::OnSlotHovered);
			PalSlot->OnSlotUnhovered.RemoveDynamic(this, &UPalBoxWidget::OnSlotUnhovered);
			PalSlot->OnSlotUnhovered.AddDynamic(this, &UPalBoxWidget::OnSlotUnhovered);
		}
	}
}

void UPalBoxWidget::UnbindSlotHover()
{
	for (UPalSlotWidget* PalSlot : PartySlots)
	{
		if (PalSlot)
		{
			PalSlot->OnSlotHovered.RemoveDynamic(this, &UPalBoxWidget::OnSlotHovered);
			PalSlot->OnSlotUnhovered.RemoveDynamic(this, &UPalBoxWidget::OnSlotUnhovered);
		}
	}
	for (UPalSlotWidget* PalSlot : BoxSlots)
	{
		if (PalSlot)
		{
			PalSlot->OnSlotHovered.RemoveDynamic(this, &UPalBoxWidget::OnSlotHovered);
			PalSlot->OnSlotUnhovered.RemoveDynamic(this, &UPalBoxWidget::OnSlotUnhovered);
		}
	}
}

void UPalBoxWidget::BindSlotClicks()
{
	for (UPalSlotWidget* PalSlot : PartySlots)
	{
		if (PalSlot)
		{
			PalSlot->OnStorageSlotClicked.RemoveDynamic(this, &UPalBoxWidget::OnStorageSlotClicked);
			PalSlot->OnStorageSlotClicked.AddDynamic(this, &UPalBoxWidget::OnStorageSlotClicked);
		}
	}
	for (UPalSlotWidget* PalSlot : BoxSlots)
	{
		if (PalSlot)
		{
			PalSlot->OnStorageSlotClicked.RemoveDynamic(this, &UPalBoxWidget::OnStorageSlotClicked);
			PalSlot->OnStorageSlotClicked.AddDynamic(this, &UPalBoxWidget::OnStorageSlotClicked);
		}
	}
}

void UPalBoxWidget::UnbindSlotClicks()
{
	for (UPalSlotWidget* PalSlot : PartySlots)
	{
		if (PalSlot)
		{
			PalSlot->OnStorageSlotClicked.RemoveDynamic(this, &UPalBoxWidget::OnStorageSlotClicked);
		}
	}
	for (UPalSlotWidget* PalSlot : BoxSlots)
	{
		if (PalSlot)
		{
			PalSlot->OnStorageSlotClicked.RemoveDynamic(this, &UPalBoxWidget::OnStorageSlotClicked);
		}
	}
}

void UPalBoxWidget::OnStorageSlotClicked(UPalSlotWidget* PalSlot, bool bFromParty, int32 SlotIndex, const FStoredPalInfo& Info)
{
	if (!bFromParty)
	{
		// 仓库侧：清空管理目标；有效仓库帕鲁提示先拖入背包
		ClearSkillSelection();
		if (Info.IsValid() && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("请先拖入背包再管理技能"));
		}
		ApplySkillTarget();
		UpdateDetailFromSelection();
		return;
	}

	if (Info.IsValid())
	{
		// 点击有效背包槽：锁定为技能管理目标
		SelectedPartyIndex = SlotIndex;
		UE_LOG(LogTemp, Warning, TEXT("[诊断] PalBoxWidget::OnStorageSlotClicked: 锁定背包槽 %d"), SlotIndex);
	}
	else
	{
		// 点击空背包槽：清空锁定
		ClearSkillSelection();
	}

	ApplySkillTarget();
	UpdateDetailFromSelection();
}

void UPalBoxWidget::ClearSkillSelection()
{
	if (SelectedPartyIndex != INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] PalBoxWidget: 清空技能管理目标（原槽 %d）"), SelectedPartyIndex);
	}
	SelectedPartyIndex = INDEX_NONE;
}

void UPalBoxWidget::ApplySkillTarget()
{
	// 锁定变化：SetTargetPal（内部清 Pending 并刷新）；存储驱动的刷新走 Refresh() 以保留 Pending
	if (SkillManagementWidget)
	{
		SkillManagementWidget->SetTargetPal(SelectedPartyIndex);
	}
}

void UPalBoxWidget::UpdateDetailFromSelection()
{
	if (!DetailWidget)
	{
		return;
	}
	if (Storage && SelectedPartyIndex != INDEX_NONE && Storage->PartyPals.IsValidIndex(SelectedPartyIndex) &&
		Storage->PartyPals[SelectedPartyIndex].IsValid())
	{
		DetailWidget->UpdateFromStoredInfo(Storage->PartyPals[SelectedPartyIndex]);
	}
	else
	{
		DetailWidget->Clear();
	}
}

void UPalBoxWidget::Refresh()
{
	if (!Storage)
	{
		return;
	}

	// 诊断：确认刷新被调用时数据状态（定位"界面不刷新"问题用）
	int32 ValidParty = 0, ValidBox = 0;
	for (const FStoredPalInfo& P : Storage->PartyPals)
	{
		ValidParty += P.IsValid() ? 1 : 0;
	}
	for (const FStoredPalInfo& B : Storage->BoxPals)
	{
		ValidBox += B.IsValid() ? 1 : 0;
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalBoxWidget::Refresh: 背包槽数=%d(有效%d), 仓库槽数=%d(有效%d), 当前槽=%d"),
		PartySlots.Num(), ValidParty, BoxSlots.Num(), ValidBox, Storage->ActivePartyIndex);

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

	for (int32 i = 0; i < BoxSlots.Num(); ++i)
	{
		if (!BoxSlots[i])
		{
			continue;
		}
		const FStoredPalInfo Info = Storage->BoxPals.IsValidIndex(i) ? Storage->BoxPals[i] : FStoredPalInfo();
		BoxSlots[i]->SetSlotData(Info, false);
	}

	// 选中槽校验：对应槽已空则清锁定
	if (SelectedPartyIndex != INDEX_NONE &&
		(!Storage->PartyPals.IsValidIndex(SelectedPartyIndex) || !Storage->PartyPals[SelectedPartyIndex].IsValid()))
	{
		ClearSkillSelection();
		ApplySkillTarget();
	}

	// 级联刷新技能管理面板（用 Refresh 保留装配中 Pending；SetPalSkill 成功后也走本路径）
	if (SkillManagementWidget)
	{
		SkillManagementWidget->Refresh();
	}
	UpdateDetailFromSelection();
}

void UPalBoxWidget::OnSlotHovered(UPalSlotWidget* PalSlot, const FStoredPalInfo& Info)
{
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 仓库槽悬浮: 槽%d %s Lv.%.0f"), PalSlot ? PalSlot->GetSlotIndex() : -1,
		Info.IsValid() ? *Info.PalClass->GetName() : TEXT("空槽"), Info.Level);
	if (DetailWidget)
	{
		DetailWidget->UpdateFromStoredInfo(Info);
	}
}

void UPalBoxWidget::OnSlotUnhovered()
{
	// 悬浮结束：恢复锁定帕鲁的详情（无锁定则清空）
	UpdateDetailFromSelection();
}

bool UPalBoxWidget::HandleSlotDrop(bool bFromParty, int32 FromIndex, bool bToParty, int32 ToIndex)
{
	if (!Storage)
	{
		return false;
	}

	// 同槽拖放忽略（SwapSlots 自身也会校验索引）
	if (bFromParty == bToParty && FromIndex == ToIndex)
	{
		return true;
	}

	// 出战的帕鲁不能直接拖进仓库 / 出战槽不能作为交换目标（兜底校验，需先按 F 收回）
	const bool bFromSummoned = bFromParty && Storage->HasSummonedPal() && Storage->GetSummonedPartyIndex() == FromIndex;
	const bool bToSummoned = bToParty && Storage->HasSummonedPal() && Storage->GetSummonedPartyIndex() == ToIndex;
	if (bFromSummoned || bToSummoned)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] HandleSlotDrop: 涉及出战帕鲁的槽（来源出战=%d 目标出战=%d），已阻止"), (int32)bFromSummoned, (int32)bToSummoned);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("出战的帕鲁不能放进仓库，先按 F 收回！"));
		}
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] HandleSlotDrop: %s第%d槽 → %s第%d槽"),
		bFromParty ? TEXT("背包") : TEXT("仓库"), FromIndex, bToParty ? TEXT("背包") : TEXT("仓库"), ToIndex);

	// 交换前清空技能管理锁定，避免交换后 SelectedPartyIndex 指向另一只帕鲁
	ClearSkillSelection();
	ApplySkillTarget();
	UpdateDetailFromSelection();

	Storage->SwapSlots(bFromParty, FromIndex, bToParty, ToIndex); // 成功后广播 → Refresh 自动刷新
	return true;
}

bool UPalBoxWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// 声明整个界面为有效放置目标（兜底）：光标在槽上时槽先收到（最深命中），空白处冒泡到本界面
	return InOperation && InOperation->Payload && InOperation->Payload->IsA<UPalDragPayload>();
}

bool UPalBoxWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const UPalDragPayload* Payload = InOperation ? Cast<UPalDragPayload>(InOperation->Payload) : nullptr;
	if (!Payload || !Storage)
	{
		return false;
	}

	// 落点判断：按屏幕坐标决定进背包侧还是仓库侧（面板空白处的兜底入口）
	// 注意：FGeometry::IsUnderLocation 内部符号不对游戏模块导出（链接会失败），手动做矩形命中
	const FVector2D DropPos = InDragDropEvent.GetScreenSpacePosition();

	auto IsUnderPanel = [](const UWidget* Panel, const FVector2D& Pos) -> bool
	{
		if (!Panel)
		{
			return false;
		}
		const FGeometry& Geo = Panel->GetCachedGeometry();
		const FVector2D Min = Geo.GetAbsolutePosition();
		const FVector2D Max = Min + Geo.GetAbsoluteSize();
		return Pos.X >= Min.X && Pos.X <= Max.X && Pos.Y >= Min.Y && Pos.Y <= Max.Y;
	};

	const bool bToParty = IsUnderPanel(PartyPanel, DropPos);
	const bool bToBox = IsUnderPanel(BoxGrid, DropPos);

	if (!bToParty && !bToBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] NativeOnDrop(界面兜底): 落点(%s)不在背包面板也不在仓库面板内，忽略"), *DropPos.ToString());
		return false;
	}

	// 找到该侧第一个空槽放入
	FReplicatedStoredPalList& TargetArr = bToParty ? Storage->PartyPals : Storage->BoxPals;
	int32 EmptyIndex = INDEX_NONE;
	for (int32 i = 0; i < TargetArr.Num(); ++i)
	{
		if (!TargetArr[i].IsValid())
		{
			EmptyIndex = i;
			break;
		}
	}

	if (EmptyIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] NativeOnDrop(界面兜底): %s已满，无法放入"), bToParty ? TEXT("背包") : TEXT("仓库"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, FString::Printf(TEXT("%s已满！"), bToParty ? TEXT("背包") : TEXT("仓库")));
		}
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] NativeOnDrop(界面兜底): 落点=%s 判定为%s侧 → 放入第%d空槽"),
		*DropPos.ToString(), bToParty ? TEXT("背包") : TEXT("仓库"), EmptyIndex);
	return HandleSlotDrop(Payload->bFromParty, Payload->SourceIndex, bToParty, EmptyIndex);
}
