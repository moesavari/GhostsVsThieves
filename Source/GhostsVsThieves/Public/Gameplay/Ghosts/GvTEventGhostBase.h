#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "GvTEventGhostBase.generated.h"

UCLASS(Abstract, Blueprintable)
class GHOSTSVSTHIEVES_API AGvTEventGhostBase : public AGvTGhostCharacterBase
{
	GENERATED_BODY()

public:
	AGvTEventGhostBase();

	virtual void BeginGhostEvent(AActor* Target, AActor* SourceActor, FGameplayTag EventTag);

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost|Event")
	AActor* GetEventSourceActor() const
	{
		return EventSourceActor.Get();
	}

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost|Event")
	virtual void StartEventPresentation(float Intensity01, float LifeSeconds);

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost|Event")
	virtual void StopEventPresentation();

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost|Event")
	bool IsEventPresentationActive() const
	{
		return bEventPresentationActive;
	}

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost|Event")
	AActor* GetEventTarget() const
	{
		return EventTarget.Get();
	}

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost|Event")
	FGameplayTag GetActiveEventTag() const
	{
		return ActiveEventTag;
	}

	virtual void BeginGhostScare(AActor* Target, FGameplayTag ScareTag) override;

protected:

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Event")
	TObjectPtr<AActor> EventTarget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Event")
	TObjectPtr<AActor> EventSourceActor = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Event")
	FGameplayTag ActiveEventTag;

	bool bEventPresentationActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Event")
	float EventIntensity01 = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Event")
	float EventLifeSeconds = 0.0f;
};