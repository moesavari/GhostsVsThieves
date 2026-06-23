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
	SetReplicateMovement(true);

	NetUpdateFrequency = 30.f;
	MinNetUpdateFrequency = 15.f;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
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

		MoveComp->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
		MoveComp->bNetworkSmoothingComplete = true;
	}

	CacheDefaultMeshTransform();
	ConfigureClientGhostProxyMovement();
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
	else if (HauntState == EGvTHauntGhostState::Searching)
	{
		UpdateSearch(DeltaSeconds);
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

	SearchElapsedSeconds = 0.f;
	SearchRepathTimer = 0.f;

	SetGhostPresenceActive(true);
	SetHauntState(EGvTHauntGhostState::Chasing);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = ChaseSpeed;
	}

	OnHauntChaseStarted(Target);

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->MoveToActor(Target, MovementAcceptanceRadius, true, true, true);
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

	OnHauntChaseEnded();
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
	if (OldState == HauntState)
	{
		return;
	}

	UE_LOG(LogTemp, Error,
		TEXT("[CLIENT STATE] Ghost=%s Old=%s New=%s Loc=%s MeshRel=%s"),
		*GetNameSafe(this),
		GetHauntStateName(OldState),
		GetHauntStateName(HauntState),
		*GetActorLocation().ToString(),
		GetMesh() ? *GetMesh()->GetRelativeLocation().ToString() : TEXT("NoMesh"));

	ResetMeshVisualTransform();

	if (HauntState != EGvTHauntGhostState::Despawning)
	{
		ConfigureClientGhostProxyMovement();
	}
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
			ResultLocation.X = NavLocation.Location.X;
			ResultLocation.Y = NavLocation.Location.Y;
			ResultLocation.Z = GetActorLocation().Z;
		}
	}

	return ResultLocation;
}

void AGvTHauntGhostBase::SnapGhostToNavigationSafeHeight()
{
	if (!HasAuthority())
	{
		return;
	}

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
	if (!HasAuthority())
	{
		return;
	}

	AActor* Target = CurrentTarget.Get();
	if (!IsValid(Target))
	{
		StartHauntDespawnSequence();
		return;
	}

	const bool bHasLOS = HasLineOfSightToTarget(Target);
	if (bHasLOS)
	{
		LastKnownTargetLocation = Target->GetActorLocation();
		LastTimeTargetSeen = GetWorld() ? GetWorld()->GetTimeSeconds() : LastTimeTargetSeen;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bLostSightTooLong = !bHasLOS && (Now - LastTimeTargetSeen) > LoseSightGraceSeconds;
	if (bLostSightTooLong)
	{
		StartSearchFromLastKnownLocation();
		return;
	}

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->MoveToActor(Target, MovementAcceptanceRadius, true, true, true);
	}

	ApplyDirectChaseFallback(Target, DeltaSeconds);

	if (GetName().Contains(TEXT("Ghoul")))
	{
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[GhoulMoveDebug] Ghost=%s Target=%s Controller=%s Mode=%d Vel=%s Loc=%s TargetLoc=%s HasLOS=%d"),
				*GetNameSafe(this),
				*GetNameSafe(Target),
				*GetNameSafe(GetController()),
				(int32)MoveComp->MovementMode,
				*GetVelocity().ToString(),
				*GetActorLocation().ToString(),
				*Target->GetActorLocation().ToString(),
				HasLineOfSightToTarget(Target));
		}
	}

	TryCatchTarget();
}

void AGvTHauntGhostBase::StartSearchFromLastKnownLocation()
{
	if (!HasAuthority())
	{
		return;
	}

	SearchElapsedSeconds = 0.f;
	SearchRepathTimer = SearchRepathInterval;
	SetHauntState(EGvTHauntGhostState::Searching);
	OnHauntSearchStarted();
	MoveToSearchLocation(LastKnownTargetLocation);

	UE_LOG(LogTemp, Warning,
		TEXT("[HauntGhost] Lost LOS. Searching last known location. Ghost=%s LastKnown=%s"),
		*GetNameSafe(this),
		*LastKnownTargetLocation.ToString());
}

void AGvTHauntGhostBase::UpdateSearch(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	SearchElapsedSeconds += DeltaSeconds;

	if (SearchElapsedSeconds >= SearchRetargetDelaySeconds && TryResumeChaseFromSearch())
	{
		return;
	}

	if (SearchAfterLostSightSeconds > 0.f && SearchElapsedSeconds >= SearchAfterLostSightSeconds)
	{
		HandleSearchExpired();
		return;
	}

	SearchRepathTimer += DeltaSeconds;
	if (SearchRepathTimer >= SearchRepathInterval)
	{
		SearchRepathTimer = 0.f;

		FVector NextSearchLocation = LastKnownTargetLocation;
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			FNavLocation NavLocation;
			if (NavSys->GetRandomReachablePointInRadius(LastKnownTargetLocation, SearchPointRadius, NavLocation))
			{
				NextSearchLocation = NavLocation.Location;
			}
		}

		MoveToSearchLocation(NextSearchLocation);
	}
}

void AGvTHauntGhostBase::MoveToSearchLocation(const FVector& SearchLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		const FVector SafeSearchLocation = GetNavigationSafeGhostLocation(SearchLocation);
		AI->MoveToLocation(SafeSearchLocation, CatchDistance, true, true, true, true, nullptr, true);
	}
}

