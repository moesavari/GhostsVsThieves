#include "Systems/Noise/GvTNoiseSubsystem.h"

void UGvTNoiseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("GvT Noise Subsystem Initialized"));
}

void UGvTNoiseSubsystem::EmitNoise(const FGvTNoiseEvent& NoiseEvent)
{
    UE_LOG(LogTemp, Log, TEXT("Noise emitted at %s | Radius: %.1f | Loudness: %.2f"),
        *NoiseEvent.Location.ToString(),
        NoiseEvent.Radius,
        NoiseEvent.Loudness
    );
}
