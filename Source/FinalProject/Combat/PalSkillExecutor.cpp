#include "Combat/PalSkillExecutor.h"

#include "AbilitySystem/PalAttributeSet.h"
#include "Characters/PalCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Combat/CombatLibrary.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* RangeTypeName(const EPalSkillRangeType RangeType)
	{
		return RangeType == EPalSkillRangeType::Ranged ? TEXT("Ranged") : TEXT("Melee");
	}
}

const UPalSkillExecutor* UPalSkillExecutor::GetExecutorCDO(const FPalSkillRow& Skill)
{
	UClass* ExecutorClass = Skill.ExecutorClass
		? Skill.ExecutorClass.Get()
		: UPalSkillDirectDamageExecutor::StaticClass();

	const UPalSkillExecutor* Executor = ExecutorClass
		? Cast<UPalSkillExecutor>(ExecutorClass->GetDefaultObject())
		: nullptr;
	if (!Executor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 技能执行器解析失败: 配置类=%s，回退 DirectDamage"), *GetNameSafe(ExecutorClass));
		Executor = GetDefault<UPalSkillDirectDamageExecutor>();
	}
	return Executor;
}

FPalSkillExecutionResult UPalSkillExecutor::ExecuteImmediate(const FPalSkillContext& Context) const
{
	FPalSkillExecutionResult Result;
	if (!Context.Source || !Context.Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 技能执行失败: Source=%s Target=%s Skill=%s"),
			*GetNameSafe(Context.Source), *GetNameSafe(Context.Target), *Context.SkillRowName.ToString());
		return Result;
	}

	const float Damage = CalculateDamage(Context);
	if (Damage <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 技能执行跳过: %s 伤害=%.2f"), *Context.SkillRowName.ToString(), Damage);
		return Result;
	}

	PlayPresentation(Context);
	Result = ApplyEffect(Context, Damage);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 技能执行: Source=%s Target=%s Skill=%s Range=%s Executor=%s Damage=%.0f Applied=%d Killed=%d"),
		*GetNameSafe(Context.Source), *GetNameSafe(Context.Target), *Context.SkillRowName.ToString(),
		RangeTypeName(Context.Skill.RangeType), *GetClass()->GetName(), Result.Damage,
		static_cast<int32>(Result.bApplied), static_cast<int32>(Result.bKilled));
	return Result;
}

float UPalSkillExecutor::CalculateDamage(const FPalSkillContext& Context) const
{
	const UPalAttributeSet* SourceSet = Context.Source ? Context.Source->GetAttributeSet() : nullptr;
	const float SourceLevel = SourceSet ? SourceSet->GetLevel() : 0.f;
	return FMath::Max(0.f, Context.Skill.Power + SourceLevel * Context.Skill.DamagePerLevel);
}

void UPalSkillExecutor::PlayPresentation(const FPalSkillContext& Context) const
{
	if (!Context.Source || !Context.Target)
	{
		return;
	}

	UAnimMontage* Montage = Context.Skill.RangeType == EPalSkillRangeType::Ranged
		? Context.Source->RangedAttackMontage.Get()
		: Context.Source->MeleeAttackMontage.Get();
	if (Montage)
	{
		Context.Source->PlayAnimMontage(Montage);
	}

	if (Context.Skill.Effect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(Context.Target->GetWorld(), Context.Skill.Effect, Context.Target->GetActorTransform());
	}
	if (Context.Skill.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(Context.Source, Context.Skill.Sound, Context.Source->GetActorLocation());
	}
}

FPalSkillExecutionResult UPalSkillExecutor::ApplyEffect(const FPalSkillContext& Context, const float Damage) const
{
	FPalSkillExecutionResult Result;
	Result.Damage = Damage;

	// 与 CombatLibrary 当前 GE 支持面保持一致；其他 Actor 不进入伤害入口
	if (!Context.Source || !Context.Target || Damage <= 0.f ||
		(!Context.Target->IsA<APalCharacter>() && !Context.Target->IsA<APlayerCharacter>()))
	{
		return Result;
	}

	Result.bApplied = true;
	Result.bKilled = UCombatLibrary::ApplyDamage(Context.Source, Context.Target, Damage);
	return Result;
}
