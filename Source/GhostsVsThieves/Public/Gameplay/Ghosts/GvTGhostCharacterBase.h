#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "GvTGhostCharacterBase.generated.h"

UCLASS(Abstract, Blueprintable)
class GHOSTSVSTHIEVES_API AGvTGhostCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AGvTGhostCharacterBase();

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost")
	virtual void SetGhostPresenceActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost|Scare")
	virtual void BeginGhostScare(AActor* Target, FGameplayTag ScareTag);

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost|Event")
	virtual void BeginGhostEvent(AActor* Target, FGameplayTag EventTag);

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost|Haunt")
	virtual void BeginGhostHaunt(AActor* Target, FGameplayTag HauntTag);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_GhostPresenceActive, EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost")
	bool bGhostPresenceActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost")
	FGameplayTagContainer SupportedGhostScares;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost")
	FGameplayTagContainer SupportedGhostEvents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost")
	FGameplayTagContainer SupportedGhostHaunts;

	UFUNCTION()
	void OnRep_GhostPresenceActive();

	void ApplyGhostPresenceActive();
};