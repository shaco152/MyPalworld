#include "GA_PlayerAttack.h"
#include "AbilitySystem/CaptureTags.h"
#include "Animation/AnimMontage.h"
#include "Combat/CombatLibrary.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Items/HitReactInterface.h"

UGA_PlayerAttack::UGA_PlayerAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 与输入标签绑定（同投掷能力模式）
	AbilityTags.AddTag(CaptureTags::TAG_InputTag_Attack.GetTag());
	ActivationOwnedTags.AddTag(CaptureTags::TAG_InputTag_Attack.GetTag());
}

void UGA_PlayerAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 冷却检查（时间戳方式，避免引入冷却 GE）
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < CooldownDuration)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	LastAttackTime = Now;

	APawn* Avatar = Cast<APawn>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!Avatar || !GetWorld())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 攻击动画（无 AnimInstance 时 PlayAnimMontage 内部安全跳过）
	if (ACharacter* AvatarChar = Cast<ACharacter>(Avatar))
	{
		if (AttackMontage)
		{
			AvatarChar->PlayAnimMontage(AttackMontage);
		}
	}

	// 起点/方向：优先摄像机视角，退化用角色前向
	FVector Start = Avatar->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	FVector Dir = Avatar->GetActorForwardVector();
	if (const AController* Controller = Avatar->GetController())
	{
		Start = Avatar->GetPawnViewLocation();
		Dir = Controller->GetControlRotation().Vector();
	}

	// 球形扫描命中第一个目标（忽略自身）
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, Start + Dir * AttackRange, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(AttackRadius), Params);

	if (bHit && Hit.GetActor())
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor->Implements<UHitReactInterface>())
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 玩家攻击触发 HitReact: %s"), *HitActor->GetName());
			IHitReactInterface::Execute_ReceiveHitReact(HitActor, Avatar, Hit);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 玩家攻击命中 %s，伤害 %.0f"), *HitActor->GetName(), AttackPower);
			UCombatLibrary::ApplyDamage(Avatar, HitActor, AttackPower);
		}
	}
	else
	{
		// 资源点通常是 WorldStatic/WorldDynamic，不会进入 ECC_Pawn 扫描；以 Visibility 做第二条窄扫描。
		FHitResult ReactHit;
		const bool bHitReactActor = GetWorld()->SweepSingleByChannel(ReactHit, Start, Start + Dir * AttackRange,
			FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(AttackRadius), Params);
		if (bHitReactActor && ReactHit.GetActor() && ReactHit.GetActor()->Implements<UHitReactInterface>())
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] 玩家攻击触发资源 HitReact: %s"), *ReactHit.GetActor()->GetName());
			IHitReactInterface::Execute_ReceiveHitReact(ReactHit.GetActor(), Avatar, ReactHit);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
