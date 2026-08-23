#include "GA_ThrowPalSphere.h"
#include "AbilitySystem/CaptureTags.h"
#include "Actors/CaptureBall.h"
#include "Engine/Engine.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"

UGA_ThrowPalSphere::UGA_ThrowPalSphere()
{
	// 与输入标签绑定：AbilityTags / ActivationOwnedTags 任一命中即可被 TryActivateAbilitiesByTag 激活
	AbilityTags.AddTag(CaptureTags::TAG_InputTag_Throw.GetTag());
	ActivationOwnedTags.AddTag(CaptureTags::TAG_InputTag_Throw.GetTag());

	BallClass = ACaptureBall::StaticClass();
}

void UGA_ThrowPalSphere::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("[诊断] 投掷能力已激活！"));
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("[诊断] CommitAbility 失败！"));
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	APawn* Avatar = Cast<APawn>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!Avatar || !GetWorld())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 瞄准方向：优先取控制器旋转，退化用角色前向
	FVector AimDir = Avatar->GetActorForwardVector();
	FVector SpawnLocation = Avatar->GetActorLocation() + AimDir * 100.f + FVector(0.f, 0.f, 80.f);
	if (const AController* Controller = Avatar->GetController())
	{
		AimDir = Controller->GetControlRotation().Vector();
		SpawnLocation = Avatar->GetPawnViewLocation() + AimDir * 120.f;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Avatar;
	SpawnParams.Instigator = Avatar;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACaptureBall* Ball = GetWorld()->SpawnActor<ACaptureBall>(BallClass, SpawnLocation, AimDir.Rotation(), SpawnParams);
	if (Ball)
	{
		if (Ball->GetProjectileMovement())
		{
			Ball->GetProjectileMovement()->Velocity = AimDir * ThrowSpeed;
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, FString::Printf(TEXT("[诊断] 球已生成: %s (类 %s)"), *Ball->GetName(), BallClass ? *BallClass->GetName() : TEXT("无")));
		}
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, FString::Printf(TEXT("[诊断] 球生成失败！BallClass=%s"), BallClass ? *BallClass->GetName() : TEXT("无")));
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
