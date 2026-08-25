#include "PalStorageComponent.h"
#include "AbilitySystem/CaptureTags.h"
#include "AbilitySystem/PalAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Combat/PalAutoBattleComponent.h"
#include "Combat/PalSkillLibrary.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerState.h"
#include "Framework/FinalProjectGameInstance.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UPalStorageComponent::UPalStorageComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // 事件驱动，无 Tick
	SetIsReplicatedByDefault(true);
}

void UPalStorageComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UPalStorageComponent, PartyPals, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPalStorageComponent, BoxPals, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPalStorageComponent, ActivePartyIndex, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPalStorageComponent, SummonedPal, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPalStorageComponent, SummonedPartyIndex, COND_OwnerOnly);
}

void UPalStorageComponent::BeginPlay()
{
	Super::BeginPlay();
	PartyPals.Owner = this;
	BoxPals.Owner = this;
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// Fast Array 的结构只能由 Authority 建立。客户端若先创建本地占位项，
		// 首包会把服务器条目追加到其后，下一次增量复制将得到失效索引。
		EnsureSlotCounts();
		PartyPals.MarkAllDirty();
		BoxPals.MarkAllDirty();
	}
	else
	{
		// 兼容旧蓝图组件模板中已经序列化的空槽；保留首包中具有有效复制 ID 的条目。
		const int32 RemovedParty = PartyPals.RemoveUnreplicatedLocalItems();
		const int32 RemovedBox = BoxPals.RemoveUnreplicatedLocalItems();
		if (RemovedParty > 0 || RemovedBox > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 客户端清理本地 FastArray 占位项：Party=%d Box=%d"),
				RemovedParty, RemovedBox);
		}
	}

	// 存储回血：定时器循环触发（禁止 Tick 轮询）
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(RegenTimer, this, &UPalStorageComponent::TickRegen, StoredPalRegenInterval, true);
		}
	}
}

void UPalStorageComponent::EnsureSlotCounts()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	PartyPals.SetNum(PartyCapacity);
	BoxPals.SetNum(BoxCapacity);

	// 保证技能槽数量为 4（BP 预填测试数据可能不完整）
	for (FStoredPalInfo& Info : PartyPals)
	{
		UPalSkillLibrary::NormalizeSkillSlots(Info.SkillRowNames);
		PreparePersistentIdentity(Info);
	}
	for (FStoredPalInfo& Info : BoxPals)
	{
		UPalSkillLibrary::NormalizeSkillSlots(Info.SkillRowNames);
		PreparePersistentIdentity(Info);
	}
}

void UPalStorageComponent::TickRegen()
{
	// 到点：背包+仓库全部回血（出战中的槽位以实体属性为准，收回时才回写，跳过避免快照与实体不一致）
	bool bChanged = false;
	for (int32 i = 0; i < PartyPals.Num(); ++i)
	{
		if (i == SummonedPartyIndex)
		{
			continue;
		}
		bChanged |= ApplyStoredRegen(PartyPals[i]);
	}
	for (FStoredPalInfo& Info : BoxPals)
	{
		bChanged |= ApplyStoredRegen(Info);
	}

	if (bChanged)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 存储回血: 背包/仓库帕鲁各回复 MaxHealth 的 %.0f%%"), StoredPalRegenPercent * 100.f);
		NotifyStorageChanged();
	}
}

bool UPalStorageComponent::ApplyStoredRegen(FStoredPalInfo& Info)
{
	if (!Info.IsValid() || Info.Health >= Info.MaxHealth)
	{
		return false;
	}
	Info.Health = FMath::Min(Info.MaxHealth, Info.Health + Info.MaxHealth * StoredPalRegenPercent);
	return true;
}

bool UPalStorageComponent::TryAddToArray(FReplicatedStoredPalList& Array, const FStoredPalInfo& Info, const TCHAR* ArrayName)
{
	for (int32 i = 0; i < Array.Num(); ++i)
	{
		if (!Array[i].IsValid())
		{
			const int32 ReplicationId = Array[i].ReplicationID;
			const int32 ReplicationKey = Array[i].ReplicationKey;
			const int32 MostRecentArrayKey = Array[i].MostRecentArrayReplicationKey;
			Array[i] = Info;
			// FFastArraySerializerItem::operator= 会主动重置复制身份；固定槽位写 payload 时必须恢复。
			Array[i].ReplicationID = ReplicationId;
			Array[i].ReplicationKey = ReplicationKey;
			Array[i].MostRecentArrayReplicationKey = MostRecentArrayKey;
			Array.MarkItemDirty(Array[i]);
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 捕捉入库: %s Lv.%.0f → %s第%d槽"), *Info.PalClass->GetName(), Info.Level, ArrayName, i);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green,
					FString::Printf(TEXT("捕捉成功: %s Lv.%.0f → %s第%d槽"), *Info.PalClass->GetName(), Info.Level, ArrayName, i));
			}
			NotifyStorageChanged();
			return true;
		}
	}
	return false;
}