bool AGvTHauntGhostBase::TryResumeChaseFromSearch()
{
	if (!HasAuthority())
	{
		return false;
	}

	APawn* VisibleVictim = FindBestVisibleVictim();
	if (!IsValid(VisibleVictim))
	{
		return false;
	}

	SetCurrentChaseTarget(VisibleVictim);
	LastKnownTargetLocation = VisibleVictim->GetActorLocation();
	LastTimeTargetSeen = GetWorld() ? GetWorld()->GetTimeSeconds() : LastTimeTargetSeen;
	SearchElapsedSeconds = 0.f;
	SearchRepathTimer = 0.f;
	SetHauntState(EGvTHauntGhostState::Chasing);
	OnHauntChaseStarted(VisibleVictim);

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->MoveToActor(VisibleVictim, MovementAcceptanceRadius, true, true, true);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[HauntGhost] Search found visible victim. Ghost=%s Target=%s Assigned=%s"),
		*GetNameSafe(this),
		*GetNameSafe(VisibleVictim),
		*GetNameSafe(AssignedChaseTarget.Get()));

	return true;
}

void AGvTHauntGhostBase::HandleSearchExpired()
{
	if (!HasAuthority())
	{
		return;
	}

	StartHauntDespawnSequence();
}

void AGvTHauntGhostBase::TryCatchTarget()
{
	AActor* Target = CurrentTarget.Get();
	if (!IsValid(Target))
	{
		return;
	}

	const float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if (Dist > CatchDistance)
	{
		return;
	}

	if (bRequireLineOfSightToCatch && !HasLineOfSightToTarget(Target))
	{
		return;
	}

	HandleCaughtTarget(Target);
}

void AGvTHauntGhostBase::HandleCaughtTarget(AActor* Target)
{
	if (!HasAuthority() || !Target)
	{
		return;
	}

	OnHauntTargetCaught(Target);

	if (AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(Target))
	{
		Thief->Server_SetDead(this);
	}

	if (bContinueHuntAfterKill)
	{
		SetCurrentChaseTarget(nullptr);
	}
	else
	{
		StartHauntDespawnSequence();
	}
}

void AGvTHauntGhostBase::StartHauntDespawnSequence()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(TimerHandle_BeginHauntAfterSpawn);
	GetWorldTimerManager().ClearTimer(TimerHandle_FinishDespawn);

	OnHauntChaseEnded();
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
		ResetMeshVisualTransform();
		break;
	case EGvTHauntGhostState::SpawnIntro:
		SetGhostPresenceActive(true);
		EnterSpawnIntroHold();
		BP_OnGhostSpawnIntroStarted(PendingHauntTag);
		break;
	case EGvTHauntGhostState::Roaming:
	case EGvTHauntGhostState::Investigating:
	case EGvTHauntGhostState::Searching:
		ResetMeshVisualTransform();

		if (HasAuthority())
		{
			if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
			{
				MoveComp->SetMovementMode(MOVE_Walking);
				MoveComp->GravityScale = 1.f;
				MoveComp->MaxWalkSpeed = RoamSpeed;
			}
		}
		break;
	case EGvTHauntGhostState::Chasing:
		SetGhostPresenceActive(true);
		ResetMeshVisualTransform();

		if (HasAuthority())
		{
			if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
			{
				MoveComp->SetMovementMode(MOVE_Walking);
				MoveComp->MaxWalkSpeed = ChaseSpeed;

				MoveComp->MaxWalkSpeed = ChaseSpeed;
				MoveComp->MaxAcceleration = 2048.f;
				MoveComp->BrakingDecelerationWalking = 2048.f;
				MoveComp->GroundFriction = 8.f;
				MoveComp->bOrientRotationToMovement = true;
			}
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

void AGvTHauntGhostBase::CacheDefaultMeshTransform()
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		DefaultMeshRelativeLocation = MeshComp->GetRelativeLocation();
		DefaultMeshRelativeRotation = MeshComp->GetRelativeRotation();
		bCachedDefaultMeshTransform = true;
	}
}

void AGvTHauntGhostBase::ResetMeshVisualTransform()
{
	if (!bCachedDefaultMeshTransform)
	{
		CacheDefaultMeshTransform();
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeLocation(DefaultMeshRelativeLocation);
		MeshComp->SetRelativeRotation(DefaultMeshRelativeRotation);
		MeshComp->SetVisibility(true, true);
	}
}


void AGvTHauntGhostBase::OnHauntChaseStarted(AActor* Target)
{
}

void AGvTHauntGhostBase::OnHauntChaseEnded()
{
}

void AGvTHauntGhostBase::OnHauntSearchStarted()
{
}

void AGvTHauntGhostBase::OnHauntTargetCaught(AActor* Target)
{
}

void AGvTHauntGhostBase::ConfigureClientGhostProxyMovement()
{
	if (HasAuthority())
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->GravityScale = 0.f;
		MoveComp->SetMovementMode(MOVE_Flying);
		MoveComp->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
		MoveComp->bNetworkSmoothingComplete = true;
	}
}

void AGvTHauntGhostBase::ApplyDirectChaseFallback(AActor* Target, float DeltaSeconds)
{
	if (!HasAuthority() || !Target || !bUseDirectChaseFallback)
	{
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.f;

	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const FVector Direction = ToTarget.GetSafeNormal();
	AddMovementInput(Direction, 1.f);

	SetActorRotation(FMath::RInterpTo(
		GetActorRotation(),
		Direction.Rotation(),
		DeltaSeconds,
		8.f));
}