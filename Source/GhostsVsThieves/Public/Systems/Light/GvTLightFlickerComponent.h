#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Systems/Light/GvTLightFlickerTypes.h"
#include "GvTLightFlickerComponent.generated.h"

class ULightComponent;
class UGvTLightFlickerSubsystem;
class AActor;

USTRUCT()
struct FGvTLightDefaultState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ULightComponent> Light = nullptr;

	UPROPERTY()
	float Intensity = 0.f;

	UPROPERTY()
	bool bVisible = true;

	UPROPERTY()
	FName ZoneName = NAME_None;

	UPROPERTY()
	bool bAffectedByHousePower = true;
};

UCLASS(ClassGroup = (GvT), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GHOSTSVSTHIEVES_API UGvTLightFlickerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGvTLightFlickerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "GvT|LightFlicker")
	void StartFlicker(const FGvTLightFlickerEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "GvT|LightFlicker")
	void StopFlicker();

	UFUNCTION(BlueprintCallable, Category = "GvT|LightFlicker")
	void SetHousePowerEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "GvT|LightFlicker")
	void SetZonePowerEnabled(FName ZoneName, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "GvT|LightFlicker")
	bool IsHousePowerEnabled() const { return bHousePowerEnabled; }

	/** Replaces the external light-actor list used by modular houses. */
	UFUNCTION(BlueprintCallable, Category = "GvT|LightFlicker")
	void SetExplicitLightActors(const TArray<AActor*>& InLightActors);

	/** Rebuilds cached defaults after the configured light list changes. */
	UFUNCTION(BlueprintCallable, Category = "GvT|LightFlicker")
	void RefreshLightCache();

	UFUNCTION(BlueprintPure, Category = "GvT|LightFlicker")
	int32 GetExplicitLightActorCount() const { return ExplicitLightActors.Num(); }

	FName ResolveZoneFromTags(const TArray<FName>& Tags) const;
	bool HasTag(const TArray<FName>& Tags, FName TagName) const;

	void GetCachedLightLocations(TArray<FVector>& OutLocations) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker")
	bool bAutoCollectOwnerLightComponents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker")
	TArray<TObjectPtr<ULightComponent>> ExplicitLights;

	/** Lights may live on separate level actors; a modular house does not need one combined mesh actor. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "GvT|LightFlicker")
	TArray<TObjectPtr<AActor>> ExplicitLightActors;

	UPROPERTY(Transient)
	TArray<FGvTLightDefaultState> CachedLights;

	UPROPERTY(Transient)
	bool bHousePowerEnabled = true;

	UPROPERTY(Transient)
	TMap<FName, bool> ZonePowerOverrides;

private:
	bool bIsFlickering = false;
	float FlickerEndTime = 0.f;
	float NextPulseTime = 0.f;
	FGvTLightFlickerEvent ActiveEvent;
	FRandomStream FlickerStream;

	void CacheLights();
	void ApplyPulse();
	void RestoreDefaults();
	void GatherLightsFromActor(AActor* Actor, TArray<ULightComponent*>& OutLights) const;
	void GatherLightsFromChildActorComponent(UChildActorComponent* ChildComp);
	void ApplyCurrentPowerState();
};
