#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/PalCharacter.h" // TSubclassOf<APalCharacter> 的 UPROPERTY 反射需要完整类型
#include "Engine/TimerHandle.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "PalStorageComponent.generated.h"

class UPalAttributeSet;
class UTexture2D;

/** 背包/仓库中一只帕鲁的存档信息（空槽 = PalClass 为 null） */
USTRUCT(BlueprintType)
struct FINALPROJECT_API FStoredPalInfo : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pal")
	TSubclassOf<APalCharacter> PalClass;

	/** 稳定定义 ID（DT_PalDefinitions 行名）与服务器生成的实例 ID。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pal|Persistence")
	FName PalDefinitionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pal|Persistence")
	FGuid PalInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pal")
	float Level = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pal")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pal")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pal")
	float MP = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pal")
	float MaxMP = 50.f;

	// 技能槽（4 个 DataTable 行名：槽 0 普攻不耗 MP，槽 1-3 学习技能；NAME_None = 空槽）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pal")
	TArray<FName> SkillRowNames;

	// 头像贴图（捕捉时取自 APalCharacter::PortraitIcon，UI 槽位显示；可为空 → 纯色兜底）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pal")
	TObjectPtr<UTexture2D> Icon;

	bool IsValid() const { return PalClass != nullptr; }
};

class UPalStorageComponent;

USTRUCT(BlueprintType)
struct FINALPROJECT_API FReplicatedStoredPalList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PalStorage")
	TArray<FStoredPalInfo> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FStoredPalInfo, FReplicatedStoredPalList>(Items, DeltaParams, *this);
	}

	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
	int32 Num() const { return Items.Num(); }
	bool IsValidIndex(int32 Index) const { return Items.IsValidIndex(Index); }
	void SetNum(int32 Count) { Items.SetNum(Count); }
	FStoredPalInfo& operator[](int32 Index) { return Items[Index]; }
	const FStoredPalInfo& operator[](int32 Index) const { return Items[Index]; }
	auto begin() { return Items.begin(); }
	auto end() { return Items.end(); }
	auto begin() const { return Items.begin(); }
	auto end() const { return Items.end(); }
	void Swap(int32 A, int32 B)
	{
		if (A == B || !Items.IsValidIndex(A) || !Items.IsValidIndex(B)) return;
		// 复制 ID 表示固定槽位而不是帕鲁实体；交换 payload 后恢复每个槽位原 ID，客户端才会更新对应索引。
		const int32 AId = Items[A].ReplicationID;
		const int32 AKey = Items[A].ReplicationKey;
		const int32 AMostRecentKey = Items[A].MostRecentArrayReplicationKey;
		const int32 BId = Items[B].ReplicationID;
		const int32 BKey = Items[B].ReplicationKey;
		const int32 BMostRecentKey = Items[B].MostRecentArrayReplicationKey;
		Items.Swap(A, B);
		Items[A].ReplicationID = AId;
		Items[A].ReplicationKey = AKey;
		Items[A].MostRecentArrayReplicationKey = AMostRecentKey;
		Items[B].ReplicationID = BId;
		Items[B].ReplicationKey = BKey;
		Items[B].MostRecentArrayReplicationKey = BMostRecentKey;
		MarkItemDirty(Items[A]);
		MarkItemDirty(Items[B]);
	}
	/**
	 * 蓝图组件模板可能带有未复制的固定空槽（ReplicationID == INDEX_NONE）。
	 * 客户端必须移除这些本地占位项，让 Fast Array 独占数组结构；同时清空索引缓存，
	 * 避免后续增量包继续引用删除前的本地索引。
	 */
	int32 RemoveUnreplicatedLocalItems()
	{
		const int32 Removed = Items.RemoveAll([](const FStoredPalInfo& Item)
		{
			return Item.ReplicationID == INDEX_NONE;
		});
		if (Removed > 0)
		{
			ItemMap.Reset();
		}
		return Removed;
	}
	void MarkAllDirty()
	{
		MarkArrayDirty();
		for (FStoredPalInfo& Item : Items) MarkItemDirty(Item);
	}

	UPalStorageComponent* Owner = nullptr;
};

template<>
struct TStructOpsTypeTraits<FReplicatedStoredPalList> : public TStructOpsTypeTraitsBase2<FReplicatedStoredPalList>
{
	enum { WithNetDeltaSerializer = true };
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStorageChanged);

