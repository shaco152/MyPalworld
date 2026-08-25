#include "Building/BuildingComponent.h"

#include "Building/BuildingBase.h"
#include "Components/BoxComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Items/ItemInventoryComponent.h"
#include "Framework/PlayerDataLibrary.h"
#include "Framework/FinalProjectPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UBuildingComponent::UBuildingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UBuildingComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshDataSource();

	UE_LOG(LogTemp, Warning, TEXT("[诊断] BuildingComponent 初始化: Owner=%s Catalog=%s Inventory=%s"),
		*GetNameSafe(GetOwner()), *GetNameSafe(BuildingCatalog), *GetNameSafe(CachedInventory));
}

void UBuildingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CachedInventory)
	{
		CachedInventory->OnInventoryChanged.RemoveDynamic(this, &UBuildingComponent::HandleInventoryChanged);
	}
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PreviewRefreshTimer);
	}
	DestroyPreview();
	Super::EndPlay(EndPlayReason);
}

UItemInventoryComponent* UBuildingComponent::GetInventory() const
{
	return UPlayerDataLibrary::ResolveItemInventory(GetOwner());
}

void UBuildingComponent::RefreshDataSource()
{
	UItemInventoryComponent* NewInventory = GetInventory();
	if (CachedInventory == NewInventory)
	{
		return;
	}
	if (CachedInventory)
	{
		CachedInventory->OnInventoryChanged.RemoveDynamic(this, &UBuildingComponent::HandleInventoryChanged);
	}
	CachedInventory = NewInventory;
	if (CachedInventory)
	{
		CachedInventory->OnInventoryChanged.RemoveDynamic(this, &UBuildingComponent::HandleInventoryChanged);
		CachedInventory->OnInventoryChanged.AddDynamic(this, &UBuildingComponent::HandleInventoryChanged);
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] BuildingComponent 数据源重绑: Owner=%s Inventory=%s"),
		*GetNameSafe(GetOwner()), *GetNameSafe(CachedInventory));
}

const FBuildingRecipeRow* UBuildingComponent::FindRecipe(FName BuildingId) const
{
	return BuildingCatalog && !BuildingId.IsNone()
		? BuildingCatalog->FindRow<FBuildingRecipeRow>(BuildingId, TEXT("BuildingComponent"), false)
		: nullptr;
}

TArray<FName> UBuildingComponent::GetRecipeIds() const
{
	TArray<FName> Result = BuildingCatalog ? BuildingCatalog->GetRowNames() : TArray<FName>();
	Result.Sort(FNameLexicalLess());
	return Result;
}

bool UBuildingComponent::EnterBuildMode()
{
	RefreshDataSource();
	if (State != EBuildModeState::Inactive)
	{
		return true;
	}
	if (!BuildingCatalog || !GetInventory())
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] EnterBuildMode 失败: Catalog=%s Inventory=%s"),
			*GetNameSafe(BuildingCatalog), *GetNameSafe(CachedInventory));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Red, TEXT("建造系统未配置：检查 BuildingCatalog / ItemDefinitions"));
		}
		return false;
	}

	SetState(EBuildModeState::CatalogOpen);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 已进入建造目录状态"));
	return true;
}

void UBuildingComponent::ExitBuildMode()
{
	if (State == EBuildModeState::Inactive)
	{
		return;
	}
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PreviewRefreshTimer);
	}
	DestroyPreview();
	ActiveBuildingId = NAME_None;
	PreviewYawOffset = 0.f;
	SetPreviewValidity(false);
	SetState(EBuildModeState::Inactive);
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 已退出建造模式"));
}

