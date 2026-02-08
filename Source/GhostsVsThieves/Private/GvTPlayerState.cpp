#include "GvTPlayerState.h"
#include "Net/UnrealNetwork.h"

void AGvTPlayerState::AddLoot(int32 Amount)
{
	if (!HasAuthority())
		return;

	if (Amount == 0)
		return;

	LootValue = FMath::Max(0, LootValue + Amount);

	OnLootValueChanged.Broadcast(LootValue);

	ForceNetUpdate();
	UE_LOG(LogTemp, Warning, TEXT("AddLoot (Server): %s Loot=%d (+%d)"), *GetNameSafe(this), LootValue, Amount);

}

void AGvTPlayerState::OnRep_LootValue()
{
	OnLootValueChanged.Broadcast(LootValue);
	UE_LOG(LogTemp, Warning, TEXT("OnRep_LootValue: %s Loot=%d"), *GetNameSafe(this), LootValue);

}


void AGvTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTPlayerState, LootValue);
}
