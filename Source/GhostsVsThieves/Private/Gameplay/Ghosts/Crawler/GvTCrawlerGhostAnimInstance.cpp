#include "Gameplay/Ghosts/Crawler/GvTCrawlerGhostAnimInstance.h"
#include "Gameplay/Ghosts/Crawler/AGvTCrawlerGhost.h"

void UGvTCrawlerGhostAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    AActor* Owner = GetOwningActor();
    const AGvTCrawlerGhost* Ghost = Cast<AGvTCrawlerGhost>(Owner);

    if (!Ghost)
    {
        // Keep safe defaults
        GhostState = EGvTCrawlerGhostState::IdleCeiling;
        Speed = 0.f;
        return;
    }

    GhostState = Ghost->GetState();
    Speed = Ghost->GetReplicatedSpeed();
}