bool UPalStorageComponent::AddCapturedPal(const FStoredPalInfo& Info)
{
	if (!Info.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] AddCapturedPal: 无效帕鲁信息（PalClass 为空），忽略"));
		return false;
	}

	FStoredPalInfo StoredInfo = Info;
	PreparePersistentIdentity(StoredInfo);
	// 背包优先（实时投球捕捉）
	if (TryAddToArray(PartyPals, StoredInfo, TEXT("背包")))
	{
		return true;
	}
	if (TryAddToArray(BoxPals, StoredInfo, TEXT("仓库")))
	{
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] 捕捉入库失败: %s 背包与仓库均满，帕鲁被丢弃"), *Info.PalClass->GetName());
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
			FString::Printf(TEXT("背包与仓库均满，%s 被丢弃！"), *Info.PalClass->GetName()));
	}
	return false;
}

bool UPalStorageComponent::AddCapturedPalToBox(const FStoredPalInfo& Info)
{
	if (!Info.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] AddCapturedPalToBox: 无效帕鲁信息（PalClass 为空），忽略"));
		return false;
	}

	FStoredPalInfo StoredInfo = Info;
	PreparePersistentIdentity(StoredInfo);
	// 仓库优先（回合制捕捉，防现捉现用），满则背包兜底
	if (TryAddToArray(BoxPals, StoredInfo, TEXT("仓库")))
	{
		return true;
	}
	if (TryAddToArray(PartyPals, StoredInfo, TEXT("背包")))
	{
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] 回合制捕捉入库失败: %s 背包与仓库均满，帕鲁被丢弃"), *Info.PalClass->GetName());
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
			FString::Printf(TEXT("背包与仓库均满，%s 被丢弃！"), *Info.PalClass->GetName()));
	}
	return false;
}

void UPalStorageComponent::SwapSlots(bool bFromParty, int32 FromIndex, bool bToParty, int32 ToIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerSwapSlots(bFromParty, FromIndex, bToParty, ToIndex);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] SwapSlots: %s第%d槽 ↔ %s第%d槽"),
		bFromParty ? TEXT("背包") : TEXT("仓库"), FromIndex, bToParty ? TEXT("背包") : TEXT("仓库"), ToIndex);

	if (bFromParty == bToParty)
	{
		// 同侧换位（背包↔背包 / 仓库↔仓库）
		FReplicatedStoredPalList& Arr = bFromParty ? PartyPals : BoxPals;
		if (Arr.IsValidIndex(FromIndex) && Arr.IsValidIndex(ToIndex))
		{
			Arr.Swap(FromIndex, ToIndex);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] SwapSlots: 索引无效（数组长度=%d, From=%d, To=%d），未交换"), Arr.Num(), FromIndex, ToIndex);
		}
	}
	else
	{
		// 跨侧交换：背包槽 ↔ 仓库槽（空槽参与交换 = 移动）
		FReplicatedStoredPalList& FromArr = bFromParty ? PartyPals : BoxPals;
		FReplicatedStoredPalList& ToArr = bToParty ? PartyPals : BoxPals;
		if (FromArr.IsValidIndex(FromIndex) && ToArr.IsValidIndex(ToIndex))
		{
			const int32 FromId = FromArr[FromIndex].ReplicationID;
			const int32 FromKey = FromArr[FromIndex].ReplicationKey;
			const int32 FromMostRecentKey = FromArr[FromIndex].MostRecentArrayReplicationKey;
			const int32 ToId = ToArr[ToIndex].ReplicationID;
			const int32 ToKey = ToArr[ToIndex].ReplicationKey;
			const int32 ToMostRecentKey = ToArr[ToIndex].MostRecentArrayReplicationKey;
			Swap(FromArr[FromIndex], ToArr[ToIndex]);
			// 两个数组的复制 ID 均属于固定槽位；交换 payload 后恢复槽位 ID 并仅标脏这两个槽。
			FromArr[FromIndex].ReplicationID = FromId;
			FromArr[FromIndex].ReplicationKey = FromKey;
			FromArr[FromIndex].MostRecentArrayReplicationKey = FromMostRecentKey;
			ToArr[ToIndex].ReplicationID = ToId;
			ToArr[ToIndex].ReplicationKey = ToKey;
			ToArr[ToIndex].MostRecentArrayReplicationKey = ToMostRecentKey;
			FromArr.MarkItemDirty(FromArr[FromIndex]);
			ToArr.MarkItemDirty(ToArr[ToIndex]);
			UE_LOG(LogTemp, Warning, TEXT("[诊断] SwapSlots: 交换完成，来源槽现在=%s, 目标槽现在=%s"),
				*GetNameSafe(FromArr[FromIndex].PalClass.Get()), *GetNameSafe(ToArr[ToIndex].PalClass.Get()));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] SwapSlots: 索引无效（From=%d/%d, To=%d/%d），未交换"), FromIndex, FromArr.Num(), ToIndex, ToArr.Num());
		}
	}
	NotifyStorageChanged();
}