bool UBuildingComponent::SelectBuilding(FName BuildingId)
{
	if (State == EBuildModeState::Inactive)
	{
		return false;
	}

	const FBuildingRecipeRow* Recipe = FindRecipe(BuildingId);
	if (!Recipe || !Recipe->BuildingClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[诊断] SelectBuilding 失败: BuildingId=%s 行/BuildingClass 无效"), *BuildingId.ToString());
		return false;
	}

	DestroyPreview();
	ActiveBuildingId = BuildingId;
	PreviewYawOffset = 0.f;
	if (!SpawnPreview(*Recipe))
	{
		ActiveBuildingId = NAME_None;
		return false;
	}

	SetState(EBuildModeState::Previewing);
	RefreshPreview();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(PreviewRefreshTimer, this, &UBuildingComponent::RefreshPreview,
			FMath::Max(0.02f, PlacementRefreshInterval), true);
	}

	UE_LOG(LogTemp, Warning, TEXT("[诊断] 选择建筑 %s，虚影已生成"), *BuildingId.ToString());
	return true;
}

bool UBuildingComponent::SpawnPreview(const FBuildingRecipeRow& Recipe)
{
	if (!GetWorld() || !Recipe.BuildingClass || !GetOwner())
	{
		return false;
	}

	const FTransform InitialTransform(GetOwner()->GetActorRotation(), GetOwner()->GetActorLocation());
	ABuildingBase* SpawnedPreview = GetWorld()->SpawnActorDeferred<ABuildingBase>(Recipe.BuildingClass, InitialTransform,
		GetOwner(), Cast<APawn>(GetOwner()), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!SpawnedPreview)
	{
		return false;
	}
	SpawnedPreview->SetReplicates(false);
	SpawnedPreview->InitializePlacedBuilding(ActiveBuildingId);
	SpawnedPreview->SetPlacementPreview(true, false);
	SpawnedPreview->FinishSpawning(InitialTransform);
	SpawnedPreview->SetPlacementPreview(true, false); // BP 构造脚本结束后再次覆盖碰撞/材质
	PreviewActor = SpawnedPreview;
	return true;
}

void UBuildingComponent::DestroyPreview()
{
	if (PreviewActor.IsValid())
	{
		PreviewActor->Destroy();
	}
	PreviewActor = nullptr;
}

bool UBuildingComponent::CalculatePlacementTransform(const FBuildingRecipeRow& Recipe, FTransform& OutTransform) const
{
	if (!GetOwner() || !GetWorld())
	{
		return false;
	}

	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	const FVector Forward = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
	const FVector DesiredXY = OwnerLocation + Forward * FMath::Max(100.f, Recipe.PlacementDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BuildPreviewGround), false, GetOwner());
	if (PreviewActor.IsValid())
	{
		Params.AddIgnoredActor(PreviewActor.Get());
	}

	FHitResult GroundHit;
	const FVector TraceStart(DesiredXY.X, DesiredXY.Y, OwnerLocation.Z + GroundTraceHalfHeight);
	const FVector TraceEnd(DesiredXY.X, DesiredXY.Y, OwnerLocation.Z - GroundTraceHalfHeight);
	const bool bFoundGround = GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params);

	const FVector Location = bFoundGround
		? GroundHit.ImpactPoint + FVector(0.f, 0.f, Recipe.PlacementZOffset)
		: FVector(DesiredXY.X, DesiredXY.Y, OwnerLocation.Z + Recipe.PlacementZOffset);
	const FRotator Rotation(0.f, GetOwner()->GetActorRotation().Yaw + PreviewYawOffset, 0.f);
	OutTransform = FTransform(Rotation, Location);
	return bFoundGround;
}

