#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/ItemData.h"
#include "ItemInventoryComponent.generated.h"

class UDataTable;
class UItemInventoryComponent;

USTRUCT(BlueprintType)
struct FINALPROJECT_API FReplicatedItemStackList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FItemStack> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FItemStack, FReplicatedItemStackList>(Items, DeltaParams, *this);
	}

	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	/**
	 * 蓝图组件模板可能序列化了测试材料。客户端上的这些条目没有复制 ID，
	 * 若不先清理，首包会把服务器条目追加在本地条目后面，造成数量翻倍且无法被服务器删除。
	 */
	int32 RemoveUnreplicatedLocalItems()
	{
		const int32 Removed = Items.RemoveAll([](const FItemStack& Item)
		{
			return Item.ReplicationID == INDEX_NONE;
		});
		if (Removed > 0)
		{
			ItemMap.Reset();
		}
		return Removed;
	}

	UItemInventoryComponent* Owner = nullptr;
};

template<>
struct TStructOpsTypeTraits<FReplicatedItemStackList> : public TStructOpsTypeTraitsBase2<FReplicatedItemStackList>
{
	enum { WithNetDeltaSerializer = true };
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnItemInventoryChanged);

/**
 * 通用可堆叠物品背包。当前由玩家材料背包和建造消耗使用，后续箱子/制作台可直接复用。
 * 修改只在 Authority 执行；Stacks 复制后通过 OnInventoryChanged 事件刷新 UI，禁止 Tick。
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class FINALPROJECT_API UItemInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UItemInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 最多可占用的堆叠槽数量。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 StackCapacity = 24;

	/** 统一物品定义表；行名即 ItemId。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UDataTable> ItemDefinitions;

	/** 可在 BP_Player 默认值中预填测试材料。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Inventory")
	FReplicatedItemStackList Stacks;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemInventoryChanged OnInventoryChanged;

	/** 尽可能加入，返回实际加入数量；背包满时允许部分加入。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetTotalQuantity(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItems(const TArray<FItemAmount>& Costs) const;

	/** 原子消耗：任一材料不足则完全不修改。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ConsumeItems(const TArray<FItemAmount>& Costs);

	/** 查表复制一份定义给 UI/蓝图；找不到返回 false。 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool GetItemDefinition(FName ItemId, FItemDefinitionRow& OutDefinition) const;

	const FItemDefinitionRow* FindItemDefinition(FName ItemId) const;
	const TArray<FItemStack>& GetStacks() const { return Stacks.Items; }
	int32 GetStackCapacity() const { return StackCapacity; }
	UDataTable* GetItemDefinitions() const { return ItemDefinitions; }
	/** 存档恢复专用：Authority 替换完整堆叠快照。 */
	void RestoreStacks(const TArray<FItemStack>& InStacks);
	void HandleReplicatedStacks();

protected:
	virtual void BeginPlay() override;

private:
	int32 GetMaxStackSize(FName ItemId) const;
	void NormalizeStacks();
	void NotifyChanged();
};
