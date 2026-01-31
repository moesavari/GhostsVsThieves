#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "GvTNoiseSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FGvTNoiseEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector Location = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    float Radius = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float Loudness = 0.f;

    UPROPERTY(BlueprintReadOnly)
    FGameplayTag NoiseTag;
};


UCLASS()
class GHOSTSVSTHIEVES_API UGvTNoiseSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable)
    void EmitNoise(const FGvTNoiseEvent& NoiseEvent);
};
