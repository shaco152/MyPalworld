#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "FinalProjectGameInstance.generated.h"

class APalCharacter;
class UDataTable;
class UNetDriver;

/** 全局资产注册表与旅行失败收口；存档/Session 子系统均以它为宿主。 */
UCLASS()
class FINALPROJECT_API UFinalProjectGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Persistence")
	TObjectPtr<UDataTable> PalDefinitions;

	/** 例如 /Game/Mine/Maps/MainMenu；由 BP_GameInstance 配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Travel")
	FString MainMenuMapPath = TEXT("/Game/Mine/Maps/MainMenu");

	/** 例如 /Game/Mine/Test/TestMap；由 BP_GameInstance 配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Travel")
	FString GameplayMapPath = TEXT("/Game/Mine/Test/TestMap");

	bool ResolvePalDefinitionId(TSubclassOf<APalCharacter> PalClass, FName& OutDefinitionId) const;
	TSubclassOf<APalCharacter> ResolvePalClass(FName DefinitionId, bool bLoadSynchronously = true) const;

	/** 玩家主动离开世界，不显示错误提示。 */
	void ReturnToMainMenu();
	void ReturnToMainMenuWithReason(const FString& Reason);
	void NotifyMainMenuReady() { bReturningToMainMenu = false; }

private:
	void RebuildPalDefinitionCache();
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType,
		const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void OpenMainMenu(const FString& Reason, bool bShowAsError);

	/**
	 * 使用稳定的生成类软路径作为键。
	 * 裸 UClass* 在 GameInstance 提前加载后可能被 GC，地图再次加载同一路径会产生不同地址。
	 */
	TMap<FSoftObjectPath, FName> PalClassPathToId;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	bool bReturningToMainMenu = false;
};
