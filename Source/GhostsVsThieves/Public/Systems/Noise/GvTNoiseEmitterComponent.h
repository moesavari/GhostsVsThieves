#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GvTNoiseEmitterComponent.generated.h"

UCLASS(ClassGroup = (GvT), meta = (BlueprintSpawnableComponent))
class GHOSTSVSTHIEVES_API UGvTNoiseEmitterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGvTNoiseEmitterComponent();

    UFUNCTION(BlueprintCallable, Category = "GvT|Noise")
    void EmitNoise(FGameplayTag NoiseTag, float Radius, float Loudness);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Noise")
    bool bDrawDebug = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Noise")
    float DebugDrawTime = 1.0f;

protected:
    UFUNCTION(Server, Reliable)
    void ServerEmitNoise(FGameplayTag NoiseTag, float Radius, float Loudness);

    void EmitNoise_Internal(FGameplayTag NoiseTag, float Radius, float Loudness);
};
