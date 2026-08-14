#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GvTGhostSpawnPoint.generated.h"

UCLASS(Blueprintable)
class GHOSTSVSTHIEVES_API AGvTGhostSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AGvTGhostSpawnPoint();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost|Spawn")
	bool IsEnabled() const { return bEnabled; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost|Spawn")
	bool SupportsHauntTag(FGameplayTag HauntTag) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Spawn")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Spawn")
	bool bIndoorSpawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Spawn")
	FGameplayTagContainer SupportedHauntTags;
};