bool UBuildingComponent::ValidatePlacementTransform(const FBuildingRecipeRow& Recipe, const FTransform& Candidate,
	AActor* ActorToIgnore, FString& OutReason) const
{
	UItemInventoryComponent* Inventory = GetInventory();
	if (!GetWorld() || !GetOwner() || !Recipe.BuildingClass || !Inventory)
	{
		OutReason = TEXT("建造依赖未就绪");
		return false;
	}

	const float MaxDistance = FMath::Max(100.f, Recipe.PlacementDistance) + FMath::Max(0.f, ServerPlacementTolerance);
	if (FVector::Dist2D(GetOwner()->GetActorLocation(), Candidate.GetLocation()) > MaxDistance)
	{
		OutReason = TEXT("放置位置过远");
		return false;
	}

	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(BuildValidateGround), false, GetOwner());
	if (ActorToIgnore)
	{
		GroundParams.AddIgnoredActor(ActorToIgnore);
	}
	FHitResult GroundHit;
	const FVector GroundStart = Candidate.GetLocation() + FVector::UpVector * GroundTraceHalfHeight;
	const FVector GroundEnd = Candidate.GetLocation() - FVector::UpVector * GroundTraceHalfHeight;
	if (!GetWorld()->LineTraceSingleByChannel(GroundHit, GroundStart, GroundEnd, ECC_Visibility, GroundParams))
	{
		OutReason = TEXT("下方没有可放置地面");
		return false;
	}

	const float MinGroundZ = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(MaxGroundSlopeDegrees, 0.f, 60.f)));
	if (GroundHit.ImpactNormal.Z < MinGroundZ)
	{
		OutReason = TEXT("地面坡度过大");
		return false;
	}
	const float ExpectedZ = GroundHit.ImpactPoint.Z + Recipe.PlacementZOffset;
	if (FMath::Abs(Candidate.GetLocation().Z - ExpectedZ) > ServerPlacementTolerance)
	{
		OutReason = TEXT("建筑未贴合地面");
		return false;
	}

	if (!Inventory->HasItems(Recipe.MaterialCosts))
	{
		OutReason = TEXT("建造材料不足");
		return false;
	}

	const ABuildingBase* BuildingCDO = Recipe.BuildingClass->GetDefaultObject<ABuildingBase>();
	if (!BuildingCDO)
	{
		OutReason = TEXT("建筑类默认对象无效");
		return false;
	}

	const FVector ScaleAbs = Candidate.GetScale3D().GetAbs();
	const FVector Extent = BuildingCDO->GetPlacementBoxExtent() * ScaleAbs * 0.95f;
	const FVector RelativeCenter = Candidate.GetRotation().RotateVector(BuildingCDO->GetPlacementBoxRelativeLocation() * ScaleAbs);
	const FVector Center = Candidate.GetLocation() + RelativeCenter;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(BuildValidateOverlap), false, GetOwner());
	if (ActorToIgnore)
	{
		OverlapParams.AddIgnoredActor(ActorToIgnore);
	}
	// 地形/普通地面是支撑面，不计作占位；建筑本身作为支撑时仍阻止叠放。
	if (GroundHit.GetActor() && !GroundHit.GetActor()->IsA(ABuildingBase::StaticClass()))
	{
		OverlapParams.AddIgnoredActor(GroundHit.GetActor());
	}

	TArray<FOverlapResult> Overlaps;
	const bool bBlocked = GetWorld()->OverlapMultiByObjectType(Overlaps, Center, Candidate.GetRotation(), ObjectParams,
		FCollisionShape::MakeBox(Extent), OverlapParams);
	if (bBlocked)
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (AActor* BlockingActor = Overlap.GetActor(); BlockingActor && BlockingActor != GetOwner() && BlockingActor != ActorToIgnore)
			{
				OutReason = FString::Printf(TEXT("位置被 %s 阻挡"), *BlockingActor->GetName());
				return false;
			}
		}
	}

	OutReason.Reset();
	return true;
}

void UBuildingComponent::RefreshPreview()
{
	if (State != EBuildModeState::Previewing || !PreviewActor.IsValid())
	{
		return;
	}
	const FBuildingRecipeRow* Recipe = FindRecipe(ActiveBuildingId);
	if (!Recipe)
	{
		ExitBuildMode();
		return;
	}

	FTransform Candidate;
	const bool bFoundGround = CalculatePlacementTransform(*Recipe, Candidate);
	PreviewActor->SetActorTransform(Candidate, false, nullptr, ETeleportType::TeleportPhysics);

	FString Reason;
	const bool bValid = bFoundGround && ValidatePlacementTransform(*Recipe, Candidate, PreviewActor.Get(), Reason);
	SetPreviewValidity(bValid);
	PreviewActor->SetPlacementPreview(true, bValid);
}

void UBuildingComponent::SetPreviewValidity(bool bNewValid)
{
	if (bPreviewValid == bNewValid)
	{
		return;
	}
	bPreviewValid = bNewValid;
	OnPlacementValidityChanged.Broadcast(bPreviewValid);
}

