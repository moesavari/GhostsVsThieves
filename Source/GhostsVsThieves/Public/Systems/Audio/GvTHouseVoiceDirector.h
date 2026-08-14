#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GvTHouseVoiceDirector.generated.h"

class USoundBase;

/** One shared, match-wide house voice. Place exactly one in the level. */
UCLASS()
class GHOSTSVSTHIEVES_API AGvTHouseVoiceDirector : public AActor
{
	GENERATED_BODY()

public:
	AGvTHouseVoiceDirector();

	/** Called by any entrance trigger. The server accepts only the first living thief. */
	UFUNCTION(BlueprintCallable, Category = "GvT|House Voice")
	void TryPlayFirstEntryVoice(AActor* EnteringActor);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|House Voice")
	TArray<TObjectPtr<USoundBase>> FirstEntrySounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|House Voice", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float FirstEntrySoundChance = 0.30f;

	UPROPERTY(Transient)
	bool bHasEvaluatedFirstEntryVoice = false;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "GvT|House Voice")
	bool bHasPlayedFirstEntryVoice = false;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayHouseVoice(USoundBase* Sound, FVector WorldLocation);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
