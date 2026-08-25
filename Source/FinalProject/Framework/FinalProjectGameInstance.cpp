#include "Framework/FinalProjectGameInstance.h"

#include "Characters/PalCharacter.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Persistence/PalDefinitionData.h"
#include "Online/PalSessionSubsystem.h"

void UFinalProjectGameInstance::Init()
{
	Super::Init();
	RebuildPalDefinitionCache();
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UFinalProjectGameInstance::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UFinalProjectGameInstance::HandleTravelFailure);
	}
}

void UFinalProjectGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	Super::Shutdown();
}

void UFinalProjectGameInstance::RebuildPalDefinitionCache()
{
	PalClassPathToId.Reset();
	if (!PalDefinitions)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] GameInstance 未配置 DT_PalDefinitions"));
		return;
	}
	for (const FName RowName : PalDefinitions->GetRowNames())
	{
		const FPalDefinitionRow* Row = PalDefinitions->FindRow<FPalDefinitionRow>(RowName, TEXT("PalDefinitionCache"), false);
		if (!Row || Row->PalClass.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] DT_PalDefinitions 行 %s 未配置 PalClass"), *RowName.ToString());
			continue;
		}
		const FSoftObjectPath ClassPath = Row->PalClass.ToSoftObjectPath();
		if (!ClassPath.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] DT_PalDefinitions 行 %s 的 PalClass 路径无效"), *RowName.ToString());
			continue;
		}
		if (const FName* ExistingId = PalClassPathToId.Find(ClassPath))
		{
			UE_LOG(LogTemp, Warning, TEXT("[诊断] DT_PalDefinitions 类路径 %s 重复映射，保留首个 ID=%s"),
				*ClassPath.ToString(), *ExistingId->ToString());
			continue;
		}
		PalClassPathToId.Add(ClassPath, RowName);
		UE_LOG(LogTemp, Warning, TEXT("[诊断] PalDefinition 注册：Id=%s ClassPath=%s"),
			*RowName.ToString(), *ClassPath.ToString());
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalDefinition 缓存完成，定义=%d"), PalClassPathToId.Num());
}

bool UFinalProjectGameInstance::ResolvePalDefinitionId(TSubclassOf<APalCharacter> PalClass, FName& OutDefinitionId) const
{
	OutDefinitionId = NAME_None;
	if (!PalClass)
	{
		return false;
	}
	const FSoftObjectPath ClassPath(PalClass.Get());
	if (const FName* Found = PalClassPathToId.Find(ClassPath))
	{
		OutDefinitionId = *Found;
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] PalDefinition 路径未命中：Class=%s Path=%s Registered=%d"),
		*GetNameSafe(PalClass.Get()), *ClassPath.ToString(), PalClassPathToId.Num());
	return false;
}

TSubclassOf<APalCharacter> UFinalProjectGameInstance::ResolvePalClass(FName DefinitionId, bool bLoadSynchronously) const
{
	if (!PalDefinitions || DefinitionId.IsNone())
	{
		return nullptr;
	}
	const FPalDefinitionRow* Row = PalDefinitions->FindRow<FPalDefinitionRow>(DefinitionId, TEXT("ResolvePalClass"), false);
	if (!Row)
	{
		return nullptr;
	}
	return bLoadSynchronously ? Row->PalClass.LoadSynchronous() : Row->PalClass.Get();
}

void UFinalProjectGameInstance::ReturnToMainMenu()
{
	OpenMainMenu(TEXT("玩家主动返回主界面"), false);
}

void UFinalProjectGameInstance::ReturnToMainMenuWithReason(const FString& Reason)
{
	OpenMainMenu(Reason, true);
}

void UFinalProjectGameInstance::OpenMainMenu(const FString& Reason, bool bShowAsError)
{
	if (bReturningToMainMenu)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 已在返回主菜单流程中，忽略重复请求：%s"), *Reason);
		return;
	}
	bReturningToMainMenu = true;
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 返回主菜单: %s"), *Reason);
	if (bShowAsError && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Red, Reason);
	}
	if (UWorld* World = GetWorld(); World && !MainMenuMapPath.IsEmpty())
	{
		UGameplayStatics::OpenLevel(World, FName(*MainMenuMapPath));
	}
}

void UFinalProjectGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
	ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	const FString Reason = FString::Printf(TEXT("网络连接失败：%s"), *ErrorString);
	if (UPalSessionSubsystem* Sessions = GetSubsystem<UPalSessionSubsystem>())
	{
		Sessions->HandleExternalTravelFailure(Reason);
		return;
	}
	ReturnToMainMenuWithReason(Reason);
}

void UFinalProjectGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType,
	const FString& ErrorString)
{
	const FString Reason = FString::Printf(TEXT("地图旅行失败：%s"), *ErrorString);
	if (UPalSessionSubsystem* Sessions = GetSubsystem<UPalSessionSubsystem>())
	{
		Sessions->HandleExternalTravelFailure(Reason);
		return;
	}
	ReturnToMainMenuWithReason(Reason);
}