bool UPalStorageComponent::CycleActiveIndex(int32 Delta)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerCycleActiveIndex(Delta);
		return true;
	}
	if (PartyPals.Num() == 0)
	{
		return false;
	}

	const int32 Step = FMath::Sign(Delta); // 归一化为 +1 / -1
	const int32 Num = PartyPals.Num();
	if (Step == 0)
	{
		return false;
	}

	// 从当前槽出发循环找下一个有效槽（跳过空槽）
	for (int32 Offset = 1; Offset <= Num; ++Offset)
	{
		const int32 Candidate = (ActivePartyIndex + Step * Offset + Num) % Num;
		if (PartyPals[Candidate].IsValid())
		{
			if (Candidate == ActivePartyIndex)
			{
				return false; // 只有当前一个有效槽，无需切换
			}
			ActivePartyIndex = Candidate;
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 切换当前帕鲁 → 背包第%d槽 %s"), Candidate, *PartyPals[Candidate].PalClass->GetName());
			NotifyStorageChanged();
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] CycleActiveIndex: 背包全空，无法切换"));
	return false;
}

APalCharacter* UPalStorageComponent::SummonOrRecallActivePal()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerSummonOrRecall();
		return nullptr;
	}
	// 当前槽就是出战槽 → 收回（F 键切换式）
	if (IsValid(SummonedPal) && SummonedPartyIndex == ActivePartyIndex)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] F 键收回出战帕鲁 %s"), *SummonedPal->GetClass()->GetName());
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
				FString::Printf(TEXT("已收回 %s"), *SummonedPal->GetClass()->GetName()));
		}
		RecallSummonedPal();
		return nullptr;
	}

	// 其他槽的帕鲁在出战 → 先收回再召唤当前槽
	if (IsValid(SummonedPal))
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 切换出战：先收回 %s"), *SummonedPal->GetClass()->GetName());
		RecallSummonedPal();
	}

	// 当前槽必须有帕鲁
	if (!PartyPals.IsValidIndex(ActivePartyIndex) || !PartyPals[ActivePartyIndex].IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] SummonOrRecallActivePal: 当前槽 %d 为空，无法召唤"), ActivePartyIndex);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("当前背包槽没有帕鲁，无法召唤！"));
		}
		return nullptr;
	}

	APawn* Owner = GetOwningPawn();
	if (!Owner || !GetWorld())
	{
		return nullptr;
	}

	const FStoredPalInfo Info = PartyPals[ActivePartyIndex];

	// 阵亡（HP=0）的帕鲁不可召唤，待在存储中缓慢回血复活
	if (Info.Health <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] SummonOrRecallActivePal: %s 已阵亡(HP=0)，不可召唤"), *Info.PalClass->GetName());
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
				FString::Printf(TEXT("%s 已阵亡，等待恢复中..."), *Info.PalClass->GetName()));
		}
		return nullptr;
	}

	// 出生点抬高 100cm：落地流程保证干净着陆；贴地出生容易嵌进地形（表现为"回溯"式抽搐、无法移动）
	const FVector SpawnLocation = Owner->GetActorLocation() + Owner->GetActorForwardVector() * SummonDistance + FVector(0.f, 0.f, 100.f);
	const FRotator SpawnRotation = Owner->GetActorRotation();

	// 延迟生成：FinishSpawning 前打 Summoned 标签，使 BeginPlay 能稳定识别召唤身份、
	// 不按野生启动 AI（普通 SpawnActor 返回前已同步 Possess/BeginPlay）
	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APalCharacter* Pal = GetWorld()->SpawnActorDeferred<APalCharacter>(
		Info.PalClass, FTransform(SpawnRotation, SpawnLocation), Owner, nullptr, Params.SpawnCollisionHandlingOverride);
	if (!Pal)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] SummonOrRecallActivePal: 生成 %s 失败"), *Info.PalClass->GetName());
		return nullptr;
	}

	// FinishSpawning 前：出战标记（球命中时查该标签直接作废——禁止捕捉已召唤的帕鲁）
	if (UAbilitySystemComponent* ASC = Pal->GetAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag());
	}

	Pal->FinishSpawning(FTransform(SpawnRotation, SpawnLocation));

	// 召唤方向调整：显式与玩家同向（叠加模型朝向修正角；Deferred 后仍需兜底保证朝向一致）
	Pal->SetActorRotation(Pal->GetFacingRotation(Owner->GetActorForwardVector()));
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 召唤方向: %s 已与玩家同向（Yaw=%.0f, 修正=%.0f）"), *Pal->GetName(), Owner->GetActorRotation().Yaw, Pal->FacingYawOffset);

	// 恢复等级/血量/MP/技能（Init* 是纯数据写入，Spawn 后调用安全；属性集不是组件，走 APalCharacter 访问器）
	if (UPalAttributeSet* Set = Pal->GetAttributeSet())
	{
		Set->InitLevel(Info.Level);
		Set->InitMaxHealth(Info.MaxHealth);
		Set->InitHealth(FMath::Min(Info.Health, Info.MaxHealth));
		Set->InitMaxMP(Info.MaxMP);
		Set->InitMP(FMath::Min(Info.MP, Info.MaxMP));
	}
	Pal->SetSkillRowNames(Info.SkillRowNames);

	// 召唤的帕鲁自动战斗（索敌打敌意野帕鲁；普攻 = 技能槽 0）
	if (UPalAutoBattleComponent* AutoBattle = Pal->GetAutoBattleComponent())
	{
		AutoBattle->SetAutoBattleEnabled(true);
	}

	// 关键：召唤不把帕鲁从背包移除（槽位保留，F 再按收回）
	SummonedPal = Pal;
	SummonedPartyIndex = ActivePartyIndex;

	UE_LOG(LogTemp, Warning, TEXT("[诊断] 召唤成功: %s Lv.%.0f HP=%.0f/MP=%.0f（背包第%d槽保留），已打 Summoned 标签"),
		*Info.PalClass->GetName(), Info.Level, Info.Health, Info.MP, ActivePartyIndex);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
			FString::Printf(TEXT("召唤: %s Lv.%.0f 出战（再按 F 收回）"), *Info.PalClass->GetName(), Info.Level));
	}
	NotifyStorageChanged();
	return Pal;
}

