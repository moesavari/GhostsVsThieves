#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Gameplay/Scare/GvTScareTypes.h"
#include "GameplayTagContainer.h"
#include "Containers/Map.h"
#include "Gameplay/Ghosts/GvTGhostRuntimeTypes.h"
#include "GvTDirectorSubsystem.generated.h"

class AGvTDoorActor;
class AGvTPowerBoxActor;
class UGvTGhostModelData;
class UGvTGhostTypeData;
class AGvTGhostCharacterBase;
class AGvTMirrorActor;

USTRUCT()
struct FGvTWeightedScareChoice
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag ScareTag;

	UPROPERTY()
	float Weight = 0.f;
};

UCLASS()
class GHOSTSVSTHIEVES_API UGvTDirectorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Director")
	float Heat = 0.f;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	bool DispatchScareEvent(const FGvTScareEvent& Event);

	bool DispatchScareEventSimple(const FGameplayTag& ScareTag, APawn* TargetPawn, AActor* SourceActor);

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	void StartDirector();

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	void StopDirector();

	UFUNCTION()
	void TickDirector();

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeMirrorEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeMirrorEventForActor(AActor* Target, AActor* MirrorActor) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeGhostScareEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeGhostChaseEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeLightChaseEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeRearAudioStingEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeGhostScreamEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeDoorSlamBehindEvent(AActor* Target, AActor* DoorActor) const;

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Tension")
	float GetHouseTension01() const { return HouseTension01; }

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Tension")
	float GetCurrentGlobalHauntCooldown() const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	AActor* FindBestDoorSlamDoor(AActor* Target) const;

	UFUNCTION()
	void OnPlayerInteractionEvent(AActor* Interactor, AActor* TargetActor);

	void TriggerInteractionReaction(
		APawn* Pawn,
		AActor* TargetActor,
		bool bIsElectrical,
		bool bIsValuable,
		bool bIsNoisy,
		float ItemValue01);

	AGvTPowerBoxActor* FindPowerBoxInWorld();

	UFUNCTION(BlueprintCallable, Category = "GvT|Director|Ghosts")
	void InitializeRoundGhosts();

	UFUNCTION(BlueprintCallable, Category = "GvT|Director|Ghosts")
	void ClearRoundGhosts();

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Ghosts")
	const TArray<AGvTGhostCharacterBase*>& GetActiveGhosts() const { return ActiveGhosts; }

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Ghosts")
	AGvTGhostCharacterBase* GetPrimaryGhost() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Ghosts")
	AGvTGhostCharacterBase* GetGhostById(FName GhostId) const;

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Ghosts")
	const UGvTGhostModelData* GetPrimaryGhostModel() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Ghosts")
	const UGvTGhostTypeData* GetPrimaryGhostType() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Ghosts")
	bool CanPrimaryGhostUseScareTag(FGameplayTag ScareTag) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director|Ghosts")
	void StartRoundGhostSetup();

	UFUNCTION(BlueprintCallable, Category = "GvT|Director|Ghosts")
	void EndRoundGhostSetup();

	UFUNCTION(BlueprintCallable, Category = "GvT|Director|Ghosts")
	void ConfigureRoundGhostPool(
		const TArray<UGvTGhostModelData*>& InModels,
		const TArray<UGvTGhostTypeData*>& InTypes,
		int32 InNumGhosts);

	UFUNCTION(BlueprintCallable, Category = "GvT|Director|Dispatch")
	bool DispatchGhostHunt(const FGvTGhostHuntRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "GvT|Director|Dispatch")
	bool DispatchGhostEvent(const FGvTGhostEventRequest& Request);

