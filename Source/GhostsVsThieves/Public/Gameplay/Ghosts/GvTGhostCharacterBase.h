#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Gameplay/Ghosts/GvTGhostRuntimeTypes.h"
#include "GvTGhostCharacterBase.generated.h"

class APawn;
class UGvTGhostModelData;
class UGvTGhostTypeData;

UCLASS(Abstract, BlueprintType)
class GHOSTSVSTHIEVES_API AGvTGhostCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AGvTGhostCharacterBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost")
	virtual void InitializeGhost(
		FName InGhostId,
		const UGvTGhostModelData* InModelData,
		const UGvTGhostTypeData* InTypeData,
		const FGvTGhostRuntimeConfig& InRuntimeConfig);

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost")
	FName GetGhostId() const { return GhostId; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost")
	const UGvTGhostModelData* GetGhostModelData() const { return GhostModelData; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost")
	const UGvTGhostTypeData* GetGhostTypeData() const { return GhostTypeData; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost")
	const FGvTGhostRuntimeConfig& GetRuntimeConfig() const { return RuntimeConfig; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost")
	EGvTGhostBehaviorMode GetBehaviorMode() const { return BehaviorMode; }

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost")
	virtual void BeginGhostChase(APawn* Victim);

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost")
	virtual bool CanUseScareTag(FGameplayTag ScareTag) const;

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost|Chase")
	APawn* GetTargetVictim() const { return TargetVictim; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost|Presence")
	bool IsGhostPresenceActive() const { return bGhostPresenceActive; }

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost|Presence")
	virtual void SetGhostPresenceActive(bool bActive);

	virtual void IgnoreDoorCollisionForActor(AActor* DoorActor);
	virtual void IgnoreNearbyDoorCollisions(float Radius);

	UFUNCTION(BlueprintNativeEvent, Category = "GvT|Ghost|Nav")
	bool ShouldForceOpenDoors() const;
	virtual bool ShouldForceOpenDoors_Implementation() const;

	UFUNCTION(BlueprintCallable)
	virtual void BeginHauntGhostScare(AActor* Target, FGameplayTag ScareTag);

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|Ghost")
	FName GhostId = NAME_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|Ghost")
	TObjectPtr<const UGvTGhostModelData> GhostModelData = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|Ghost")
	TObjectPtr<const UGvTGhostTypeData> GhostTypeData = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|Ghost")
	FGvTGhostRuntimeConfig RuntimeConfig;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|Ghost")
	EGvTGhostBehaviorMode BehaviorMode = EGvTGhostBehaviorMode::Idle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|Ghost|Chase")
	TObjectPtr<APawn> TargetVictim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Chase", meta = (ClampMin = "0.0"))
	float RepathInterval = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Chase", meta = (ClampMin = "0.0"))
	float KillDistance = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Chase", meta = (ClampMin = "0.0"))
	float MaxChaseDistance = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Nav", meta = (ClampMin = "0.0"))
	float ChaseGoalProjectRadius = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Nav", meta = (ClampMin = "0.0"))
	float ChaseGoalAcceptRadius = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Nav", meta = (ClampMin = "0.0"))
	float DoorAssistTraceDistance = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Nav", meta = (ClampMin = "0.0"))
	float DoorOpenRetryCooldown = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Nav")
	bool bAllowGhostToForceOpenDoors = true;

	virtual void ApplyModelPresentation();
	virtual void ApplyRuntimeMovementConfig();

	virtual bool FindReachablePointNearVictim(FVector& OutPoint, bool& bOutPathIsPartial) const;
	virtual bool UpdateReachableChaseMove();
	virtual bool TryOpenBlockingDoor();
	virtual void TickGhostChase(float DeltaSeconds);
	virtual void TryCatchVictim();

	float RepathTimer = 0.f;
	float LastDoorAssistTime = -1000.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|Ghost|Nav")
	bool bLastChasePathWasPartial = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|Ghost|Presence")
	bool bGhostPresenceActive = true;
};