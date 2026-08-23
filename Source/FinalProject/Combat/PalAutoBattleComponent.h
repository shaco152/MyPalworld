#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "PalAutoBattleComponent.generated.h"

class APalAIController;

UENUM(BlueprintType)
enum class EPalTargetSource : uint8
{
	Sight,
	Damage
};

UENUM(BlueprintType)
enum class EPalTargetClearReason : uint8
{
	LostSight,
	Killed,
	Invalid,
	Pause,
	Disabled
};

UENUM(BlueprintType)
enum class EPalAIDriver : uint8
{
	LegacyTimer,
	BehaviorTree
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, bool, bInCombat);

/**
 * 帕鲁自动战斗组件（事件驱动，无 Tick）：
 * - LegacyTimer：定时索敌、追击和攻击，可作为行为树资产未配置时的显式回退
 * - BehaviorTree：由感知事件、Blackboard、路径完成委托和一次性攻击 Timer 驱动
 * - 被召唤的帕鲁打敌意野帕鲁；野生敌意帕鲁 AggroRange>0 主动攻击玩家方，被伤害后反击
 * - 战斗状态变化广播 OnCombatStateChanged（头顶血条等 UI 订阅，禁止轮询）
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class FINALPROJECT_API UPalAutoBattleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPalAutoBattleComponent();

	// 启用/停用自动战斗（注意不能用 SetActive 命名：遮蔽 UActorComponent::SetActive）
	void SetAutoBattleEnabled(bool bInEnabled);
	bool IsAutoBattleEnabled() const { return bActive; }
	bool IsInCombat() const { return bInCombatState; }
	bool HasTarget() const { return Target.IsValid(); }

	// 目标唯一真相：所有感知/受击来源统一经组件收口，Blackboard 只保存镜像。
	bool AcquireTarget(AActor* Candidate, EPalTargetSource Source);
	void ClearCombatTarget(EPalTargetClearReason Reason);
	AActor* GetCombatTarget() const { return Target.Get(); }
	EPalTargetSource GetCombatTargetSource() const { return TargetSource; }
	void HandleSightLost(AActor* Actor);

	// APalCharacter::OnDamaged 的兼容入口，内部按 Damage 来源收口。
	void SetTarget(AActor* InTarget);

	// 回合制暂停/恢复：暂停当前 Driver 并清目标，恢复后按配置重启。
	void Pause();
	void Resume();
	bool IsPaused() const { return bPaused; }

	// 普攻判定：目标在 AttackRange 内且冷却完毕 → 用技能槽 0（普攻）打伤害；返回是否实际应用。
	bool TryBasicAttack(AActor* InTarget);

	// BTTask 只调用稳定域接口，不复制交战、身份或配置状态。
	void SetCombatState(bool bInCombat);
	bool IsSummonedPal() const;
	EPalAIDriver GetAIDriver() const { return AIDriver; }
	float GetSummonedAggroRange() const { return SummonedAggroRange; }
	float GetFollowDistance() const { return FollowDistance; }

	// 战斗状态变化广播（索敌成功/目标丢失/停用/暂停时触发）
	UPROPERTY(BlueprintAssignable, Category = "AutoBattle")
	FOnCombatStateChanged OnCombatStateChanged;

protected:
	// 默认保留 LegacyTimer；设置为 BehaviorTree 后由 APalAIController 运行行为树。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AutoBattle|Driver")
	EPalAIDriver AIDriver = EPalAIDriver::LegacyTimer;

	// 被召唤帕鲁的索敌半径（cm，野生帕鲁用 APalCharacter::AggroRange）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AutoBattle")
	float SummonedAggroRange = 2500.f;

	// 无目标时跟随玩家的保持距离（cm，仅被召唤的帕鲁）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AutoBattle")
	float FollowDistance = 250.f;

	// 交战移速倍率（追击时提速；帕鲁默认 600 与玩家走路同速、比冲刺慢，不提速永远追不上目标）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AutoBattle")
	float CombatSpeedMultiplier = 1.5f;

	// 索敌扫描间隔（秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AutoBattle")
	float TargetSearchInterval = 0.5f;

	// 行为更新间隔（秒）：追击路径刷新/距离判断/普攻冷却递减
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AutoBattle")
	float BehaviourUpdateInterval = 0.15f;

	// 普攻动画由技能行 RangeType + APalCharacter 的 MeleeAttackMontage/RangedAttackMontage 决定，
	// 本组件不再持有动画配置源——原 BasicAttackMontage 请迁移到帕鲁 BP 类默认值的 MeleeAttackMontage
private:
	// --- 定时器回调（TimerManager 驱动，替代 Tick）---
	void SearchForTarget();
	void UpdateBehaviour();

	// --- 工具 ---
	void ClearTargetInternal();
	void BindTargetEndPlay(AActor* InTarget);
	void UnbindTargetEndPlay();
	bool StartBehaviorTreeDriver();
	void StopBehaviorTreeDriver();
	APalAIController* GetPalAIController() const;
	void MoveToTarget(AActor* InTarget);
	void StartTimers();
	void StopTimers();
	AActor* FindNearestTarget() const;
	static float CapsuleRadiusOf(const AActor* Actor);

	UFUNCTION()
	void HandleTargetEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	bool bActive = false;
	bool bPaused = false;
	bool bInCombatState = false;
	bool bBehaviorTreeStarted = false;
	float AttackCooldown = 0.f;
	TWeakObjectPtr<AActor> Target;
	EPalTargetSource TargetSource = EPalTargetSource::Sight;

	// 基础移速缓存（首次进入交战时记录，脱离时恢复）
	float BaseWalkSpeed = 0.f;

	// 寻路请求状态（目标几乎没动且上次成功时跳过重发寻路，减少抖动）
	bool bLastMoveSucceeded = false;
	FVector LastMoveTargetLoc = FVector::ZeroVector;

	// 行为日志累计（每秒一条，排查"有目标但不攻击/不追击"用）
	float DebugLogAccumulator = 0.f;

	FTimerHandle SearchTimer;
	FTimerHandle BehaviourTimer;
};
