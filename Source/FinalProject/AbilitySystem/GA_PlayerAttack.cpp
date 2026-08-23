#include "GA_PlayerAttack.h"
#include "AbilitySystem/CaptureTags.h"
#include "Animation/AnimMontage.h"
#include "Combat/CombatLibrary.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UGA_PlayerAttack::UGA_PlayerAttack()
{
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
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 玩家攻击命中 %s，伤害 %.0f"), *Hit.GetActor()->GetName(), AttackPower);
		UCombatLibrary::ApplyDamage(Avatar, Hit.GetActor(), AttackPower);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
