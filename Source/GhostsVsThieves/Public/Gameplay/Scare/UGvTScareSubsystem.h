#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Gameplay/Scare/GvTScareTypes.h"
#include "UGvTScareSubsystem.generated.h"

class UGvTScareDefinition;
class UGvTGhostProfileAsset;

USTRUCT()
struct FGvTWeightedScareCandidate
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const UGvTScareDefinition> Def = nullptr;

	UPROPERTY()
	float Weight = 0.f;
};

USTRUCT()
struct FGvTScareWeightDebugRow
{
	GENERATED_BODY()

	UPROPERTY() FGameplayTag Tag;
	UPROPERTY() float Base = 0.f;
	UPROPERTY() float TierMult = 1.f;
	UPROPERTY() float GhostMult = 1.f;
	UPROPERTY() float ContextMult = 1.f;
	UPROPERTY() float RepeatMult = 1.f;
	UPROPERTY() float Final = 0.f;

	UPROPERTY() FString FilterReason;
};

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API UGvTScareSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare")
	void RegisterScareDefinition(UGvTScareDefinition* Def);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare")
	void RegisterGhostProfile(UGvTGhostProfileAsset* Profile);

	UFUNCTION(BlueprintPure, Category = "GvT|Scare")
	const UGvTGhostProfileAsset* GetGhostProfile(FGameplayTag GhostTag) const;

	const UGvTScareDefinition* PickScareWeighted(
		const FGameplayTagContainer& ContextTags,
		const UGvTGhostProfileAsset* GhostProfile,
		uint8 PanicTier,
		const TMap<FGameplayTag, float>& LastTagTimeSeconds,
		float NowSeconds,
		FRandomStream& Stream
	) const;

	const UGvTScareDefinition* PickScareWeightedDebug(
		const FGameplayTagContainer& ContextTags,
		const UGvTGhostProfileAsset* GhostProfile,
		uint8 PanicTier,
		const TMap<FGameplayTag, float>& LastTagTimeSeconds,
		float NowSeconds,
		FRandomStream& Stream,
		TArray<FGvTScareWeightDebugRow>& OutDebugRows
	) const;

	float ComputeFinalWeight(
		const UGvTScareDefinition* Def,
		const FGameplayTagContainer& ContextTags,
		const UGvTGhostProfileAsset* GhostProfile,
		uint8 PanicTier,
		const TMap<FGameplayTag, float>& LastTagTimeSeconds,
		float NowSeconds
	) const;

	static bool IsScareDebugEnabled();

private:
	UPROPERTY()
	TArray<TObjectPtr<UGvTScareDefinition>> ScareDefinitions;

	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UGvTGhostProfileAsset>> GhostProfiles;
};
