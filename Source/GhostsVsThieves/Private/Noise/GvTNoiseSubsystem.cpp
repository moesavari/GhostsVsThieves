#include "Systems/Noise/GvTNoiseSubsystem.h"

void UGvTNoiseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("GvT Noise Subsystem Initialized"));
}

void UGvTNoiseSubsystem::EmitNoise(const FGvTNoiseEvent& NoiseEvent)
{
    UE_LOG(LogTemp, Log, TEXT("Noise [%s] at %s | R=%.1f | L=%.2f"),
        *NoiseEvent.NoiseTag.ToString(),
        *NoiseEvent.Location.ToString(),
        NoiseEvent.Radius,
        NoiseEvent.Loudness
    );

}