/**
 * 帕鲁存储组件（长期数据挂在 AFinalProjectPlayerState 上）：
 * 背包固定 5 槽 + 仓库 BoxCapacity 槽，捕捉入库优先进背包，
 * 提供拖放交换、左右切换当前槽、召唤/收回（F 键切换式，召唤保留槽位）等操作，
 * 每次变更广播 OnStorageChanged 供 HUD / 仓库 UI 刷新。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FINALPROJECT_API UPalStorageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPalStorageComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 背包容量（需求硬性规定最多携带 5 只）
	static constexpr int32 PartyCapacity = 5;

	// 背包 5 槽 / 仓库容量个槽（空槽 IsValid()==false）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "PalStorage")
	FReplicatedStoredPalList PartyPals;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "PalStorage")
	FReplicatedStoredPalList BoxPals;

	// 当前选中的背包槽（左右键切换，F 召唤取它）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = HandleReplicatedStorage, Category = "PalStorage")
	int32 ActivePartyIndex = 0;

	// 仓库容量（槽数，蓝图中可调）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalStorage")
	int32 BoxCapacity = 16;

	// 存储中的帕鲁每分钟回血比例（对背包+仓库全部生效，HP=0 也回 → 缓慢复活重新可召唤）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalStorage")
	float StoredPalRegenPercent = 0.05f;

	// 存储回血间隔（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalStorage")
	float StoredPalRegenInterval = 60.f;

	// 召唤时帕鲁出现在玩家前方距离（厘米）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PalStorage")
	float SummonDistance = 200.f;

	// 存储变化广播（HUD / 仓库 UI 监听刷新）
	UPROPERTY(BlueprintAssignable, Category = "PalStorage")
	FOnStorageChanged OnStorageChanged;

	// 捕捉入库：优先进背包第一个空槽，否则进仓库第一个空槽；都满返回 false（丢弃）
	bool AddCapturedPal(const FStoredPalInfo& Info);

	// 捕捉入库（回合制捕捉用）：优先进仓库（防现捉现用），仓库满则进背包兜底；都满返回 false
	bool AddCapturedPalToBox(const FStoredPalInfo& Info);

	// 拖放交换：跨背包/仓库或同侧换位（空槽参与交换 = 移动）
	void SwapSlots(bool bFromParty, int32 FromIndex, bool bToParty, int32 ToIndex);

	// 左右切换当前背包槽：Delta=+1/-1，跳过空槽循环；全空返回 false
	bool CycleActiveIndex(int32 Delta);

	// 召唤/收回当前槽帕鲁（F 键，切换式）：
	// 当前槽已在出战 → 收回；其他槽在出战 → 先收回再召唤当前槽；
	// 召唤的帕鲁保留在背包（不清槽），打 State.Pal.Summoned 标签禁止被捕捉。
	// 返回 null 表示本次执行的是收回。
	APalCharacter* SummonOrRecallActivePal();

	// 收回出战帕鲁（移除防捕捉标签 + 销毁实体）
	void RecallSummonedPal();

	// 出战状态查询（UI 用于禁止把出战帕鲁拖进仓库、显示出战标识）
	bool HasSummonedPal() const { return IsValid(SummonedPal); }
	int32 GetSummonedPartyIndex() const { return SummonedPartyIndex; }

	// 出战帕鲁阵亡回调（APalCharacter::HandleDeath 调用）：回写 HP=0 到槽位快照并销毁实体
	void HandleSummonedPalDeath(APalCharacter* Pal);

	// 确保有出战帕鲁（回合制进入用）：已有且存活直接返回；否则召唤第一个存活（HP>0）槽位；全灭返回 nullptr
	APalCharacter* EnsureSummonedPal();

	// 学习/更换技能：把可学池中的技能写入指定槽（仅槽 1-3，槽 0 普攻固定不可换）；
	// 该帕鲁正在出战时同步更新实体技能槽。返回 false = 索引无效 / 技能不在可学池
	bool SetPalSkill(int32 PartyIndex, int32 SlotIndex, FName SkillRowName);

	// 读取帕鲁类的可学技能池（BP 配置的 LearnableSkillRowNames）
	static TArray<FName> GetLearnablePoolFor(const FStoredPalInfo& Info);

	/** 存档恢复专用：Authority 一次性替换快照并广播。 */
	void RestoreSnapshot(const TArray<FStoredPalInfo>& InParty, const TArray<FStoredPalInfo>& InBox, int32 InActiveIndex);
	UFUNCTION()
	void HandleReplicatedStorage();

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerSwapSlots(bool bFromParty, int32 FromIndex, bool bToParty, int32 ToIndex);

	UFUNCTION(Server, Reliable)
	void ServerCycleActiveIndex(int32 Delta);

	UFUNCTION(Server, Reliable)
	void ServerSummonOrRecall();

	UFUNCTION(Server, Reliable)
	void ServerSetPalSkill(int32 PartyIndex, int32 SlotIndex, FName SkillRowName);

private:
	// 尝试放入指定数组第一个空槽（成功落盘+广播，返回 true）
	bool TryAddToArray(FReplicatedStoredPalList& Array, const FStoredPalInfo& Info, const TCHAR* ArrayName);

	// 存储回血：单个槽按比例回血（满血返回 false）
	bool ApplyStoredRegen(FStoredPalInfo& Info);

	// 回血定时器回调（事件驱动：FTimerHandle 循环触发，禁止 Tick 轮询）
	void TickRegen();

	// 回血定时器
	FTimerHandle RegenTimer;

	// 按容量保证数组槽数（BP 里改 BoxCapacity 后构造期拿不到，BeginPlay 校正）
	void EnsureSlotCounts();
	void NotifyStorageChanged();
	APawn* GetOwningPawn() const;
	bool PreparePersistentIdentity(FStoredPalInfo& Info);

	// 当前出战的帕鲁（Weak 指针避免强引用阻止销毁）与其来源槽位
	UPROPERTY(ReplicatedUsing = HandleReplicatedStorage)
	TObjectPtr<APalCharacter> SummonedPal;

	UPROPERTY(ReplicatedUsing = HandleReplicatedStorage)
	int32 SummonedPartyIndex = INDEX_NONE;
};