void UBuildingComponent::RotatePreview(float WheelDelta)
{
	if (!HasActivePreview() || FMath::IsNearlyZero(WheelDelta))
	{
		return;
	}
	PreviewYawOffset = FMath::UnwindDegrees(PreviewYawOffset + FMath::Sign(WheelDelta) * RotationStepDegrees);
	RefreshPreview();
}

bool UBuildingComponent::ConfirmPlacement()
{
	if (!HasActivePreview() || !bPreviewValid)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("当前位置不能建造"));
		}
		return false;
	}

	const FTransform RequestedTransform = PreviewActor->GetActorTransform();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		FString Message;
		const bool bSuccess = PlaceBuildingAuthoritative(ActiveBuildingId, RequestedTransform, Message);
		ClientPlacementResult(bSuccess, Message);
		return bSuccess;
	}

	ServerPlaceBuilding(ActiveBuildingId, RequestedTransform);
	return true;
}

void UBuildingComponent::ServerPlaceBuilding_Implementation(FName BuildingId, FTransform RequestedTransform)
{
	FString Message;
	const bool bSuccess = PlaceBuildingAuthoritative(BuildingId, RequestedTransform, Message);
	ClientPlacementResult(bSuccess, Message);
}

bool UBuildingComponent::PlaceBuildingAuthoritative(FName BuildingId, const FTransform& RequestedTransform, FString& OutMessage)
{
	RefreshDataSource();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		OutMessage = TEXT("只有服务器可以放置建筑");
		return false;
	}

	const FBuildingRecipeRow* Recipe = FindRecipe(BuildingId);
	UItemInventoryComponent* Inventory = GetInventory();
	if (!Recipe || !Recipe->BuildingClass || !Inventory)
	{
		OutMessage = TEXT("建筑配方无效");
		return false;
	}

	FString InvalidReason;
	if (!ValidatePlacementTransform(*Recipe, RequestedTransform, PreviewActor.Get(), InvalidReason))
	{
		OutMessage = InvalidReason;
		UE_LOG(LogTemp, Warning, TEXT("[诊断] 服务端拒绝建造 %s: %s"), *BuildingId.ToString(), *InvalidReason);
		return false;
	}

	ABuildingBase* Building = GetWorld()->SpawnActorDeferred<ABuildingBase>(Recipe->BuildingClass, RequestedTransform,
		GetOwner(), Cast<APawn>(GetOwner()), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Building)
	{
		OutMessage = TEXT("建筑生成失败");
		return false;
	}
	FGuid OwnerPlayerId;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const AFinalProjectPlayerState* PlayerState = OwnerPawn->GetPlayerState<AFinalProjectPlayerState>())
		{
			OwnerPlayerId = PlayerState->GetPlayerPersistentId();
		}
	}
	Building->InitializePersistentBuilding(BuildingId, FGuid::NewGuid(), OwnerPlayerId);
	Building->SetPlacementPreview(false, true);
	Building->FinishSpawning(RequestedTransform);

	if (!Inventory->ConsumeItems(Recipe->MaterialCosts))
	{
		Building->Destroy();
		OutMessage = TEXT("材料在放置前发生变化");
		return false;
	}

	OutMessage = FString::Printf(TEXT("已建造：%s"), Recipe->DisplayName.IsEmpty() ? *BuildingId.ToString() : *Recipe->DisplayName.ToString());
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 建造成功: Id=%s Actor=%s Transform=%s"),
		*BuildingId.ToString(), *GetNameSafe(Building), *RequestedTransform.ToHumanReadableString());
	return true;
}

void UBuildingComponent::ClientPlacementResult_Implementation(bool bSuccess, const FString& Message)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, bSuccess ? FColor::Green : FColor::Red, Message);
	}
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 建造结果: Success=%d Message=%s"), bSuccess, *Message);
	RefreshPreview();
}

void UBuildingComponent::HandleInventoryChanged()
{
	RefreshPreview();
}

void UBuildingComponent::SetState(EBuildModeState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	OnBuildModeStateChanged.Broadcast(State);
}
