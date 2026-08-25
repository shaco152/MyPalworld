#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CaptureBall.generated.h"

class USphereComponent;
class USkeletalMeshComponent;
class UProjectileMovementComponent;
class UWidgetComponent;
class UCaptureWidget;

UENUM(BlueprintType)
enum class ECaptureBallState : uint8
{
	Flying,     // 飞行中
	Capturing,  // 命中帕鲁：悬停晃动 + 判定中
	Succeeding, // 捕捉成功：等待圆环填充到 100% 后结算销毁
	Resolving,  // 应用结果
	Done        // 结束
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCaptureStarted, float, Chance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCaptureResolved, bool, bSuccess, float, Chance, float, Roll);

/**
 * 帕鲁球：投射物命中可捕捉对象后立即悬停晃动，
 * 判定概率并把"概率 / 结果 / 判定值"广播给挂载的捕捉控件。
 */
UCLASS()
class FINALPROJECT_API ACaptureBall : public AActor
{
	GENERATED_BODY()

public:
	ACaptureBall();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 捕捉控件绑定这两个委托来显示概率与判定结果
	UPROPERTY(BlueprintAssignable, Category = "Capture")
	FOnCaptureStarted OnCaptureStarted;

	UPROPERTY(BlueprintAssignable, Category = "Capture")
	FOnCaptureResolved OnCaptureResolved;

	// 挂到球上的捕捉控件类：直接用子组件 WidgetComp 的 "Widget Class" 属性设置（蓝图中设为 WBP_CaptureUI）

	// 晃动参数（每次晃动一个阶段，共两次判定，各约 1 秒）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture|Shake")
	float ShakeDuration = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture|Shake")
	float ShakeAmplitudeDegrees = 12.f;

	// 每秒完整震荡次数
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture|Shake")
	float ShakeFrequency = 6.f;

	// 上下浮动幅度（厘米），球体纯旋转看不出晃动，配合浮动才明显
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture|Shake")
	float ShakeBobAmplitude = 8.f;

	// 第一次判定成功后，第二判概率至少为该值（如 10% 中了 → 第二判按 60% 判）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Capture|Judgment")
	float SecondRollChance = 0.6f;

	UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> WidgetComp;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	UFUNCTION()
	void OnRep_CapturePresentation();

	void ApplyCapturePresentation();
	void UpdateShakePresentation(float DeltaSeconds);
	void ResetMeshPresentation();
	void StartCapture(AActor* Pal, const FHitResult& Hit);
	void ApplyCaptureOutcome();

	UPROPERTY(ReplicatedUsing = OnRep_CapturePresentation)
	ECaptureBallState State = ECaptureBallState::Flying;
	TWeakObjectPtr<AActor> CapturedPal;
	TWeakObjectPtr<UCaptureWidget> CaptureWidgetRef; // 控件实例（球的 Tick 驱动其平滑填充）
	FVector HitLocation = FVector::ZeroVector;
	FVector PalOriginalLocation = FVector::ZeroVector; // 捕捉前帕鲁位置（失败复位用，避免跳到命中点）
	FTransform MeshInitialRelativeTransform;
	ECaptureBallState LastPresentationState = ECaptureBallState::Flying;
	float PresentationShakeElapsed = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_CapturePresentation)
	float CaptureChance = 0.f;   // 当前阶段判定概率（第一次成功后提高）
	float RollValue = 0.f;       // 最近一次判定值
	bool bCaptureSucceeded = false; // 最终结果（两次判定）
	int32 ShakePhase = 1;        // 当前晃动阶段：1 = 第一次判定，2 = 第二次判定
	float ShakeElapsed = 0.f;
	float SucceedElapsed = 0.f; // 成功阶段等待填充完成的计时（兜底超时用）
};
