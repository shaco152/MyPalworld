#include "Framework/FinalProjectGameState.h"

#include "Net/UnrealNetwork.h"

void AFinalProjectGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFinalProjectGameState, LoadState);
}

void AFinalProjectGameState::SetLoadState(EWorldLoadState NewState)
{
	if (!HasAuthority() || LoadState == NewState)
	{
		return;
	}
	LoadState = NewState;
	OnLoadStateChanged.Broadcast(LoadState);
	ForceNetUpdate();
	UE_LOG(LogTemp, Warning, TEXT("[诊断] 世界 LoadState=%d"), static_cast<int32>(LoadState));
}

void AFinalProjectGameState::OnRep_LoadState()
{
	OnLoadStateChanged.Broadcast(LoadState);
}
