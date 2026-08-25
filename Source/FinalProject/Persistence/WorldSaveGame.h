#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Items/ItemData.h"
#include "WorldSaveGame.generated.h"

class APalCharacter;

USTRUCT(BlueprintType)
struct FINALPROJECT_API FWorldSaveMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Save")
	FGuid WorldId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Save")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Save")
	FDateTime SavedAtUtc;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Save")
	int64 TotalPlaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Save")
	int64 SaveRevision = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Save")
	FString MapPath;

	/** 世界所有者（Listen 主机）的稳定玩家 ID；旧档为空时按首条玩家记录迁移。 */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Save")
	FGuid OwnerPlayerId;
};

USTRUCT(BlueprintType)
struct FINALPROJECT_API FPalSaveRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid PalInstanceId;

	UPROPERTY(SaveGame)
	FName PalDefinitionId = NAME_None;

	/** 仅旧档迁移兜底；正常恢复以 DefinitionId 为准。 */
	UPROPERTY(SaveGame)
	TSoftClassPtr<APalCharacter> ClassFallback;

	UPROPERTY(SaveGame)
	float Level = 1.f;

	UPROPERTY(SaveGame)
	float Health = 100.f;

	UPROPERTY(SaveGame)
	float MaxHealth = 100.f;

	UPROPERTY(SaveGame)
	float MP = 50.f;

	UPROPERTY(SaveGame)
	float MaxMP = 50.f;

	UPROPERTY(SaveGame)
	TArray<FName> SkillRowNames;
};

USTRUCT(BlueprintType)
struct FINALPROJECT_API FPlayerSaveRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid PlayerPersistentId;

	UPROPERTY(SaveGame)
	FString PlayerKey;

	UPROPERTY(SaveGame)
	FTransform PawnTransform;

	UPROPERTY(SaveGame)
	float Health = 100.f;

	UPROPERTY(SaveGame)
	float MaxHealth = 100.f;

	UPROPERTY(SaveGame)
	TArray<FPalSaveRecord> PartyPals;

	UPROPERTY(SaveGame)
	TArray<FPalSaveRecord> BoxPals;

	UPROPERTY(SaveGame)
	int32 ActivePartyIndex = 0;

	UPROPERTY(SaveGame)
	TArray<FItemStack> ItemStacks;
};

USTRUCT(BlueprintType)
struct FINALPROJECT_API FBuildingSaveRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid PersistentId;

	UPROPERTY(SaveGame)
	FName BuildingTypeId = NAME_None;

	UPROPERTY(SaveGame)
	FTransform Transform;

	UPROPERTY(SaveGame)
	FGuid OwnerPlayerId;
};

UCLASS()
class FINALPROJECT_API UWorldSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 2;

	UPROPERTY(SaveGame)
	int32 SchemaVersion = CurrentSchemaVersion;

	UPROPERTY(SaveGame)
	FWorldSaveMetadata Metadata;

	UPROPERTY(SaveGame)
	TArray<FPlayerSaveRecord> Players;

	UPROPERTY(SaveGame)
	TArray<FBuildingSaveRecord> Buildings;

	bool IsStructurallyValid() const;
};

/**
 * 本机只保存身份，不保存权威角色数值。客户端加入房间时把该 ID 交给主机，
 * 主机据此在自己的世界档中恢复该玩家上次被保存的记录。
 */
UCLASS()
class FINALPROJECT_API ULocalPlayerProfileSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FGuid PlayerProfileId;
};

USTRUCT()
struct FINALPROJECT_API FWorldSaveIndexEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FWorldSaveMetadata Metadata;

	UPROPERTY(SaveGame)
	FString ActiveBuffer;
};

UCLASS()
class FINALPROJECT_API UWorldSaveIndex : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	TArray<FWorldSaveIndexEntry> Entries;
};
