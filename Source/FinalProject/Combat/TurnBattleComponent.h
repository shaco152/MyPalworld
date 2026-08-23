#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "TurnBattleComponent.generated.h"

class APalCharacter;
class APlayerCharacter;
class UPalBattleEnemyManager;
class UPalStorageComponent;
class UTurnBattleWidget;

UENUM(BlueprintType)
enum class ETurnBattlePhase : uint8
{
	Inactive,      // 无战斗
	PlayerAction,  // 等待玩家选择行动
	Resolving,     // 行动结算/演出中（定时器衔接）
	Victory,
	Defeat,
};

/**
 * 回合制战斗组件（挂 APlayerCharacter）：
 * P 键把附近敌意野帕鲁卷入 1v1 车轮战；我方出战帕鲁用 1 普攻 + 3 技能（技能耗 MP），
 * 行动：技能 / 切换（耗回合，死亡帕鲁不可切）/ 投球捕捉 / 通用药（回血回蓝各 10%，3 回合冷却，战斗结束清除）。
 * 血量与自由战斗共用属性集（唯一真相）：自由战斗掉血带进回合制，回合制剩余 HP 保留回第三人称。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class FINALPROJECT_API UTurnBattleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurnBattleComponent();

	// 尝试开始战斗（P 键）：搜附近敌意野帕鲁 → 卷入 → 冻结角色 → 出战斗 UI
	void TryStartBattle();

	// 状态查询（控制器冻结输入 / UI 按钮可用性判定用）
	bool IsActive() const { return Phase != ETurnBattlePhase::Inactive; }
	bool IsPlayerActionPhase() const { return Phase == ETurnBattlePhase::PlayerAction; }

	// --- 玩家行动（战斗 UI 按钮调用；非 PlayerAction 阶段自动忽略）---
	void TryUseSkill(int32 SlotIndex);
	void TrySwitchPal(int32 PartyIndex); // 切到指定背包槽（切换子页面 F 确认；耗回合，死亡/出战槽不可切）
	void TryThrowBall();   // 帕鲁球捕捉（复用捕捉概率公式）
	void TryUseMed(bool bHP);

	// --- UI 查询 ---
	APalCharacter* GetOurPal() const { return OurPal.Get(); }
	APalCharacter* GetCurrentEnemy() const;
	UPalStorageComponent* GetStorage() const;
	int32 GetHPMedCooldown() const { return HPMedCooldown; }
	int32 GetMPMedCooldown() const { return MPMedCooldown; }
	FString GetBattleMessage() const { return BattleMessage; }

	// 切换子页面输入转发（PlayerController → BattleComponent → 根 Widget → SwitchPanel）
	bool IsSwitchPanelVisible() const;
	void NavigateSwitchSelection(int32 Direction);
	void ConfirmSwitchSelection();
	void CancelSwitchSelection();

	// 战斗中右键拖动旋转观战相机（输入由 APalPlayerController 的 IA 转发到这里）
	void RotateBattleCamera(const FVector2D& Delta);

	// 通用药数值（回上限的 10%，3 回合冷却）
	static constexpr float MedRestorePercent = 0.1f;
	static constexpr int32 MedCooldownTurns = 3;

protected:
	// 卷入半径（cm，把范围内的敌意野帕鲁全部拉进战斗）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle")
	float BattlePullRadius = 5000.f;

	// 我方帕鲁对峙站位：玩家前方偏移 / 左侧偏移（cm，与玩家同方向面朝敌人）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle")
	float OurPalForwardOffset = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle")
	float OurPalSideOffset = 150.f;

	// 敌方对峙距离（cm；名单管理器保证任一时刻只显示当前对手）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle")
	float EnemyBattleDistance = 2200.f;

	// 视角 Blend 时长（秒，进入战斗切到观战相机 / 结束切回玩家）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle")
	float ViewTransitionDuration = 0.8f;

	// 观战相机离战场中点的侧向距离（cm）与离地高度（相机独立于玩家，看向双方连线中点）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle")
	float CameraSideDistance = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle")
	float BattleCameraHeight = 150.f;

	// 玩家进入战斗时向后退的距离（cm，离开画面，不承载相机）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle")
	float PlayerRetreatDistance = 800.f;

	// 右键拖动旋转观战相机的灵敏度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle")
	float CameraRotateRate = 0.3f;

	// 普通行动结算延时（秒）：未发生退场时，玩家/敌方行动消息的展示时间
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle", meta = (ClampMin = "0.1"))
	float ResolveDelay = 1.0f;

	// 敌方被击败但队列仍有下一只时的转场时间，避免击杀与下一只出手挤在同一拍
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle|Pacing", meta = (ClampMin = "0.1"))
	float EnemyDefeatedDelay = 2.5f;

	// 我方帕鲁阵亡并自动换人后的展示时间
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle|Pacing", meta = (ClampMin = "0.1"))
	float OurPalDefeatedDelay = 2.5f;

	// 胜利/失败消息保留时间，结束后才收起 UI、复位角色与敌人
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle|Pacing", meta = (ClampMin = "0.1"))
	float VictoryDisplayDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle|Pacing", meta = (ClampMin = "0.1"))
	float DefeatDisplayDuration = 3.0f;

	// 战斗 UI 类（BP 里设为 WBP_TurnBattle；为空则无法开始战斗）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TurnBattle")
	TSubclassOf<UTurnBattleWidget> BattleWidgetClass;

private:
	// --- 回合流程 ---
	void StartEnemyTurn();
	void FinishEnemyTurn();
	void ResolveVictory();
	void ResolveDefeat();
	void EndBattle();

	// 我方出战帕鲁阵亡：收回 → 自动换下一个存活帕鲁；全灭 → 战败
	void HandleOurPalDefeated();

	// 敌方当前帕鲁退场并激活下一只（击败/捕捉都真正销毁当前 Actor）
	void RemoveCurrentEnemy(bool bCaptured);

	// --- 对峙摆位 ---
	// 我方帕鲁站到玩家前方 300（面向敌人）
	void PlaceOurPalInPosition(APalCharacter* Pal);
	APlayerController* GetPlayerController() const;

	// 工具
	APlayerCharacter* GetOwnerPlayer() const;
	void RefreshWidget();
	void SetBattleMessage(const FString& Msg);

	ETurnBattlePhase Phase = ETurnBattlePhase::Inactive;

	// 敌方名单/候场/退场/原位恢复唯一所有者。
	UPROPERTY()
	TObjectPtr<UPalBattleEnemyManager> EnemyManager;

	// 我方当前出战帕鲁
	TWeakObjectPtr<APalCharacter> OurPal;

	UPROPERTY()
	TObjectPtr<UTurnBattleWidget> BattleWidget;

	// 通用药冷却（剩余回合数；每过一回合计 1，战斗结束清 0）
	int32 HPMedCooldown = 0;
	int32 MPMedCooldown = 0;

	FString BattleMessage;

	FTimerHandle EnemyTurnTimer;
	FTimerHandle EndTimer;

	// 观战相机（独立于玩家的战斗镜头；结束 Blend 回玩家后销毁）
	UPROPERTY()
	TObjectPtr<class ACameraActor> BattleCamera;

	// 进入战斗前的玩家位置（结束转回）
	FVector OriginalPlayerLocation = FVector::ZeroVector;
	bool bStoredOriginalPlayerLocation = false;
};