void UPalStorageComponent::RecallSummonedPal()
{
	if (IsValid(SummonedPal))
	{
		// 收回前把当前 HP/MP/等级/技能回写槽位快照（自由战斗掉血/回合制耗蓝要跨召唤保留）
		if (PartyPals.IsValidIndex(SummonedPartyIndex))
		{
			FStoredPalInfo& Info = PartyPals[SummonedPartyIndex];
			if (const UPalAttributeSet* Set = SummonedPal->GetAttributeSet())
			{
				Info.Level = Set->GetLevel();
				Info.Health = Set->GetHealth();
				Info.MaxHealth = Set->GetMaxHealth();
				Info.MP = Set->GetMP();
				Info.MaxMP = Set->GetMaxMP();
				UE_LOG(LogTemp, Warning, TEXT("[诊断] 收回回写: 背包槽%d ← HP=%.0f/MP=%.0f"), SummonedPartyIndex, Info.Health, Info.MP);
			}
			Info.SkillRowNames = SummonedPal->GetSkillRowNames();
		}

		// 移除防捕捉标签后销毁实体
		if (UAbilitySystemComponent* ASC = SummonedPal->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag());
		}
		SummonedPal->Destroy();
	}
	SummonedPal = nullptr;
	SummonedPartyIndex = INDEX_NONE;
	NotifyStorageChanged();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] RecallSummonedPal: 出战帕鲁已收回销毁"));
}

