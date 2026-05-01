#include "Systems/Spawn/GvTGhostSpawnPoint.h"
#include "Components/ArrowComponent.h"
#include "Components/SphereComponent.h"

AGvTGhostSpawnPoint::AGvTGhostSpawnPoint()
{
    Root = CreateDefaultSubobject<USceneComponent>("Root");
    SetRootComponent(Root);

    Arrow = CreateDefaultSubobject<UArrowComponent>("Arrow");
    Arrow->SetupAttachment(Root);

    DebugSphere = CreateDefaultSubobject<USphereComponent>("DebugSphere");
    DebugSphere->SetupAttachment(Root);
    DebugSphere->SetSphereRadius(50.f);
}

bool AGvTGhostSpawnPoint::IsValidFor(AActor* Victim, float Now, bool bForHauntChase) const 
{
    if (!bEnabled || !Victim)
        return false;

    if (bForHauntChase && !bSupportsHauntChase)
        return false;

    if (!bForHauntChase && !bSupportsGhoulChase)
        return false;

    const float Dist = FVector::Dist(Victim->GetActorLocation(), GetActorLocation());

    if (Dist < MinDistanceToVictim || Dist > MaxDistanceToVictim)
        return false;

    if ((Now - LastUsedTime) < ReuseCooldown)
        return false;

    return true;
}