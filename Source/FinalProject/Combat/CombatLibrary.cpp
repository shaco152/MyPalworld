#include "CombatLibrary.h"
#include "AbilitySystem/CaptureTags.h"
#include "AbilitySystem/GE_Damage.h"
#include "AbilitySystem/GE_PlayerDamage.h"
#include "AbilitySystem/PalAttributeSet.h"
#include "AbilitySystem/PlayerAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Characters/PalCharacter.h"
#include "Characters/PlayerCharacter.h"

bool UCombatLibrary::ApplyDamage(AActor* Source, AActor* Target, float Amount)
{
	if (!Target || Amount <= 0.f)
	{
		return false;
	}
	if (!Target->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] ApplyDamage 拒绝客户端结算：Target=%s"), *GetNameSafe(Target));
		return false;
	}

	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Target);
	if (!ASI || !ASI->GetAbilitySystemComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] ApplyDamage: 目标 %s 无 ASC，伤害无效"), *GetNameSafe(Target));
		return false;
	}
	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();

	// 按目标类型选伤害 GE（帕鲁 → Pal 属性集；玩家 → Player 属性集）
	const UGameplayEffect* DamageGE = nullptr;
	if (Target->IsA(APalCharacter::StaticClass()))
	{
		DamageGE = GetDefault<UGE_Damage>();
	}
	else if (Target->IsA(APlayerCharacter::StaticClass()))
	{
		DamageGE = GetDefault<UGE_PlayerDamage>();
	}
	if (!DamageGE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] ApplyDamage: 目标 %s 不是可受伤类型"), *GetNameSafe(Target));
		return false;
	}

	// 友伤保护：玩家不打自己召唤的帕鲁
	if (Source && Source->IsA(APlayerCharacter::StaticClass()) && Target->IsA(APalCharacter::StaticClass()) &&
		ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Pal_Summoned.GetTag()))
	{
		return false;
	}

	FGameplayEffectSpec Spec(DamageGE, ASC->MakeEffectContext(), 1.f);
	// 关键：GE 修饰符是 Additive，扣血必须传负值（传正数会变成加血！）
	Spec.SetSetByCallerMagnitude(FName(TEXT("Damage")), -Amount);
	ASC->ApplyGameplayEffectSpecToSelf(Spec);

	// 落盘诊断：伤害前后血量（排查"打了不掉血"用）
	float AfterHP = 1.f;
	if (const APalCharacter* Pal = Cast<APalCharacter>(Target))
	{
		AfterHP = Pal->GetAttributeSet() ? Pal->GetAttributeSet()->GetHealth() : 1.f;
	}
	else if (const APlayerCharacter* Player = Cast<APlayerCharacter>(Target))
	{
		AfterHP = Player->GetAttributeSet() ? Player->GetAttributeSet()->GetHealth() : 1.f;
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] ApplyDamage: %s → %s 伤害 %.0f, 结算后 HP=%.0f"), *GetNameSafe(Source), *GetNameSafe(Target), Amount, AfterHP);

	// 反击：帕鲁被伤害后锁定攻击者（被动反击，即使 AggroRange=0）
	if (APalCharacter* Pal = Cast<APalCharacter>(Target))
	{
		if (!Pal->IsDead() && Source)
		{
			Pal->OnDamaged(Source);
		}
	}

	// 击杀判定（属性集 PostGameplayEffectExecute 已钳制到 [0, Max]）
	float HP = 1.f;
	if (const APalCharacter* Pal = Cast<APalCharacter>(Target))
	{
		HP = Pal->GetAttributeSet() ? Pal->GetAttributeSet()->GetHealth() : 1.f;
	}
	else if (const APlayerCharacter* Player = Cast<APlayerCharacter>(Target))
	{
		HP = Player->GetAttributeSet() ? Player->GetAttributeSet()->GetHealth() : 1.f;
	}

	const bool bKilled = HP <= 0.f;
	if (bKilled)
	{
		// 回合制中的目标由战斗流程处理退场（不在此销毁/重生）
		if (!ASC->HasMatchingGameplayTag(CaptureTags::TAG_State_Battle_Battling.GetTag()))
		{
			HandleTargetKilled(Target);
		}
	}
	return bKilled;
}

void UCombatLibrary::HandleTargetKilled(AActor* Target)
{
	if (APalCharacter* Pal = Cast<APalCharacter>(Target))
	{
		Pal->HandleDeath();
	}
	else if (APlayerCharacter* Player = Cast<APlayerCharacter>(Target))
	{
		Player->HandleDeath();
	}
}