APalCharacter* UPalStorageComponent::EnsureSummonedPal()
{
	// 已有存活的出战帕鲁直接用
	if (IsValid(SummonedPal) && !SummonedPal->IsDead())
	{
		return SummonedPal.Get();
	}

	// 残血/阵亡实体先清掉
	if (IsValid(SummonedPal))
	{
		RecallSummonedPal();
	}

	// 召唤第一个存活槽（HP>0；阵亡帕鲁等存储回血复活）
	for (int32 i = 0; i < PartyPals.Num(); ++i)
	{
		if (PartyPals[i].IsValid() && PartyPals[i].Health > 0.f)
		{
			ActivePartyIndex = i;
			return SummonOrRecallActivePal();
		}
	}
	return nullptr;
}

void UPalStorageComponent::HandleSummonedPalDeath(APalCharacter* Pal)
{
	if (IsValid(SummonedPal) && SummonedPal.Get() == Pal)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] HandleSummonedPalDeath: 出战帕鲁阵亡，回写 HP=0 并销毁实体"));
		// 复用收回逻辑：读属性集回写快照（此时 Health=0 → 槽位标记阵亡，不可再召唤）
		RecallSummonedPal();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] HandleSummonedPalDeath: %s 不是当前出战帕鲁，直接销毁"), *GetNameSafe(Pal));
		Pal->Destroy();
	}
}

bool UPalStorageComponent::SetPalSkill(int32 PartyIndex, int32 SlotIndex, FName SkillRowName)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerSetPalSkill(PartyIndex, SlotIndex, SkillRowName);
		return true;
	}
	if (!PartyPals.IsValidIndex(PartyIndex) || !PartyPals[PartyIndex].IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] SetPalSkill: 背包槽 %d 无效或为空"), PartyIndex);
		return false;
	}
	if (SlotIndex < 1 || SlotIndex > 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] SetPalSkill: 槽位 %d 越界（仅 1-3 可换，0 为普攻）"), SlotIndex);
		return false;
	}

	FStoredPalInfo& Info = PartyPals[PartyIndex];

	// 校验技能在可学池内（防 UI 传错行名）
	const TArray<FName> Pool = GetLearnablePoolFor(Info);
	if (SkillRowName.IsNone() || !Pool.Contains(SkillRowName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] SetPalSkill: %s 不在 %s 的可学池中，拒绝学习"), *SkillRowName.ToString(), *Info.PalClass->GetName());
		return false;
	}

	// 必须命中该帕鲁类配置的技能表精确行；不能用 Checked 兜底，否则误配行会静默变成默认普攻
	const APalCharacter* PalCDO = Info.PalClass->GetDefaultObject<APalCharacter>();
	const UDataTable* SkillTable = PalCDO ? PalCDO->SkillTable.Get() : nullptr;
	const FPalSkillRow* SkillRow = UPalSkillLibrary::GetSkillRow(SkillTable, SkillRowName);
	if (!SkillRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] SetPalSkill: %s 在 %s 的技能表中不存在，拒绝写入槽位"),
			*SkillRowName.ToString(), *Info.PalClass->GetName());
		return false;
	}
	if (SkillRow->bBasicAttack)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] SetPalSkill: %s 是普攻行，不能装入槽位 %d"),
			*SkillRowName.ToString(), SlotIndex);
		return false;
	}

	UPalSkillLibrary::NormalizeSkillSlots(Info.SkillRowNames);
	if (Info.SkillRowNames[SlotIndex] == SkillRowName)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] SetPalSkill: 背包槽 %d 的技能槽 %d 已是 %s，幂等成功（不广播）"),
			PartyIndex, SlotIndex, *SkillRowName.ToString());
		return true;
	}

	// 同一技能不得同时占据多个学习槽；UI 禁用只是体验层，这里是最终一致性边界
	for (int32 ExistingSlot = 1; ExistingSlot < UPalSkillLibrary::SkillSlotCount; ++ExistingSlot)
	{
		if (ExistingSlot != SlotIndex && Info.SkillRowNames[ExistingSlot] == SkillRowName)
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] SetPalSkill: %s 已装备在技能槽 %d，拒绝重复写入槽 %d"),
				*SkillRowName.ToString(), ExistingSlot, SlotIndex);
			return false;
		}
	}

	Info.SkillRowNames[SlotIndex] = SkillRowName;

	// 出战中的帕鲁同步到实体（回合制技能结算读实体槽位）
	if (IsValid(SummonedPal) && SummonedPartyIndex == PartyIndex)
	{
		SummonedPal->SetSkillRowNames(Info.SkillRowNames);
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] SetPalSkill: 背包槽 %d 槽位 %d ← %s"), PartyIndex, SlotIndex, *SkillRowName.ToString());
	NotifyStorageChanged();
	return true;
}