protected:
	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime")
	bool bEnableAutoHaunts = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime", meta = (ClampMin = "0.10"))
	float DirectorTickInterval = 4.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime", meta = (ClampMin = "0.10"))
	float GlobalHauntCooldown = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Director|Runtime")
	float LastGlobalHauntTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Mirror", meta = (ClampMin = "0.10"))
	float MirrorDuration = 1.5f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Mirror", meta = (ClampMin = "0.0"))
	float MirrorPanicAmount = 12.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Mirror")
	bool bMirrorTriggersLocalFlicker = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScare", meta = (ClampMin = "0.10"))
	float GhostScareDuration = 1.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScare", meta = (ClampMin = "0.0"))
	float GhostScarePanicAmount = 10.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScare")
	bool bGhostScareTriggersLocalFlicker = false;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostChase", meta = (ClampMin = "0.10"))
	float GhostChaseDuration = 12.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostChase", meta = (ClampMin = "0.0"))
	float GhostChasePanicAmount = 18.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostChase")
	bool bGhostChaseTriggersGroupFlicker = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.10"))
	float LightChaseDuration = 1.1f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightChasePanicAmount = 7.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "2"))
	int32 LightChaseStepCount = 5;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.01"))
	float LightChaseStepInterval = 0.16f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseStartDistance = 1800.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseEndDistance = 120.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseFlickerRadius = 350.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseAudioLeadDistance = 80.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting", meta = (ClampMin = "0.01"))
	float RearAudioStingDuration = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting", meta = (ClampMin = "0.0"))
	float RearAudioStingPanicAmount = 4.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting")
	bool bRearAudioAllowTwoShot = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RearAudioTwoShotChance = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting", meta = (ClampMin = "0.0"))
	float RearAudioBackOffset = 180.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting", meta = (ClampMin = "0.0"))
	float RearAudioSideOffset = 110.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting")
	float RearAudioUpOffset = -10.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.01"))
	float GhostScreamDuration = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.0"))
	float GhostScreamPanicAmount = 9.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.0"))
	float GhostScreamAudibleRadius = 1200.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.0"))
	float GhostScreamSpawnDistanceMin = 180.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.0"))
	float GhostScreamSpawnDistanceMax = 320.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GhostScreamHighestPanicBiasChance = 0.75f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.01"))
	float DoorSlamBehindDuration = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamBehindPanicAmount = 8.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamSearchRadius = 900.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamMaxFrontDot = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamDistanceWeight = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamBehindWeight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamPanicRadius = 450.f;

	FTimerHandle TimerHandle_DirectorTick;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float PanicTargetWeight = 0.45f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float IsolationTargetWeight = 0.30f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float NearbyNoiseTargetWeight = 0.15f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float HauntPressurePenaltyWeight = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HouseTension01 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float HouseTensionDecayPerSecond = 0.04f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float AvgPanicToTensionWeight = 0.65f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float AvgPressureToTensionWeight = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float GenericDispatchTensionImpulse = 0.08f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float MirrorDispatchTensionImpulse = 0.10f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float GhostScareDispatchTensionImpulse = 0.14f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float GhostChaseDispatchTensionImpulse = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.1"))
	float GlobalHauntCooldownMin = 4.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.1"))
	float GlobalHauntCooldownMax = 12.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float LightChaseDispatchTensionImpulse = 0.10f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension")
	bool bLogHouseTension = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float RearAudioDispatchTensionImpulse = 0.06f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float GhostScreamDispatchTensionImpulse = 0.12f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float DoorSlamDispatchTensionImpulse = 0.10f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Director|Interaction")
	float InteractionReactionMinGapSeconds = 2.25f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Director|Interaction")
	float BaseInteractionReactionChance = 0.22f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Director|Interaction")
	float LootPressureMaxLootValue = 750.f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Director|Interaction")
	float LootPressureChanceBonus = 0.30f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Director|Interaction")
	float AntiRepeatWeightMultiplier = 0.30f;

	UPROPERTY()
	TMap<TWeakObjectPtr<APawn>, float> LastInteractionReactionTime;

	UPROPERTY()
	TMap<TWeakObjectPtr<APawn>, FGameplayTag> LastInteractionScareByPlayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Director|Ghosts")
	int32 NumGhostsThisRound = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Director|Ghosts")
	TArray<TObjectPtr<UGvTGhostModelData>> AvailableGhostModels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Director|Ghosts")
	TArray<TObjectPtr<UGvTGhostTypeData>> AvailableGhostTypes;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|Director|Ghosts")
	TArray<TObjectPtr<AGvTGhostCharacterBase>> ActiveGhosts;

	bool BuildRuntimeConfig(
		const UGvTGhostModelData* ModelData,
		const UGvTGhostTypeData* TypeData,
		FGvTGhostRuntimeConfig& OutConfig) const;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Director|Mirror")
	float MirrorMaxDistance = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Director|Mirror")
	float MirrorPreferredFacingDot = -0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Director|Mirror")
	float MirrorRecentReusePenalty = 0.35f;

	bool BuildRoundGhostSpawnSpec(int32 GhostIndex, FGvTGhostRoundSpawnSpec& OutSpec) const;

	AGvTMirrorActor* ChooseMirrorForTarget(APawn* TargetPawn) const;
	bool IsMirrorValidForTarget(const AGvTMirrorActor* Mirror, const APawn* TargetPawn) const;
	float ScoreMirrorForTarget(const AGvTMirrorActor* Mirror, const APawn* TargetPawn) const;

private:
	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float BaseTargetScore = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float RecentTargetPenaltyWeight = 0.55f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float RecentTargetMemorySeconds = 20.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float TopScoreWindow = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float BusyTargetPenalty = 1.0f;

	mutable TMap<TWeakObjectPtr<APawn>, float> LastTargetedTimeSeconds;

	float ComputeIsolationScore(const APawn* Pawn) const;
	float ComputeRecentTargetPenalty01(const APawn* Pawn) const;
	void RememberTarget(APawn* Pawn);

	bool TriggerRequestedFlicker(const FGvTScareEvent& Event, class UGvTScareComponent* TargetScareComp);
	bool TryDispatchAutoScare();
	AActor* ChooseBestTarget() const;
	float GetHauntPressureForPawn(const APawn* Pawn) const;
	float ScoreTarget(APawn* Pawn) const;

	void UpdateHouseTension(float DeltaSeconds);
	float ComputeAveragePlayerPanic01() const;
	float ComputeAveragePlayerPressure01() const;
	float GetDispatchTensionImpulse(const FGvTScareEvent& Event) const;
	void ApplyHouseTensionImpulse(float Delta01);
	AActor* ChooseHighestPanicTarget() const;
	AGvTDoorActor* ChooseBestDoorSlamTarget(APawn* TargetPawn) const;
	float ScoreDoorForSlam(const APawn* TargetPawn, const AGvTDoorActor* Door) const;

	FGameplayTag ChooseWeightedInteractionScare(
		APawn* Pawn,
		AActor* TargetActor,
		bool bIsElectrical,
		bool bIsValuable,
		bool bIsNoisy,
		float ItemValue01) const;

	static FGameplayTag PickWeightedScare(const TArray<FGvTWeightedScareChoice>& Choices);
	static void AddScareWeight(TArray<FGvTWeightedScareChoice>& Choices, const FGameplayTag& Tag, float Weight);

	float ComputeInteractionReactionChance(
		APawn* Pawn,
		AActor* TargetActor,
		bool bIsElectrical,
		bool bIsValuable,
		bool bIsNoisy,
		float ItemValue01) const;

	bool IsInteractionReactionOnCooldown(APawn* Pawn) const;
	void MarkInteractionReactionTriggered(APawn* Pawn);
	void ApplyAntiRepeatBias(APawn* Pawn, TArray<FGvTWeightedScareChoice>& Choices) const;
};