#include "Gameplay/Ghosts/GvTHauntGhostBase.h"

#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "Gameplay/Scare/GvTScareTags.h"

AGvTHauntGhostBase::AGvTHauntGhostBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void AGvTHauntGhostBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTHauntGhostBase, HauntState);
	DOREPLIFETIME(AGvTHauntGhostBase, AssignedChaseTarget);
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

	if (!HasAuthority() || !Target)
	{
		return;
	}

	PendingHauntTarget = Target;
	PendingHauntTag = HauntTag;

	SetGhostPresenceActive(true);
	SetHauntState(EGvTHauntGhostState::SpawnIntro);

	GetWorldTimerManager().ClearTimer(TimerHandle_BeginHauntAfterSpawn);
	const float Delay = FMath::Max(PreHauntDelaySeconds, 0.f);
	if (Delay <= 0.f)
	{
		HandleBeginHauntAfterSpawnDelay();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			TimerHandle_BeginHauntAfterSpawn,
			this,
			&AGvTHauntGhostBase::HandleBeginHauntAfterSpawnDelay,
			Delay,
			false);
	}
}


void AGvTHauntGhostBase::EnterSpawnIntroHold()
{
	if (!HasAuthority())
	{
		return;
	}

	SpawnIntroHoldLocation = GetActorLocation();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		SavedSpawnIntroMovementMode = MoveComp->MovementMode;
		SavedSpawnIntroGravityScale = MoveComp->GravityScale;
		MoveComp->StopMovementImmediately();
		MoveComp->GravityScale = 0.f;
		MoveComp->DisableMovement();
		bSpawnIntroMovementHeld = true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GhostLifecycle] Spawn intro hold started Ghost=%s Loc=%s Delay=%.2f"),
		*GetNameSafe(this),
		*SpawnIntroHoldLocation.ToString(),
		PreHauntDelaySeconds);
}

void AGvTHauntGhostBase::ExitSpawnIntroHold()
{
	if (!HasAuthority() || !bSpawnIntroMovementHeld)
	{
		return;
	}

	SetActorLocation(SpawnIntroHoldLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->GravityScale = SavedSpawnIntroGravityScale;
		MoveComp->SetMovementMode(SavedSpawnIntroMovementMode);
	}

	bSpawnIntroMovementHeld = false;

	UE_LOG(LogTemp, Warning, TEXT("[GhostLifecycle] Spawn intro hold ended Ghost=%s RestoredLoc=%s"),
		*GetNameSafe(this),
		*GetActorLocation().ToString());
}

void AGvTHauntGhostBase::HandleBeginHauntAfterSpawnDelay()
{
	if (!HasAuthority())
	{
		return;
	}

	AActor* Target = PendingHauntTarget.Get();
	const FGameplayTag HauntTag = PendingHauntTag;

	PendingHauntTarget = nullptr;
	PendingHauntTag = FGameplayTag();

	if (!Target)
	{
		StartHauntDespawnSequence();
		return;
	}

	ExitSpawnIntroHold();
	BP_OnGhostHauntStarted(HauntTag);
	BeginHauntAction(Target, HauntTag);
}

void AGvTHauntGhostBase::BeginHauntAction(AActor* Target, FGameplayTag HauntTag)
{
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

	SetAssignedChaseTarget(Target);
	SetCurrentChaseTarget(Target);
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

	GetWorldTimerManager().ClearTimer(TimerHandle_BeginHauntAfterSpawn);
	ExitSpawnIntroHold();

	SetCurrentChaseTarget(nullptr);
	SetHauntState(EGvTHauntGhostState::Idle);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = RoamSpeed;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[HauntGhost] Chase stopped. Ghost=%s"),
		*GetNameSafe(this));
}


void AGvTHauntGhostBase::OnRep_HauntState(EGvTHauntGhostState OldState)
{
	EnterHauntState(HauntState, OldState);
}

bool AGvTHauntGhostBase::IsValidHauntVictim(const APawn* CandidatePawn) const
{
	if (!IsValid(CandidatePawn))
	{
		return false;
	}

	if (const AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(CandidatePawn))
	{
		return !Thief->IsDead();
	}

	return true;
}

bool AGvTHauntGhostBase::HasLineOfSightToPawn(const APawn* CandidatePawn) const
{
	if (!IsValidHauntVictim(CandidatePawn) || !GetWorld())
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_HauntGhost_PawnLOS), false, this);
	Params.AddIgnoredActor(this);

	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector End = CandidatePawn->GetActorLocation() + FVector(0.f, 0.f, 60.f);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	return !bHit || Hit.GetActor() == CandidatePawn;
}

