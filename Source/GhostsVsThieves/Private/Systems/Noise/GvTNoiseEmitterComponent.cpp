#include "Systems/Noise/GvTNoiseEmitterComponent.h"

#include "Systems/Noise/GvTNoiseSubsystem.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UGvTNoiseEmitterComponent::UGvTNoiseEmitterComponent()
{
    SetIsReplicatedByDefault(true);
}

void UGvTNoiseEmitterComponent::EmitNoise(FGameplayTag NoiseTag, float Radius, float Loudness)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        EmitNoise_Internal(NoiseTag, Radius, Loudness);
    }
    else
    {
        ServerEmitNoise(NoiseTag, Radius, Loudness);
    }
}

void UGvTNoiseEmitterComponent::ServerEmitNoise_Implementation(FGameplayTag NoiseTag, float Radius, float Loudness)
{
    EmitNoise_Internal(NoiseTag, Radius, Loudness);
}

void UGvTNoiseEmitterComponent::EmitNoise_Internal(FGameplayTag NoiseTag, float Radius, float Loudness)
{
    if (!GetWorld() || !GetOwner()) return;

    UGameInstance* GI = GetWorld()->GetGameInstance();
    if (!GI) return;

    UGvTNoiseSubsystem* NoiseSubsystem = GI->GetSubsystem<UGvTNoiseSubsystem>();
    if (!NoiseSubsystem) return;

    FGvTNoiseEvent Evt;
    Evt.Location = GetOwner()->GetActorLocation();
    Evt.Radius = Radius;
    Evt.Loudness = Loudness;
    Evt.NoiseTag = NoiseTag;

    NoiseSubsystem->EmitNoise(Evt);

    if (bDrawDebug)
    {
        DrawDebugSphere(GetWorld(), Evt.Location, Evt.Radius, 16, FColor::Green, false, DebugDrawTime);
        DrawDebugString(GetWorld(), Evt.Location + FVector(0, 0, 50), Evt.NoiseTag.ToString(), nullptr, FColor::White, DebugDrawTime);
    }
}
