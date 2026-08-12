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

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "GvT|House Voice")
	bool bHasPlayedFirstEntryVoice = false;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayHouseVoice(USoundBase* Sound, FVector WorldLocation);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