APawn* AGvTHauntGhostBase::FindBestVisibleVictim() const
{
	if (!GetWorld())
	{
		return nullptr;
	}

	// Keep the Director-selected target meaningful: if the assigned victim is visible, prefer them.
	if (APawn* AssignedPawn = Cast<APawn>(AssignedChaseTarget.Get()))
	{
		if (HasLineOfSightToPawn(AssignedPawn))
		{
			return AssignedPawn;
		}
	}

	APawn* BestVictim = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (TActorIterator<AGvTThiefCharacter> It(GetWorld()); It; ++It)
	{
		AGvTThiefCharacter* Candidate = *It;
		if (!IsValidHauntVictim(Candidate) || !HasLineOfSightToPawn(Candidate))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestVictim = Candidate;
		}
	}

	return BestVictim;
}

void AGvTHauntGhostBase::SetAssignedChaseTarget(AActor* Target)
{
	AssignedChaseTarget = Target;
}

void AGvTHauntGhostBase::SetCurrentChaseTarget(AActor* Target)
{
	CurrentTarget = Target;
}

FVector AGvTHauntGhostBase::GetNavigationSafeGhostLocation(const FVector& DesiredLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return DesiredLocation;
	}

	FVector ResultLocation = DesiredLocation;
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSys)
	{
		FNavLocation NavLocation;
		if (NavSys->ProjectPointToNavigation(DesiredLocation, NavLocation))
		{
			ResultLocation = NavLocation.Location;
		}
	}

	if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		ResultLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
	}

	return ResultLocation;
}

void AGvTHauntGhostBase::SnapGhostToNavigationSafeHeight()
{
	const FVector SafeLocation = GetNavigationSafeGhostLocation(GetActorLocation());
	SetActorLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);
	ForceNetUpdate();
}

bool AGvTHauntGhostBase::HasLineOfSightToTarget(AActor* Target) const
{
	if (const APawn* PawnTarget = Cast<APawn>(Target))
	{
		return HasLineOfSightToPawn(PawnTarget);
	}

	if (!Target || !GetWorld())
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_HauntGhost_LOS), false, this);
	Params.AddIgnoredActor(this);

	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector End = Target->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	return !bHit || Hit.GetActor() == Target;
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

void AGvTHauntGhostBase::StartHauntDespawnSequence()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(TimerHandle_BeginHauntAfterSpawn);
	GetWorldTimerManager().ClearTimer(TimerHandle_FinishDespawn);

	SetCurrentChaseTarget(nullptr);
	AssignedChaseTarget = nullptr;
	PendingHauntTarget = nullptr;
	PendingHauntTag = FGameplayTag();
	SetHauntState(EGvTHauntGhostState::Despawning);

	const float Delay = FMath::Max(PreDespawnDelaySeconds, 0.f);
	if (Delay <= 0.f)
	{
		FinishDespawnSequence();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			TimerHandle_FinishDespawn,
			this,
			&AGvTHauntGhostBase::FinishDespawnSequence,
			Delay,
			false);
	}
}

void AGvTHauntGhostBase::FinishDespawnSequence()
{
	Destroy();
}

const TCHAR* AGvTHauntGhostBase::GetHauntStateName(EGvTHauntGhostState State)
{
	switch (State)
	{
	case EGvTHauntGhostState::Idle: return TEXT("Idle");
	case EGvTHauntGhostState::SpawnIntro: return TEXT("SpawnIntro");
	case EGvTHauntGhostState::Roaming: return TEXT("Roaming");
	case EGvTHauntGhostState::Investigating: return TEXT("Investigating");
	case EGvTHauntGhostState::Chasing: return TEXT("Chasing");
	case EGvTHauntGhostState::Searching: return TEXT("Searching");
	case EGvTHauntGhostState::PerformingScare: return TEXT("PerformingScare");
	case EGvTHauntGhostState::Recovering: return TEXT("Recovering");
	case EGvTHauntGhostState::Despawning: return TEXT("Despawning");
	default: return TEXT("Unknown");
	}
}

