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
	NormalizeStacks();
	NotifyChanged();

	UE_LOG(LogTemp, Warning, TEXT("[诊断] ItemInventory 初始化: Owner=%s 堆叠=%d/%d Definitions=%s"),
		*GetNameSafe(GetOwner()), Stacks.Num(), StackCapacity, *GetNameSafe(ItemDefinitions));
}

void UItemInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UItemInventoryComponent, Stacks);
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
	for (FItemStack& Stack : Stacks)
	{
		if (Stack.ItemId != ItemId || Stack.Quantity >= MaxStack)
		{
			continue;
		}
		const int32 Added = FMath::Min(MaxStack - Stack.Quantity, Remaining);
		Stack.Quantity += Added;
		Remaining -= Added;
		if (Remaining <= 0)
		{
			break;
		}
	}

	while (Remaining > 0 && Stacks.Num() < FMath::Max(1, StackCapacity))
	{
		const int32 Added = FMath::Min(MaxStack, Remaining);
		FItemStack& NewStack = Stacks.AddDefaulted_GetRef();
		NewStack.ItemId = ItemId;
		NewStack.Quantity = Added;
		Remaining -= Added;
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
	for (const FItemStack& Stack : Stacks)
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
		for (FItemStack& Stack : Stacks)
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
			Required.Value -= Removed;
		}
	}

	NormalizeStacks();
	NotifyChanged();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] ItemInventory 已原子消耗配方材料，剩余堆叠=%d"), Stacks.Num());
	return true;
}

void UItemInventoryComponent::NormalizeStacks()
{
	Stacks.RemoveAll([](const FItemStack& Stack) { return !Stack.IsValid(); });

	if (Stacks.Num() > FMath::Max(1, StackCapacity))
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] ItemInventory 预填堆叠%d超过容量%d，尾部数据被裁剪"), Stacks.Num(), StackCapacity);
		Stacks.SetNum(FMath::Max(1, StackCapacity));
	}

	for (FItemStack& Stack : Stacks)
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
}

void UItemInventoryComponent::OnRep_Stacks()
{
	NormalizeStacks();
	NotifyChanged();
}
