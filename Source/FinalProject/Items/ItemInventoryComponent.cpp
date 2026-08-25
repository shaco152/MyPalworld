#include "Items/ItemInventoryComponent.h"

#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"

UItemInventoryComponent::UItemInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UItemInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	Stacks.Owner = this;
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		NormalizeStacks();
		Stacks.MarkArrayDirty();
		for (FItemStack& Stack : Stacks.Items)
		{
			Stacks.MarkItemDirty(Stack);
		}
	}
	else
	{
		// Fast Array 的结构由服务器独占。兼容旧蓝图组件模板中预填的测试材料，
		// 只保留首包中已经具有有效复制 ID 的权威条目。
		const int32 Removed = Stacks.RemoveUnreplicatedLocalItems();
		if (Removed > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 客户端清理本地材料 FastArray 占位项：Removed=%d"), Removed);
		}
	}
	NotifyChanged();

	UE_LOG(LogTemp, Warning, TEXT("[诊断] ItemInventory 初始化: Owner=%s 堆叠=%d/%d Definitions=%s"),
		*GetNameSafe(GetOwner()), Stacks.Items.Num(), StackCapacity, *GetNameSafe(ItemDefinitions));
}

void UItemInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UItemInventoryComponent, Stacks, COND_OwnerOnly);
}

const FItemDefinitionRow* UItemInventoryComponent::FindItemDefinition(FName ItemId) const
{
	if (!ItemDefinitions || ItemId.IsNone())
	{
		return nullptr;
	}
	return ItemDefinitions->FindRow<FItemDefinitionRow>(ItemId, TEXT("ItemInventory"), false);
}

bool UItemInventoryComponent::GetItemDefinition(FName ItemId, FItemDefinitionRow& OutDefinition) const
{
	if (const FItemDefinitionRow* Definition = FindItemDefinition(ItemId))
	{
		OutDefinition = *Definition;
		return true;
	}
	return false;
}

int32 UItemInventoryComponent::GetMaxStackSize(FName ItemId) const
{
	const FItemDefinitionRow* Definition = FindItemDefinition(ItemId);
	return Definition ? FMath::Max(1, Definition->MaxStackSize) : 0;
}

int32 UItemInventoryComponent::AddItem(FName ItemId, int32 Quantity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ItemId.IsNone() || Quantity <= 0)
	{
		return 0;
	}

	const int32 MaxStack = GetMaxStackSize(ItemId);
	if (MaxStack <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] ItemInventory::AddItem 拒绝未知 ItemId=%s（检查 DT_ItemDefinitions）"), *ItemId.ToString());
		return 0;
	}

	int32 Remaining = Quantity;
	for (FItemStack& Stack : Stacks.Items)
	{
		if (Stack.ItemId != ItemId || Stack.Quantity >= MaxStack)
		{
			continue;
		}
		const int32 Added = FMath::Min(MaxStack - Stack.Quantity, Remaining);
		Stack.Quantity += Added;
		Stacks.MarkItemDirty(Stack);
		Remaining -= Added;
		if (Remaining <= 0)
		{
			break;
		}
	}

	while (Remaining > 0 && Stacks.Items.Num() < FMath::Max(1, StackCapacity))
	{
		const int32 Added = FMath::Min(MaxStack, Remaining);
		FItemStack& NewStack = Stacks.Items.AddDefaulted_GetRef();
		NewStack.ItemId = ItemId;
		NewStack.Quantity = Added;
		Remaining -= Added;
		Stacks.MarkItemDirty(NewStack);
	}

	const int32 AddedTotal = Quantity - Remaining;
	if (AddedTotal > 0)
	{
		NotifyChanged();
		UE_LOG(LogTemp, Warning, TEXT("[诊断] ItemInventory 加入 %s x%d（请求%d，当前总数%d）"),
			*ItemId.ToString(), AddedTotal, Quantity, GetTotalQuantity(ItemId));
	}
	return AddedTotal;
}

