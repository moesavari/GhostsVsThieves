#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GvTThiefPerceptionComponent.generated.h"

class AGvTMirrorActor;

UCLASS(ClassGroup = (GvT), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GHOSTSVSTHIEVES_API UGvTThiefPerceptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGvTThiefPerceptionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "GvT|Perception|Mirror")
	void Test_MirrorScare(float Intensity01 = 1.f, float LifeSeconds = 1.5f);

protected:
	UFUNCTION(Server, Reliable)
	void Server_RequestMirrorActorScare(AGvTMirrorActor* Mirror, float Intensity01, float LifeSeconds);

	UFUNCTION(Client, Reliable)
	void Client_PlayMirrorActorScare(AGvTMirrorActor* Mirror, float Intensity01, float LifeSeconds);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Perception|Mirror")
	bool bEnableMirrorReflectSense = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Perception|Mirror")
	float MirrorReflectTraceDistance = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Perception|Mirror")
	float MirrorReflectSphereRadius = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Perception|Mirror")
	float MirrorReflectDotMin = 0.86f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Perception|Mirror")
	float MirrorReflectCheckInterval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Perception|Mirror")
	float MirrorReflectLifeSeconds = 0.6f;

private:
	void TickMirrorReflectSense();

	bool IsServer() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGvTMirrorActor> LastTriggeredMirror;

	UPROPERTY(Transient)
	float NextAllowedMirrorReflectTime = 0.f;

	FTimerHandle MirrorReflectTimer;
};