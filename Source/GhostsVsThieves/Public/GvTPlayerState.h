#pragma once

#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "GameFramework/PlayerState.h"
#include "GvTPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLootValueChanged, int32, NewLootValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPanicChanged, float, NewPanic01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHauntPressureChanged, float, NewPressure01);

UENUM(BlueprintType)
enum class EGvTPanicSource : uint8
{
	None					UMETA(DisplayName = "None"),
	ItemPickup				UMETA(DisplayName = "Item Pickup"),
	ItemPickupHighValue		UMETA(DisplayName = "Item Pickup High Value"),
	ItemPickupHaunted		UMETA(DisplayName = "Item Pickup Haunted"),
	ScannerRevealHaunted	UMETA(DisplayName = "Scanner Reveal Haunted"),
	DoorSlam				UMETA(DisplayName = "Door Slam"),
	LightFlicker			UMETA(DisplayName = "Light Flicker"),
	LightShutdown			UMETA(DisplayName = "Light Shutdown"),
	PowerOutage				UMETA(DisplayName = "Power Outage"),
	PowerRestore			UMETA(DisplayName = "Power Restore"),
	MirrorScare				UMETA(DisplayName = "Mirror Scare"),
	CrawlerOverhead			UMETA(DisplayName = "Crawler Overhead"),
	CrawlerChaseStart		UMETA(DisplayName = "Crawler Chase Start"),
	CrawlerChaseTick		UMETA(DisplayName = "Crawler Chase Tick"),
	RearAudioSting			UMETA(DisplayName = "Rear Audio Sting"),
	GhostScream				UMETA(DisplayName = "Ghost Scream"),
	DeathRipple				UMETA(DisplayName = "Death Ripple")
};

USTRUCT(BlueprintType)
struct FGvTPanicEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Panic")
	EGvTPanicSource Source = EGvTPanicSource::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Panic", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float PanicDelta01 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Panic", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float HauntPressureDelta01 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Panic")
	TObjectPtr<AActor> InstigatorActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Panic")
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Panic")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Panic", meta = (ClampMin = "0.0"))
	float SourceRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Panic")
	bool bRequiresProximity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Panic")
	bool bRequiresSuccessfulExecution = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Panic")
	bool bExecutionSucceeded = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Panic", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.f;
};

UCLASS()
class GHOSTSVSTHIEVES_API AGvTPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AGvTPlayerState();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Loot
	UFUNCTION(BlueprintCallable, Category = "GvT|Loot")
	void AddLoot(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "GvT|Loot")
	int32 GetLoot() const { return LootValue; }

	UPROPERTY(BlueprintAssignable, Category = "GvT|Loot")
	FOnLootValueChanged OnLootValueChanged;

	// Panic
	UFUNCTION(BlueprintPure, Category = "GvT|Panic")
	float GetPanic01() const { return Panic01; }

	UPROPERTY(BlueprintAssignable, Category = "GvT|Panic")
	FOnPanicChanged OnPanicChanged;

	UFUNCTION(Server, Reliable)
	void Server_AddPanic(float Delta01);

	UFUNCTION(Server, Reliable)
	void Server_ReducePanic(float Delta01);

	void AddPanicAuthority(float Delta01);
	void ReducePanicAuthority(float Delta01);

	// New event-driven panic API
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "GvT|Panic")
	void Server_ApplyPanicEvent(const FGvTPanicEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "GvT|Panic")
	void ApplyPanicEventAuthority(const FGvTPanicEvent& Event);

	UFUNCTION(BlueprintPure, Category = "GvT|Panic")
	bool WouldApplyPanicEvent(const FGvTPanicEvent& Event) const;

	// Haunt Pressure
	UFUNCTION(BlueprintPure, Category = "GvT|Haunt")
	float GetRecentHauntPressure01() const { return RecentHauntPressure01; }

	UPROPERTY(BlueprintAssignable, Category = "GvT|Haunt")
	FOnHauntPressureChanged OnHauntPressureChanged;

	UFUNCTION(Server, Reliable)
	void Server_AddHauntPressure(float Delta01);

	UFUNCTION(Server, Reliable)
	void Server_ReduceHauntPressure(float Delta01);

	void AddHauntPressureAuthority(float Delta01);
	void ReduceHauntPressureAuthority(float Delta01);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_LootValue, BlueprintReadOnly, Category = "GvT|Loot")
	int32 LootValue = 0;

	UFUNCTION()
	void OnRep_LootValue();

	UPROPERTY(ReplicatedUsing = OnRep_Panic, BlueprintReadOnly, Category = "GvT|Panic")
	float Panic01 = 0.f;

	UFUNCTION()
	void OnRep_Panic();

	UPROPERTY(ReplicatedUsing = OnRep_HauntPressure, BlueprintReadOnly, Category = "GvT|Haunt")
	float RecentHauntPressure01 = 0.f;

	UFUNCTION()
	void OnRep_HauntPressure();

	// Panic decay
	UPROPERTY(EditDefaultsOnly, Category = "GvT|Panic|Decay", meta = (ClampMin = "0.0"))
	float PanicDecayPerSecond = 0.02f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Panic|Decay", meta = (ClampMin = "0.0"))
	float PanicRecoveryDelayAfterSpike = 7.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Panic")
	float LastPanicSpikeTime = -1000.f;

	// Pressure decay
	UPROPERTY(EditDefaultsOnly, Category = "GvT|Haunt|Decay", meta = (ClampMin = "0.0"))
	float HauntPressureDecayPerSecond = 0.12f;

	// Per-source cooldown tuning
	UPROPERTY(EditDefaultsOnly, Category = "GvT|Panic|Cooldowns", meta = (ClampMin = "0.0"))
	float DefaultScareCooldownSeconds = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Panic|Cooldowns", meta = (ClampMin = "0.0"))
	float DefaultLightCooldownSeconds = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Panic|Cooldowns", meta = (ClampMin = "0.0"))
	float DefaultDoorSlamCooldownSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Panic|Cooldowns", meta = (ClampMin = "0.0"))
	float DefaultPowerCooldownSeconds = 15.0f;

private:
	void ApplyPanicDecay(float DeltaSeconds);
	void ApplyHauntPressureDecay(float DeltaSeconds);

	bool CanApplyPanicSource(EGvTPanicSource Source, float CooldownSeconds) const;
	void MarkPanicSourceApplied(EGvTPanicSource Source);
	float ResolveCooldownForSource(EGvTPanicSource Source, float OverrideCooldownSeconds) const;
	static const TCHAR* PanicSourceToString(EGvTPanicSource Source);

private:
	UPROPERTY()
	TMap<uint8, float> LastAppliedPanicSourceTime;
};