#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Gameplay/Scare/GvTScareTypes.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "GvTDirectorSubsystem.generated.h"

struct FGvTPanicEvent;

class AGvTDoorActor;
class AGvTPowerBoxActor;
class AGvTGhostCharacterBase;
class AGvTHauntGhostBase;
class UGvTGhostModelData;
class UGvTGhostTypeData;
class AGvTInteractableItem;
class AGvTMirrorActor;

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

	bool DispatchScareEventSimple(const FGameplayTag& ScareTag, APawn* TargetPawn, AActor* SourceActor, bool bIgnorePanicThreshold = false);

	UFUNCTION(BlueprintCallable, Category = "GvT|Director|Ghosts")
	AGvTGhostCharacterBase* SpawnHauntGhostForTarget(APawn* TargetPawn, FGameplayTag HauntTag, TSubclassOf<AGvTGhostCharacterBase> FallbackGhostClass = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	void StartDirector();

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	void StopDirector();

	UFUNCTION()
	void TickDirector();

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeMirrorEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeCrawlerOverheadEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeCrawlerChaseEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeLightChaseEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeRearAudioStingEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeGhostScreamEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeDoorSlamBehindEvent(AActor* Target, AActor* DoorActor) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeCloseGhostScareEvent(AActor* Target) const;

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Tension")
	float GetHouseTension01() const { return HouseTension01; }

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Activity")
	float GetHouseActivity01() const { return HouseActivity01; }

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Activity")
	float GetTheftActivity01() const { return TheftActivity01; }

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Activity")
	float GetTimeActivity01() const { return TimeActivity01; }

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Panic")
	float GetPanicMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Ghosts")
	bool IsHauntActiveForDebug() const { return IsAnyHauntActive(); }

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Tension")
	float GetCurrentGlobalHauntCooldown() const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	AActor* FindBestDoorSlamDoor(AActor* Target) const;

	UFUNCTION()
	void OnPlayerInteractionEvent(AActor* Interactor, AActor* TargetActor, EGvTInteractionVerb Verb);

	void TriggerInteractionReaction(
		APawn* Pawn,
		AActor* TargetActor,
		const AGvTInteractableItem* Item,
		bool bIsElectrical,
		bool bIsValuable,
		bool bIsNoisy,
		float ItemValue01);
	void ScheduleTheftReaction(APawn* Pawn, AActor* TargetActor, const AGvTInteractableItem* Item, bool bIsElectrical, bool bIsValuable, bool bIsNoisy, float ItemValue01);
	void ExecutePendingTheftReaction();

	AGvTPowerBoxActor* FindPowerBoxInWorld();

	void ApplyPanicEventToPlayers(
		const FGvTPanicEvent& PanicEvent,
		AActor* ExcludedActor = nullptr) const;

	void ApplyDoorSlamPanicToNearbyPlayers(
		AGvTDoorActor* Door,
		AActor* InstigatorActor,
		bool bSlamSucceeded) const;

	void ApplyGlobalScarePanicToOtherPlayers(const FGvTScareEvent& Event, AActor* PrimaryTarget) const;
	void ApplyHauntStartPanicToAllPlayers(AActor* HauntSource, AActor* InstigatorActor) const;
	void SetHauntExitDoorsLocked(bool bLocked);


protected:
	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime")
	bool bEnableAutoHaunts = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime", meta = (ClampMin = "0.10"))
	float DirectorTickInterval = 4.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime", meta = (ClampMin = "0.10"))
	float GlobalHauntCooldown = 12.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Director|Runtime")
	float LastGlobalHauntTime = -1000.0f;

	// After a haunt ends, the Director may continue environmental pressure but cannot
	// immediately begin another ordinary haunt. The main objective haunt bypasses this.
	UPROPERTY(EditAnywhere, Category = "GvT|Director|Recovery", meta = (ClampMin = "0.0"))
	float PostHauntRecoveryDuration = 40.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Recovery", meta = (ClampMin = "0.0"))
	float PostHauntStrongScareBlockDuration = 15.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Recovery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PostHauntPanicMultiplier = 0.50f;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Director|Recovery")
	float LastHauntEndTime = -1000.0f;


	UPROPERTY(EditAnywhere, Category = "GvT|Director|PanicThresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MirrorEventPanicThreshold01 = 0.30f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|PanicThresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HauntChasePanicThreshold01 = 0.60f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|PanicThresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GhostScareMinPanicThreshold01 = 0.45f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MirrorActivityUnlock01 = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GhostScareActivityUnlock01 = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HauntActivityUnlock01 = 0.70f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|PanicThresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HauntChaseChanceAtThreshold01 = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|PanicThresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HauntChaseChanceAtMaxPanic01 = 0.55f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|PanicThresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MirrorChanceAtThreshold01 = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|PanicThresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MirrorChanceAtMaxPanic01 = 0.75f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|PanicThresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AutoHauntLoopInterval = 2.0f;


	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Haunt", meta = (ClampMin = "0.0"))
	float HauntStartPanicAmount = 6.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Haunt", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HauntStartPressureAmount01 = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Mirror", meta = (ClampMin = "0.10"))
	float MirrorDuration = 1.5f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Mirror", meta = (ClampMin = "0.0"))
	float MirrorPanicAmount = 7.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Mirror")
	bool bMirrorTriggersLocalFlicker = false;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerOverhead", meta = (ClampMin = "0.10"))
	float CrawlerOverheadDuration = 1.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerOverhead", meta = (ClampMin = "0.0"))
	float CrawlerOverheadPanicAmount = 6.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerOverhead")
	bool bCrawlerOverheadTriggersLocalFlicker = false;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerChase", meta = (ClampMin = "0.10"))
	float CrawlerChaseDuration = 12.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerChase", meta = (ClampMin = "0.0"))
	float CrawlerChasePanicAmount = 10.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerChase")
	bool bCrawlerChaseTriggersGroupFlicker = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.10"))
	float LightChaseDuration = 1.1f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightChasePanicAmount = 3.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightingResponseCooldown = 12.0f;

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
	float RearAudioStingPanicAmount = 3.0f;

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
	float GhostScreamPanicAmount = 6.0f;

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
	float DoorSlamBehindPanicAmount = 4.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamSearchRadius = 900.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamDistanceWeight = 1.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamPanicRadius = 700.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DoorSlamPressureAmount01 = 0.14f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamPanicCooldown = 2.0f;

	/** Chance that a selected door scare creeps instead of slamming. */
	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DoorCreakSelectionChance = 0.45f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Ghost Scare", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float CloseGhostScarePanicAmount = 7.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Ghost Scare", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CloseGhostScareSelectionChance = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|PanicScaling", meta = (ClampMin = "1.0"))
	float MaximumPanicMultiplier = 2.5f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|PanicScaling", meta = (ClampMin = "0.0"))
	float PanicTimeGrowthPerMinute = 0.10f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|PanicScaling", meta = (ClampMin = "0.0"))
	float MaximumPanicTimeGrowth = 0.50f;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director|Ghosts")
	void SetDefaultHauntGhostClass(TSubclassOf<AGvTGhostCharacterBase> InGhostClass);

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Ghosts")
	TArray<TSubclassOf<AGvTGhostCharacterBase>> HauntGhostClasses;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Ghosts")
	TArray<TObjectPtr<UGvTGhostModelData>> GhostModels;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Ghosts")
	TArray<TObjectPtr<UGvTGhostTypeData>> GhostTypes;

	// Spawn points are placed on the floor, while Character actor locations are capsule centers.
	UPROPERTY(EditAnywhere, Category = "GvT|Director|Ghosts", meta = (ClampMin = "0.0"))
	float HauntSpawnPointZOffset = 92.f;

	// Prevent debug/manual haunt key spam from creating a ghost conga line.
	UPROPERTY(EditAnywhere, Category = "GvT|Director|Ghosts", meta = (ClampMin = "0.0"))
	float ManualHauntRequestCooldown = 0.75f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Ghosts")
	TSubclassOf<AGvTGhostCharacterBase> DefaultHauntGhostClass;

	FTimerHandle TimerHandle_DirectorTick;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float PanicTargetWeight = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float IsolationTargetWeight = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float NearbyNoiseTargetWeight = 0.15f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float HauntPressurePenaltyWeight = 0.45f;


	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HouseActivity01 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TheftActivity01 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TimeActivity01 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0"))
	float TimeActivityPerSecond = 0.0035f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0"))
	float SmallTheftActivityImpulse = 0.05f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0"))
	float MediumTheftActivityImpulse = 0.09f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0"))
	float LargeTheftActivityImpulse = 0.14f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0"))
	float MainObjectiveTheftActivityImpulse = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity|TheftPanic", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SmallTheftPanic01 = 0.03f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity|TheftPanic", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MediumTheftPanic01 = 0.075f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity|TheftPanic", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LargeTheftPanic01 = 0.12f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity|TheftPanic", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MainObjectiveTheftPanic01 = 0.18f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity|TheftPanic", meta = (ClampMin = "0.0"))
	float NearbyTheftPanicRadius = 1200.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity|TheftPanic", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NearbyTheftPanicMultiplier = 0.50f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0"))
	float TheftActivityWeight = 0.45f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0"))
	float TimeActivityWeight = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0"))
	float PanicActivityWeight = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity", meta = (ClampMin = "0.0"))
	float PressureActivityWeight = 0.15f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Activity")
	bool bLogHouseActivity = true;

	bool bHouseActivityStarted = false;

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
	float CrawlerOverheadDispatchTensionImpulse = 0.14f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float CrawlerChaseDispatchTensionImpulse = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.1"))
	float GlobalHauntCooldownMin = 8.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.1"))
	float GlobalHauntCooldownMax = 14.0f;

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


