#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h" // FAIStimulus
#include "PalAIController.generated.h"

class UBehaviorTree;
class UBehaviorTreeComponent;
class UBlackboardComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

/**
 * 帕鲁 AI 控制器：只负责感知、行为树生命周期与决策数据镜像。
 * 目标与战斗状态唯一真相在 UPalAutoBattleComponent（AcquireTarget/HandleSightLost 收口），
 * Blackboard 仅作镜像；本类不持有技能/属性/冷却或第二份目标。
 */
UCLASS()
class FINALPROJECT_API APalAIController : public AAIController
{
	GENERATED_BODY()

public:
	APalAIController();

	// --- 由自动战斗组件调用的行为控制窄接口（组件为战斗状态唯一真相，本类只做决策数据镜像）---
	bool StartPalBehavior(bool bIsSummoned);
	void StopPalBehavior();
	void PausePalBehavior();
	void ResumePalBehavior();

	// Blackboard 镜像写手（组件为唯一真相，经本窄接口镜像，不得直接写 BB 目标）
	void SetBBTarget(AActor* TargetActor);
	void ClearBBTarget();
	void SetReturnHomeRequested(bool bRequested);
	void SetSummonedRole(bool bIsSummoned);

	const FVector& GetHomeLocation() const { return HomeLocation; }
	float GetPatrolRadius() const { return PatrolRadius; }

protected:
	virtual void OnPossess(APawn* InPawn) override;

	// 行为树资产只在 Controller 配置（BP_PalAIController 上挂载）；PalCharacter 不重复持有引用
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pal|AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	// 巡逻半径（cm）：围绕 HomeLocation 取随机可达点
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pal|AI", meta = (ClampMin = "0"))
	float PatrolRadius = 1200.f;

private:
	// 蓝图 CDO 在反射/热重载后可能丢失默认子对象属性引用；OnPossess 前恢复或重建，禁止空指针崩溃。
	bool EnsurePerceptionRuntime();

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// Sight 候选过滤（主动野生=玩家方；被动 AggroRange=0 拒绝主动获取；召唤=敌意野生；过滤死亡/捕捉/Battling/自身/同侧）
	bool ShouldAcquireFromSight(AActor* Candidate) const;

	UPROPERTY(VisibleAnywhere, Category = "Pal|AI")
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComp;

	UPROPERTY(VisibleAnywhere, Category = "Pal|AI")
	TObjectPtr<UBlackboardComponent> BlackboardComp;

	UPROPERTY(VisibleAnywhere, Category = "Pal|AI")
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;

	UPROPERTY(VisibleAnywhere, Category = "Pal|AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	// 出生点锚点（OnPossess 记录一次，本实例不再漂移）
	FVector HomeLocation = FVector::ZeroVector;
};
