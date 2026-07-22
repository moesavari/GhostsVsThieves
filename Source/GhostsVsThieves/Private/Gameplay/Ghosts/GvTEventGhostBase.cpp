#include "Gameplay/Ghosts/GvTEventGhostBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AGvTEventGhostBase::AGvTEventGhostBase()
{
	bReplicates = false;
	SetReplicateMovement(false);

	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(
			ECollisionEnabled::NoCollision);

		Capsule->SetGenerateOverlapEvents(false);
	}

	if (UCharacterMovementComponent* Movement =
		GetCharacterMovement())
	{
		Movement->DisableMovement();
		Movement->SetComponentTickEnabled(false);
	}
}

void AGvTEventGhostBase::BeginGhostEvent(AActor* Target, AActor* SourceActor, FGameplayTag EventTag)
{
	EventTarget = Target;
	EventSourceActor = SourceActor;
	ActiveEventTag = EventTag;

	UE_LOG(LogTemp, Log, 
		TEXT("[EventGhost] BeginGhostEvent Ghost=%s Target=%s Source=%s Tag=%s"), 
		*GetNameSafe(this), 
		*GetNameSafe(Target), 
		*GetNameSafe(SourceActor), 
		*EventTag.ToString());
}

void AGvTEventGhostBase::StartEventPresentation(float Intensity01, float LifeSeconds)
{
	EventIntensity01 = FMath::Clamp(Intensity01, 0.0f, 1.0f);

	EventLifeSeconds = FMath::Max(LifeSeconds, 0.01f);

	bEventPresentationActive = true;

	SetActorHiddenInGame(false);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[EventGhost] StartPresentation Ghost=%s Intensity=%.2f Life=%.2f Tag=%s"),
		*GetNameSafe(this),
		EventIntensity01,
		EventLifeSeconds,
		*ActiveEventTag.ToString());
}

void AGvTEventGhostBase::StopEventPresentation()
{
	bEventPresentationActive = false;
	EventIntensity01 = 0.0f;
	EventLifeSeconds = 0.0f;
	EventTarget = nullptr;
	EventSourceActor = nullptr;
	ActiveEventTag = FGameplayTag();

	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);

	UE_LOG(
		LogTemp,
		Verbose,
		TEXT("[EventGhost] StopPresentation Ghost=%s"),
		*GetNameSafe(this));
}

void AGvTEventGhostBase::BeginGhostScare(AActor* Target, FGameplayTag ScareTag)
{
	BeginGhostEvent(Target, nullptr, ScareTag);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[EventGhost] BeginGhostScare redirected to event. Ghost=%s Target=%s Tag=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Target),
		*ScareTag.ToString());
}