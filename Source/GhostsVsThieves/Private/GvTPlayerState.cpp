#include "GvTPlayerState.h"
#include "Net/UnrealNetwork.h"

void AGvTPlayerState::AddLoot(int32 Amount)
{
	if (!HasAuthority())
		return;

	LootValue += Amount;
}

void AGvTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTPlayerState, LootValue);
}