void UPalStorageComponent::RestoreSnapshot(const TArray<FStoredPalInfo>& InParty, const TArray<FStoredPalInfo>& InBox,
	int32 InActiveIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	RecallSummonedPal();
	PartyPals.Items = InParty;
	BoxPals.Items = InBox;
	ActivePartyIndex = InActiveIndex;
	EnsureSlotCounts();
	ActivePartyIndex = FMath::Clamp(ActivePartyIndex, 0, PartyPals.Num() - 1);
	NotifyStorageChanged();
}

void UPalStorageComponent::HandleReplicatedStorage()
{
	// 客户端数组长度与复制 ID 映射均由 Fast Array 管理，禁止在 RepNotify 中 SetNum。
	const int32 RemovedParty = PartyPals.RemoveUnreplicatedLocalItems();
	const int32 RemovedBox = BoxPals.RemoveUnreplicatedLocalItems();
	if (RemovedParty > 0 || RemovedBox > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 复制后清理本地 FastArray 占位项：Party=%d Box=%d"),
			RemovedParty, RemovedBox);
	}
	OnStorageChanged.Broadcast();
}

void UPalStorageComponent::NotifyStorageChanged()
{
	OnStorageChanged.Broadcast();
	if (AActor* OwnerActor = GetOwner())
	{
		if (OwnerActor->HasAuthority())
		{
			PartyPals.MarkAllDirty();
			BoxPals.MarkAllDirty();
		}
		OwnerActor->ForceNetUpdate();
	}
}

APawn* UPalStorageComponent::GetOwningPawn() const
{
	if (const APlayerState* PlayerState = Cast<APlayerState>(GetOwner()))
	{
		return PlayerState->GetPawn();
	}
	return Cast<APawn>(GetOwner());
}

void UPalStorageComponent::ServerSwapSlots_Implementation(bool bFromParty, int32 FromIndex, bool bToParty, int32 ToIndex)
{
	SwapSlots(bFromParty, FromIndex, bToParty, ToIndex);
}

void UPalStorageComponent::ServerCycleActiveIndex_Implementation(int32 Delta)
{
	CycleActiveIndex(Delta);
}

void UPalStorageComponent::ServerSummonOrRecall_Implementation()
{
	SummonOrRecallActivePal();
}

void UPalStorageComponent::ServerSetPalSkill_Implementation(int32 PartyIndex, int32 SlotIndex, FName SkillRowName)
{
	SetPalSkill(PartyIndex, SlotIndex, SkillRowName);
}

bool UPalStorageComponent::PreparePersistentIdentity(FStoredPalInfo& Info)
{
	if (!Info.IsValid())
	{
		return false;
	}
	if (!Info.PalInstanceId.IsValid() && GetOwner() && GetOwner()->HasAuthority())
	{
		Info.PalInstanceId = FGuid::NewGuid();
	}
	UFinalProjectGameInstance* GameInstance = GetWorld() ? Cast<UFinalProjectGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (Info.PalDefinitionId.IsNone() && GameInstance)
	{
		if (!GameInstance->ResolvePalDefinitionId(Info.PalClass, Info.PalDefinitionId))
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 帕鲁类 %s 无 DefinitionId 映射；可继续运行，但保存会拒绝该条目"),
				*GetNameSafe(Info.PalClass.Get()));
		}
	}
	if (!Info.PalClass && GameInstance && !Info.PalDefinitionId.IsNone())
	{
		Info.PalClass = GameInstance->ResolvePalClass(Info.PalDefinitionId);
	}
	return Info.IsValid() && Info.PalInstanceId.IsValid() && !Info.PalDefinitionId.IsNone();
}

TArray<FName> UPalStorageComponent::GetLearnablePoolFor(const FStoredPalInfo& Info)
{
	if (!Info.IsValid())
	{
		return TArray<FName>();
	}
	const APalCharacter* CDO = Info.PalClass->GetDefaultObject<APalCharacter>();
	return CDO ? CDO->LearnableSkillRowNames : TArray<FName>();
}

void FReplicatedStoredPalList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (Owner) Owner->HandleReplicatedStorage();
}