private:
	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float BaseTargetScore = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float RecentTargetPenaltyWeight = 0.75f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float RecentTargetMemorySeconds = 50.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float TopScoreWindow = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float BusyTargetPenalty = 1.0f;

	mutable TMap<TWeakObjectPtr<APawn>, float> LastTargetedTimeSeconds;

	bool IsAnyHauntActive() const;
	bool IsInPostHauntRecovery() const;
	float GetPostHauntRecoveryElapsed() const;
	float GetRecoveryPanicMultiplier(const FGameplayTag& ScareTag) const;
	bool IsStrongRecoveryScare(const FGameplayTag& ScareTag) const;
	bool IsScareTagOnCooldown(const FGameplayTag& ScareTag) const;
	void RememberDispatchedScare(const FGameplayTag& ScareTag);
	float GetPerScareCooldown(const FGameplayTag& ScareTag) const;
	AGvTMirrorActor* FindEligibleMirrorForTarget(APawn* TargetPawn) const;
	AGvTHauntGhostBase* FindActiveHauntGhost() const;
	bool bHauntSpawnInProgress = false;
	bool bWasHauntActiveLastTick = false;

	float ComputeIsolationScore(const APawn* Pawn) const;
	float ComputeRecentTargetPenalty01(const APawn* Pawn) const;
	void RememberTarget(APawn* Pawn);

	bool TriggerRequestedFlicker(const FGvTScareEvent& Event, class UGvTScareComponent* TargetScareComp);
	bool IsLightingScareAvailable(const FGvTScareEvent& Event) const;
	float ScalePanicAmount(float BaseAmount) const;
	bool TryDispatchAutoScare();
	bool IsPawnEligibleForDirector(const APawn* Pawn) const;
	float GetPanicForPawn(const APawn* Pawn) const;
	bool CanScareTagRunAtPanic(const FGameplayTag& ScareTag, float Panic01) const;
	bool IsGhostScareTag(const FGameplayTag& ScareTag) const;
	bool HasActiveHaunt() const;
	AActor* ChooseBestTarget() const;
	float GetHauntPressureForPawn(const APawn* Pawn) const;
	float ScoreTarget(APawn* Pawn) const;

	void UpdateHouseTension(float DeltaSeconds);
	void UpdateHouseActivity(float DeltaSeconds);
	void RegisterUniqueTheft(const AGvTInteractableItem* Item);
	float ComputeAveragePlayerPanic01() const;
	float ComputeAveragePlayerPressure01() const;
	float GetDispatchTensionImpulse(const FGvTScareEvent& Event) const;
	void ApplyHouseTensionImpulse(float Delta01);
	AActor* ChooseHighestPanicTarget() const;
	AGvTDoorActor* ChooseBestDoorSlamTarget(APawn* TargetPawn) const;
	float ScoreDoorForSlam(const APawn* TargetPawn, const AGvTDoorActor* Door) const;
	TSubclassOf<AGvTGhostCharacterBase> ChooseHauntGhostClass() const;
	bool ChooseHauntSpawnTransform(APawn* TargetPawn, FGameplayTag HauntTag, FTransform& OutSpawnTransform) const;

	float LastLightingResponseTime = -1000.0f;
	TMap<FGameplayTag, float> LastScareDispatchTimes;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareCooldowns", meta = (ClampMin = "0.0"))
	float MirrorRepeatCooldown = 35.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareCooldowns", meta = (ClampMin = "0.0"))
	float GhostScreamRepeatCooldown = 20.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareCooldowns", meta = (ClampMin = "0.0"))
	float DefaultScareRepeatCooldown = 10.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|TheftReaction", meta = (ClampMin = "0.0"))
	float TheftReactionDelayMin = 2.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|TheftReaction", meta = (ClampMin = "0.0"))
	float TheftReactionDelayMax = 5.0f;

	FTimerHandle TimerHandle_TheftReaction;
	TWeakObjectPtr<APawn> PendingTheftPawn;
	TWeakObjectPtr<AActor> PendingTheftSource;
	bool bPendingTheftElectrical = false;
	bool bPendingTheftValuable = false;
	bool bPendingTheftNoisy = false;
	float PendingTheftValue01 = 0.f;
	int32 PendingTheftCount = 0;

	mutable TMap<TWeakObjectPtr<APawn>, TWeakObjectPtr<AGvTGhostCharacterBase>> ActiveHauntGhostByTarget;
	mutable TMap<TWeakObjectPtr<APawn>, float> LastManualHauntRequestTimeByTarget;
};
