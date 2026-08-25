#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/CapturableInterface.h"
#include "PalCharacter.generated.h"

class UAbilitySystemComponent;
class UPalAttributeSet;
class UTexture2D;
class UDataTable;
class UPalAutoBattleComponent;
class UWidgetComponent;
class UPalHPBarWidget;
class UAnimMontage;

/** 服务器权威捕捉状态；复制到客户端以驱动隐藏、碰撞和移动表现。 */
UENUM()
enum class EPalCaptureNetState : uint8
{
	None,
	Capturing,
	Captured
};

/**
 * 帕鲁基类：继承 ACharacter（保留现有蓝图的移动/动画能力），
 * 持有 ASC + 属性集，实现可捕捉接口。
 */
UCLASS()
class FINALPROJECT_API APalCharacter : public ACharacter, public IAbilitySystemInterface, public ICapturableInterface
{
	GENERATED_BODY()

public:
	APalCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ICapturableInterface
	virtual float GetCaptureChance_Implementation() const override;
	virtual void BeginCapture_Implementation(ACaptureBall* Ball, const FVector& HitLocation) override;
	virtual void ResolveCapture_Implementation(bool bSuccess, const FVector& HitLocation) override;

	// 属性集访问器（捕捉成功入库 / 召唤恢复数值时读取等级血量用）
	UPalAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// 头像贴图（BP_Dragon 等子类蓝图里设置；捕捉入库后传给背包/仓库 UI 槽位显示，可为空 → UI 用纯色兜底）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pal|UI")
	TObjectPtr<UTexture2D> PortraitIcon;

	// 头顶血条组件（屏幕空间，Widget Class 默认 UPalHPBarWidget，BP 里可换样式类）；
	// 事件驱动（禁止 Tick）：显隐由战斗状态委托/回合制流程触发，血量由属性集 OnHealthChanged 触发
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pal|UI")
	TObjectPtr<UWidgetComponent> HPBarComp;

	// 血条显隐（AutoBattle 战斗状态委托 / 回合制流程调用）
	void SetHPBarVisible(bool bVisible);

	// 回合制强制显示开关（战斗中忽略战斗状态广播，结束后恢复）
	void SetHPBarForced(bool bForced);

	// 可学技能池（DataTable 行名集合，技能管理 UI 的学习来源；BP 类默认值里配置）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pal|Skills")
	TArray<FName> LearnableSkillRowNames;

	// 默认技能（野生帕鲁与捕捉时的初始技能；槽 0 应为普攻，槽 1-3 学习技能）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pal|Skills")
	TArray<FName> DefaultSkillRowNames;

	// 运行时技能槽（4 个：0 普攻 + 3 学习；召唤时从 FStoredPalInfo 恢复，空则取 DefaultSkillRowNames）
	const TArray<FName>& GetSkillRowNames() const { return SkillRowNames; }
	void SetSkillRowNames(const TArray<FName>& InSkills);

	// 技能表资产（DT_PalSkills，BP 里设置；自由战斗普攻与回合制技能结算都读它）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pal|Skills")
	TObjectPtr<UDataTable> SkillTable;

	// 技能行按 RangeType 选择的两套通用演出蒙太奇（帕鲁 BP 类默认值中配置）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pal|Skills|Presentation")
	TObjectPtr<UAnimMontage> MeleeAttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pal|Skills|Presentation")
	TObjectPtr<UAnimMontage> RangedAttackMontage;

	// --- 战斗 ---
	// 敌意标记（野生帕鲁默认敌意：被玩家帕鲁索敌、可卷入回合制）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Combat")
	bool bHostile = true;

	// 主动攻击范围（cm，>0 时主动攻击范围内玩家方；=0 仅被攻击后反击）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Combat")
	float AggroRange = 0.f;

	// 普攻距离（cm）与攻击间隔（秒，自由战斗用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Combat")
	float AttackRange = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|Combat")
	float AttackInterval = 1.5f;

	// 自动战斗组件（野生敌意帕鲁主动攻击/被攻击反击；被召唤时打野怪）
	UPalAutoBattleComponent* GetAutoBattleComponent() const { return AutoBattle; }

	// 死亡状态与处理（自由战斗击杀流程 / 回合制战败）
	bool IsDead() const { return bIsDead; }
	void HandleDeath();

	// 敌意查询（索敌/卷入战斗过滤用）
	bool IsHostile() const { return bHostile; }

	// 模型朝向修正角（度）：素材网格的前向与 Actor 前向不一致时在 BP 里调（如 90/-90/180），
	// 所有朝向设定（召唤/对峙摆位/自动战斗转身）都会叠加这个偏移
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pal|Combat")
	float FacingYawOffset = 0.f;

	// 面向指定方向的最终旋转（叠加 FacingYawOffset）
	FRotator GetFacingRotation(const FVector& Direction) const;

	// 被伤害回调（反击锁定攻击者）
	void OnDamaged(AActor* Attacker);

	// 初始属性配置（普通 float，BP 类默认值 / 关卡实例详情面板都可直接编辑；
	// InitAbilitySystem 时写入属性集。FGameplayAttributeData 内部成员编辑器不可改，所以配置项放这里）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|InitialAttributes")
	float InitialHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|InitialAttributes")
	float InitialMaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|InitialAttributes")
	float InitialLevel = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|InitialAttributes")
	float InitialMP = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pal|InitialAttributes")
	float InitialMaxMP = 50.f;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UPalAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UPalAutoBattleComponent> AutoBattle;

private:
	// 属性集血量变化回调（事件驱动刷新血条）
	UFUNCTION()
	void OnHealthAttributeChanged(float Health, float MaxHealth);

	// 自动战斗状态变化回调（进入/脱离交战 → 血条显隐）
	UFUNCTION()
	void OnAutoBattleCombatChanged(bool bInCombat);

	// 按当前属性集血量刷新血条百分比
	void UpdateHPBarPercent();

	// 运行时技能槽（见 GetSkillRowNames 说明）
	UPROPERTY(ReplicatedUsing = OnRep_SkillRowNames)
	TArray<FName> SkillRowNames;

	UPROPERTY(Replicated)
	bool bIsDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_CaptureNetState)
	EPalCaptureNetState CaptureNetState = EPalCaptureNetState::None;

	UFUNCTION()
	void OnRep_SkillRowNames();

	UFUNCTION()
	void OnRep_CaptureNetState();

	void ApplyCaptureNetState();

	// 回合制强制显示血条（忽略战斗状态广播）
	bool bHPBarForcedVisible = false;

	// 属性集血量委托是否已绑定（InitAbilitySystem 幂等调用）
	bool bHealthBound = false;

	// 头顶血条控件缓存（BeginPlay 里 InitWidget 后缓存，事件驱动更新）
	TWeakObjectPtr<UPalHPBarWidget> HPBarWidget;

	void InitAbilitySystem();
};
