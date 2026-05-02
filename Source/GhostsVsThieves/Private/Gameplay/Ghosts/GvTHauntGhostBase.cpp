#include "Gameplay/Ghosts/GvTHauntGhostBase.h"

#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/Scare/GvTScareTags.h"

AGvTHauntGhostBase::AGvTHauntGhostBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void AGvTHauntGhostBase::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = RoamSpeed;
	}
}

void AGvTHauntGhostBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	if (HauntState == EGvTHauntGhostState::Chasing)
	{
		UpdateChase(DeltaSeconds);
	}
}

void AGvTHauntGhostBase::BeginGhostHaunt(AActor* Target, FGameplayTag HauntTag)
{
	Super::BeginGhostHaunt(Target, HauntTag);

	if (HauntTag.MatchesTagExact(GvTScareTags::GhostHaunt_Chase()))
	{
		StartGhostChase(Target);
	}
}

void AGvTHauntGhostBase::StartGhostChase(AActor* Target)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!Target)
	{
		return;
	}

	CurrentTarget = Target;
	LastKnownTargetLocation = Target->GetActorLocation();
	LastTimeTargetSeen = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	SetGhostPresenceActive(true);
	SetHauntState(EGvTHauntGhostState::Chasing);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = ChaseSpeed;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[HauntGhost] Chase started. Ghost=%s Target=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Target));
}

void AGvTHauntGhostBase::StopGhostChase()
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentTarget = nullptr;
	SetHauntState(EGvTHauntGhostState::Idle);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = RoamSpeed;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[HauntGhost] Chase stopped. Ghost=%s"),
		*GetNameSafe(this));
}

bool AGvTHauntGhostBase::HasLineOfSightToTarget(AActor* Target) const
{
	if (!Target || !GetWorld())
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_HauntGhost_LOS), false, this);
	Params.AddIgnoredActor(this);

	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector End = Target->GetActorLocation() + FVector(0.f, 0.f, 60.f);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		ECC_Visibility,
		Params);

	if (!bHit)
	{
		return true;
	}

	return Hit.GetActor() == Target;
}

void AGvTHauntGhostBase::UpdateChase(float DeltaSeconds)
{
	if (!CurrentTarget)
	{
		StopGhostChase();
		return;
	}

	const bool bHasLOS = HasLineOfSightToTarget(CurrentTarget);

	if (bHasLOS)
	{
		LastKnownTargetLocation = CurrentTarget->GetActorLocation();
		LastTimeTargetSeen = GetWorld() ? GetWorld()->GetTimeSeconds() : LastTimeTargetSeen;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bLostSightTooLong = !bHasLOS && (Now - LastTimeTargetSeen) > LoseSightGraceSeconds;

	if (bLostSightTooLong)
	{
		SetHauntState(EGvTHauntGhostState::Searching);

		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			AI->MoveToLocation(LastKnownTargetLocation);
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[HauntGhost] Lost target. Moving to last known location."));

		return;
	}

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->MoveToActor(CurrentTarget, CatchDistance * 0.5f);
	}

	TryCatchTarget();
}

void AGvTHauntGhostBase::TryCatchTarget()
{
	if (!CurrentTarget)
	{
		return;
	}

	const float Dist = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());

	if (Dist > CatchDistance)
	{
		return;
	}

	if (bRequireLineOfSightToCatch && !HasLineOfSightToTarget(CurrentTarget))
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[HauntGhost] In catch range but no LOS. Catch blocked."));
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[HauntGhost] Target caught/scared. Ghost=%s Target=%s"),
		*GetNameSafe(this),
		*GetNameSafe(CurrentTarget));

	// Later:
	// - trigger panic
	// - trigger scare/catch event
	// - notify Director
	// For now, stop chase so we can test safely.
	StopGhostChase();
}

void AGvTHauntGhostBase::SetHauntState(EGvTHauntGhostState NewState)
{
	if (HauntState == NewState)
	{
		return;
	}

	HauntState = NewState;

	UE_LOG(LogTemp, Warning,
		TEXT("[HauntGhost] State changed: %d"),
		static_cast<int32>(HauntState));
}