#include "Systems/World/GvTHouseBoundsLibrary.h"

#include "Components/BrushComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Volume.h"

bool UGvTHouseBoundsLibrary::IsLocationInsideHouse(const UObject* WorldContextObject, const FVector& Location, bool& bFoundValidBounds)
{
    bFoundValidBounds = false;
    const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World)
    {
        return true;
    }

    for (TActorIterator<AVolume> It(World); It; ++It)
    {
        const AVolume* Volume = *It;
        if (!IsValid(Volume) || !Volume->ActorHasTag(TEXT("HouseBounds")))
        {
            continue;
        }

        const UBrushComponent* Brush = Volume->GetBrushComponent();
        if (!IsValid(Brush) || !Brush->Bounds.SphereRadius)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HouseBounds] Ignoring invalid tagged volume %s"), *GetNameSafe(Volume));
            continue;
        }

        bFoundValidBounds = true;
        const FBox WorldBox = Brush->Bounds.GetBox().ExpandBy(2.f);
        const bool bInside = WorldBox.IsInsideOrOn(Location);
        UE_LOG(LogTemp, VeryVerbose, TEXT("[HouseBounds] Volume=%s Point=%s Min=%s Max=%s Inside=%s"),
            *GetNameSafe(Volume), *Location.ToCompactString(), *WorldBox.Min.ToCompactString(), *WorldBox.Max.ToCompactString(), bInside ? TEXT("true") : TEXT("false"));

        if (bInside)
        {
            return true;
        }
    }

    return !bFoundValidBounds;
}
