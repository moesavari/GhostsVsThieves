#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "GvTCrawlerGhostCharacter.generated.h"

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTCrawlerGhostCharacter : public AGvTGhostCharacterBase
{
	GENERATED_BODY()

public:
	AGvTCrawlerGhostCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void BeginHauntGhostScare(AActor* Target, FGameplayTag ScareTag) override;

protected:
	void BeginGhostScare_Local(ACharacter* TargetCharacter);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Ghost|Crawler|Overhead")
	float OverheadForwardOffset = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Ghost|Crawler|Overhead")
	float OverheadHeightOffset = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Ghost|Crawler|Overhead")
	float OverheadPitchOffset = -45.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Ghost|Crawler|Overhead")
	float OverheadDuration = 1.2f;
};