bool AGvTHauntGhostBase::CanTransitionTo(EGvTHauntGhostState NewState) const
{
	if (HauntState == NewState)
	{
		return true;
	}

	if (HauntState == EGvTHauntGhostState::Despawning)
	{
		return false;
	}

	switch (HauntState)
	{
	case EGvTHauntGhostState::Idle:
		return NewState == EGvTHauntGhostState::SpawnIntro
			|| NewState == EGvTHauntGhostState::Roaming
			|| NewState == EGvTHauntGhostState::Investigating
			|| NewState == EGvTHauntGhostState::Chasing
			|| NewState == EGvTHauntGhostState::PerformingScare
			|| NewState == EGvTHauntGhostState::Despawning;
	case EGvTHauntGhostState::SpawnIntro:
		return NewState == EGvTHauntGhostState::Chasing
			|| NewState == EGvTHauntGhostState::PerformingScare
			|| NewState == EGvTHauntGhostState::Despawning
			|| NewState == EGvTHauntGhostState::Idle;
	case EGvTHauntGhostState::Roaming:
		return NewState == EGvTHauntGhostState::Investigating
			|| NewState == EGvTHauntGhostState::Chasing
			|| NewState == EGvTHauntGhostState::PerformingScare
			|| NewState == EGvTHauntGhostState::Despawning
			|| NewState == EGvTHauntGhostState::Idle;
	case EGvTHauntGhostState::Investigating:
		return NewState == EGvTHauntGhostState::Chasing
			|| NewState == EGvTHauntGhostState::Searching
			|| NewState == EGvTHauntGhostState::Roaming
			|| NewState == EGvTHauntGhostState::Despawning
			|| NewState == EGvTHauntGhostState::Idle;
	case EGvTHauntGhostState::Chasing:
		return NewState == EGvTHauntGhostState::Searching
			|| NewState == EGvTHauntGhostState::PerformingScare
			|| NewState == EGvTHauntGhostState::Recovering
			|| NewState == EGvTHauntGhostState::Despawning
			|| NewState == EGvTHauntGhostState::Idle;
	case EGvTHauntGhostState::Searching:
		return NewState == EGvTHauntGhostState::Chasing
			|| NewState == EGvTHauntGhostState::Roaming
			|| NewState == EGvTHauntGhostState::Despawning
			|| NewState == EGvTHauntGhostState::Idle;
	case EGvTHauntGhostState::PerformingScare:
		return NewState == EGvTHauntGhostState::Recovering
			|| NewState == EGvTHauntGhostState::Despawning
			|| NewState == EGvTHauntGhostState::Idle;
	case EGvTHauntGhostState::Recovering:
		return NewState == EGvTHauntGhostState::Roaming
			|| NewState == EGvTHauntGhostState::Chasing
			|| NewState == EGvTHauntGhostState::Despawning
			|| NewState == EGvTHauntGhostState::Idle;
	case EGvTHauntGhostState::Despawning:
	default:
		return false;
	}
}

void AGvTHauntGhostBase::ExitHauntState(EGvTHauntGhostState OldState, EGvTHauntGhostState NewState)
{
	if (OldState == EGvTHauntGhostState::SpawnIntro && NewState != EGvTHauntGhostState::SpawnIntro)
	{
		ExitSpawnIntroHold();
	}
}

void AGvTHauntGhostBase::EnterHauntState(EGvTHauntGhostState NewState, EGvTHauntGhostState OldState)
{
	switch (NewState)
	{
	case EGvTHauntGhostState::Idle:
		SetCurrentChaseTarget(nullptr);
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed = RoamSpeed;
		}
		break;
	case EGvTHauntGhostState::SpawnIntro:
		SetGhostPresenceActive(true);
		EnterSpawnIntroHold();
		BP_OnGhostSpawnIntroStarted(PendingHauntTag);
		break;
	case EGvTHauntGhostState::Roaming:
	case EGvTHauntGhostState::Investigating:
	case EGvTHauntGhostState::Searching:
		SnapGhostToNavigationSafeHeight();
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed = RoamSpeed;
		}
		break;
	case EGvTHauntGhostState::Chasing:
		SetGhostPresenceActive(true);
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->SetMovementMode(MOVE_Walking);
			MoveComp->MaxWalkSpeed = ChaseSpeed;
		}
		break;
	case EGvTHauntGhostState::Despawning:
		SetCurrentChaseTarget(nullptr);
		PendingHauntTarget = nullptr;
		PendingHauntTag = FGameplayTag();
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			AI->StopMovement();
		}
		BP_OnGhostDespawnStarted();
		break;
	default:
		break;
	}
}

bool AGvTHauntGhostBase::SetHauntState(EGvTHauntGhostState NewState)
{
	if (HauntState == NewState)
	{
		return true;
	}

	if (!CanTransitionTo(NewState))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[HauntGhost] Blocked invalid state transition Ghost=%s From=%s To=%s"),
			*GetNameSafe(this),
			GetHauntStateName(HauntState),
			GetHauntStateName(NewState));
		return false;
	}

	const EGvTHauntGhostState OldState = HauntState;
	ExitHauntState(OldState, NewState);
	HauntState = NewState;
	EnterHauntState(NewState, OldState);

	UE_LOG(LogTemp, Warning,
		TEXT("[HauntGhost] State changed Ghost=%s From=%s To=%s"),
		*GetNameSafe(this),
		GetHauntStateName(OldState),
		GetHauntStateName(NewState));

	ForceNetUpdate();
	return true;
}
