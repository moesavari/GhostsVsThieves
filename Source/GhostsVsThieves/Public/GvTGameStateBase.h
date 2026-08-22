#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Systems/Session/GvTLobbyTypes.h"
#include "GvTGameStateBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGvTTeamSecuredLootChanged, int32, NewTeamSecuredLoot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGvTLobbyMapChanged, EGvTPlayableMap, SelectedMap);

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

USTRUCT(BlueprintType)
struct FGvTMissionResults
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="GvT|Mission Results")
	EGvTMissionOutcome Outcome = EGvTMissionOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category="GvT|Mission Results")
	bool bMissionComplete = false;

	UPROPERTY(BlueprintReadOnly, Category="GvT|Mission Results")
	int32 SurvivingPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="GvT|Mission Results")
	int32 TotalPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="GvT|Mission Results")
	int32 ItemsStolen = 0;

	UPROPERTY(BlueprintReadOnly, Category="GvT|Mission Results")
	int32 MoneyAccumulated = 0;
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

	UFUNCTION(BlueprintPure, Category="GvT|Mission")
	FGvTMissionResults GetMissionResults() const { return MissionResults; }

	UFUNCTION(BlueprintImplementableEvent, Category="GvT|Mission")
	void OnMissionDataChanged();

	UPROPERTY(BlueprintAssignable, Category="GvT|Mission")
	FGvTTeamSecuredLootChanged OnTeamSecuredLootChanged;

	UFUNCTION(BlueprintPure, Category = "GvT|Lobby")
	EGvTPlayableMap GetLobbySelectedMap() const { return LobbySelectedMap; }

	void SetLobbySelectedMapAuthority(EGvTPlayableMap NewMap);

	UPROPERTY(BlueprintAssignable, Category = "GvT|Lobby")
	FGvTLobbyMapChanged OnLobbyMapChanged;

	UFUNCTION(BlueprintImplementableEvent, Category="GvT|Mission")
	void OnMissionResultsReady(EGvTMissionOutcome Outcome, int32 FinalSecuredLoot);

	UFUNCTION(BlueprintImplementableEvent, Category="GvT|Mission")
	void OnMissionSummaryReady(const FGvTMissionResults& Results);

	void SetMissionPhaseAuthority(EGvTMissionPhase NewPhase);
	void SetMissionOutcomeAuthority(EGvTMissionOutcome NewOutcome);
	void AddSecuredLootAuthority(int32 Amount, bool bWasMainObjective);
	void SetLivingPlayerCountAuthority(int32 NewCount);
	void SetReadyPlayerCountAuthority(int32 NewCount);
	void SetMissionResultsAuthority(const FGvTMissionResults& NewResults);

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

	UPROPERTY(ReplicatedUsing=OnRep_MissionData, BlueprintReadOnly, Category="GvT|Mission")
	FGvTMissionResults MissionResults;

	UPROPERTY(ReplicatedUsing = OnRep_LobbySelectedMap, BlueprintReadOnly, Category = "GvT|Lobby")
	EGvTPlayableMap LobbySelectedMap = EGvTPlayableMap::MVPHouse;

	UFUNCTION()
	void OnRep_MissionData();

	UFUNCTION()
	void OnRep_LobbySelectedMap();
};