int32 UItemInventoryComponent::GetTotalQuantity(FName ItemId) const
{
	int32 Total = 0;
	for (const FItemStack& Stack : Stacks.Items)
	{
		if (Stack.ItemId == ItemId && Stack.Quantity > 0)
		{
			Total += Stack.Quantity;
		}
	}
	return Total;
}

bool UItemInventoryComponent::HasItems(const TArray<FItemAmount>& Costs) const
{
	TMap<FName, int32> RequiredById;
	for (const FItemAmount& Cost : Costs)
	{
		if (Cost.IsValid())
		{
			RequiredById.FindOrAdd(Cost.ItemId) += Cost.Quantity;
		}
	}

	for (const TPair<FName, int32>& Required : RequiredById)
	{
		if (GetTotalQuantity(Required.Key) < Required.Value)
		{
			return false;
		}
	}
	return true;
}

bool UItemInventoryComponent::ConsumeItems(const TArray<FItemAmount>& Costs)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !HasItems(Costs))
	{
		return false;
	}

	TMap<FName, int32> RemainingById;
	for (const FItemAmount& Cost : Costs)
	{
		if (Cost.IsValid())
		{
			RemainingById.FindOrAdd(Cost.ItemId) += Cost.Quantity;
		}
	}

	for (TPair<FName, int32>& Required : RemainingById)
	{
		for (FItemStack& Stack : Stacks.Items)
		{
			if (Required.Value <= 0)
			{
				break;
			}
			if (Stack.ItemId != Required.Key || Stack.Quantity <= 0)
			{
				continue;
			}
			const int32 Removed = FMath::Min(Stack.Quantity, Required.Value);
			Stack.Quantity -= Removed;
			Stacks.MarkItemDirty(Stack);
			Required.Value -= Removed;
		}
	}

	NormalizeStacks();
	NotifyChanged();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] ItemInventory 已原子消耗配方材料，剩余堆叠=%d"), Stacks.Items.Num());
	return true;
}

void UItemInventoryComponent::NormalizeStacks()
{
	const int32 Removed = Stacks.Items.RemoveAll([](const FItemStack& Stack) { return !Stack.IsValid(); });
	if (Removed > 0 && GetOwner() && GetOwner()->HasAuthority())
	{
		Stacks.MarkArrayDirty();
	}

	if (Stacks.Items.Num() > FMath::Max(1, StackCapacity))
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] ItemInventory 预填堆叠%d超过容量%d，尾部数据被裁剪"), Stacks.Items.Num(), StackCapacity);
		Stacks.Items.SetNum(FMath::Max(1, StackCapacity));
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			Stacks.MarkArrayDirty();
		}
	}

	for (FItemStack& Stack : Stacks.Items)
	{
		const int32 MaxStack = GetMaxStackSize(Stack.ItemId);
		if (MaxStack > 0)
		{
			Stack.Quantity = FMath::Clamp(Stack.Quantity, 1, MaxStack);
		}
	}
}

void UItemInventoryComponent::NotifyChanged()
{
	OnInventoryChanged.Broadcast();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetOwner()->ForceNetUpdate();
	}
}

void UItemInventoryComponent::HandleReplicatedStacks()
{
	// 首次复制可能早于 BeginPlay，也可能与蓝图默认条目同时存在；每次收包都清理
	// ReplicationID 无效的本地占位项，绝不在客户端补槽或规格化权威数组。
	const int32 Removed = Stacks.RemoveUnreplicatedLocalItems();
	if (Removed > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 材料 FastArray 收包后清理本地占位项：Removed=%d"), Removed);
	}
	NotifyChanged();
}

void UItemInventoryComponent::RestoreStacks(const TArray<FItemStack>& InStacks)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	Stacks.Items = InStacks;
	Stacks.MarkArrayDirty();
	for (FItemStack& Stack : Stacks.Items)
	{
		Stacks.MarkItemDirty(Stack);
	}
	NormalizeStacks();
	NotifyChanged();
	GetOwner()->ForceNetUpdate();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] ItemInventory 已从存档恢复，堆叠=%d/%d"), Stacks.Items.Num(), StackCapacity);
}

void FReplicatedItemStackList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (Owner) Owner->HandleReplicatedStacks();
}
