#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GvTGameStateBase.generated.h"

UENUM(BlueprintType)
enum class EGvTMissionPhase : uint8
{
	Waiting,
	Active,
	Results
};

UENUM(BlueprintType)
enum class EGvTMissionOutcome : uint8
{
	None,
	Success,
	Failure
};

UCLASS()
class GHOSTSVSTHIEVES_API AGvTGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	AGvTGameStateBase();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="GvT|Mission")
	EGvTMissionPhase GetMissionPhase() const { return MissionPhase; }

	UFUNCTION(BlueprintPure, Category="GvT|Mission")
	EGvTMissionOutcome GetMissionOutcome() const { return MissionOutcome; }

	UFUNCTION(BlueprintPure, Category="GvT|Mission")
	int32 GetTeamSecuredLoot() const { return TeamSecuredLoot; }

	UFUNCTION(BlueprintPure, Category="GvT|Mission")
	bool IsMainObjectiveSecured() const { return bMainObjectiveSecured; }

	UFUNCTION(BlueprintPure, Category="GvT|Mission")
	int32 GetLivingPlayerCount() const { return LivingPlayerCount; }

	UFUNCTION(BlueprintPure, Category="GvT|Mission")
	int32 GetReadyPlayerCount() const { return ReadyPlayerCount; }

	UFUNCTION(BlueprintImplementableEvent, Category="GvT|Mission")
	void OnMissionDataChanged();

	UFUNCTION(BlueprintImplementableEvent, Category="GvT|Mission")
	void OnMissionResultsReady(EGvTMissionOutcome Outcome, int32 FinalSecuredLoot);

	void SetMissionPhaseAuthority(EGvTMissionPhase NewPhase);
	void SetMissionOutcomeAuthority(EGvTMissionOutcome NewOutcome);
	void AddSecuredLootAuthority(int32 Amount, bool bWasMainObjective);
	void SetLivingPlayerCountAuthority(int32 NewCount);
	void SetReadyPlayerCountAuthority(int32 NewCount);

protected:
	UPROPERTY(ReplicatedUsing=OnRep_MissionData, BlueprintReadOnly, Category="GvT|Mission")
	EGvTMissionPhase MissionPhase = EGvTMissionPhase::Waiting;

	UPROPERTY(ReplicatedUsing=OnRep_MissionData, BlueprintReadOnly, Category="GvT|Mission")
	EGvTMissionOutcome MissionOutcome = EGvTMissionOutcome::None;

	UPROPERTY(ReplicatedUsing=OnRep_MissionData, BlueprintReadOnly, Category="GvT|Mission")
	int32 TeamSecuredLoot = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MissionData, BlueprintReadOnly, Category="GvT|Mission")
	bool bMainObjectiveSecured = false;

	UPROPERTY(ReplicatedUsing=OnRep_MissionData, BlueprintReadOnly, Category="GvT|Mission")
	int32 LivingPlayerCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MissionData, BlueprintReadOnly, Category="GvT|Mission")
	int32 ReadyPlayerCount = 0;

	UFUNCTION()
	void OnRep_MissionData();
};
