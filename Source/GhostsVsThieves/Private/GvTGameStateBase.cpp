#include "GvTGameStateBase.h"
#include "Net/UnrealNetwork.h"

AGvTGameStateBase::AGvTGameStateBase()
{
	bReplicates = true;
}

void AGvTGameStateBase::SetMissionPhaseAuthority(EGvTMissionPhase NewPhase)
{
	if (!HasAuthority() || MissionPhase == NewPhase) return;
	MissionPhase = NewPhase;
	OnRep_MissionData();
}

void AGvTGameStateBase::SetMissionOutcomeAuthority(EGvTMissionOutcome NewOutcome)
{
	if (!HasAuthority() || MissionOutcome == NewOutcome) return;
	MissionOutcome = NewOutcome;
	OnRep_MissionData();
}

void AGvTGameStateBase::AddSecuredLootAuthority(int32 Amount, bool bWasMainObjective)
{
	if (!HasAuthority()) return;
	TeamSecuredLoot += FMath::Max(0, Amount);
	bMainObjectiveSecured |= bWasMainObjective;
	OnRep_MissionData();
}

void AGvTGameStateBase::SetLivingPlayerCountAuthority(int32 NewCount)
{
	if (!HasAuthority()) return;
	LivingPlayerCount = FMath::Max(0, NewCount);
	OnRep_MissionData();
}

void AGvTGameStateBase::SetReadyPlayerCountAuthority(int32 NewCount)
{
	if (!HasAuthority()) return;
	ReadyPlayerCount = FMath::Max(0, NewCount);
	OnRep_MissionData();
}

void AGvTGameStateBase::OnRep_MissionData()
{
	OnTeamSecuredLootChanged.Broadcast(TeamSecuredLoot);
	OnMissionDataChanged();
	if (MissionPhase == EGvTMissionPhase::Results && MissionOutcome != EGvTMissionOutcome::None)
	{
		OnMissionResultsReady(MissionOutcome, TeamSecuredLoot);
	}
}

void AGvTGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTGameStateBase, MissionPhase);
	DOREPLIFETIME(AGvTGameStateBase, MissionOutcome);
	DOREPLIFETIME(AGvTGameStateBase, TeamSecuredLoot);
	DOREPLIFETIME(AGvTGameStateBase, bMainObjectiveSecured);
	DOREPLIFETIME(AGvTGameStateBase, LivingPlayerCount);
	DOREPLIFETIME(AGvTGameStateBase, ReadyPlayerCount);
}
