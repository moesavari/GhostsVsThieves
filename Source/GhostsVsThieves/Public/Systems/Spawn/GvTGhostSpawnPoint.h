#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GvTGhostSpawnPoint.generated.h"

class UArrowComponent;
class USphereComponent;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTGhostSpawnPoint : public AActor
{
    GENERATED_BODY()

public:
    AGvTGhostSpawnPoint();

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UArrowComponent> Arrow;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USphereComponent> DebugSphere;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    bool bSupportsHauntChase = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    bool bSupportsGhoulChase = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    float MinDistanceToVictim = 400.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    float MaxDistanceToVictim = 2000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    float ReuseCooldown = 8.f;

    float LastUsedTime = -1000.f;

    bool IsValidFor(AActor* Victim, float Now, bool bForHauntChase) const;
};