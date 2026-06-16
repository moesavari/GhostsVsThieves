#include "Gameplay/Ghosts/Ghoul/GvTGhoulGhostCharacter.h"
#include "AIController.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Volume.h"
#include "EngineUtils.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "World/Doors/GvTDoorActor.h"

AGvTGhoulGhostCharacter::AGvTGhoulGhostCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	bOnlyRelevantToOwner = false;
	NetDormancy = DORM_Never;
	NetCullDistanceSquared = FMath::Square(30000.f);
	SetReplicateMovement(true);
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	}
}

void AGvTGhoulGhostCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Blueprint assets can serialize networking flags from older class defaults.
	// Force haunt ghosts to remain group-visible at runtime, not owner-only.
	bReplicates = true;
	bAlwaysRelevant = true;
	bOnlyRelevantToOwner = false;
	NetDormancy = DORM_Never;
	SetReplicateMovement(true);
	SetGhostPresenceActive(true);
	FlushNetDormancy();
	ForceNetUpdate();

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = MaxSpeed;
		Move->MaxAcceleration = Acceleration;
		Move->BrakingDecelerationWalking = BrakingDecel;
	}

}

void AGvTGhoulGhostCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector Vel2D(GetVelocity().X, GetVelocity().Y, 0.f);
	ReplicatedSpeed = Vel2D.Size();

	if (!HasAuthority())
	{
		return;
	}

	if (bIsChasing)
	{
		ChaseTick(DeltaSeconds);
	}
	else if (bIsSearching)
	{
		SearchTick(DeltaSeconds);
	}
}

void AGvTGhoulGhostCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTGhoulGhostCharacter, TargetVictim);
	DOREPLIFETIME(AGvTGhoulGhostCharacter, AssignedTargetVictim);
	DOREPLIFETIME(AGvTGhoulGhostCharacter, ReplicatedSpeed);
	DOREPLIFETIME(AGvTGhoulGhostCharacter, bIsChasing);
	DOREPLIFETIME(AGvTGhoulGhostCharacter, bIsPerformingScare);
	DOREPLIFETIME(AGvTGhoulGhostCharacter, bIsSearching);
}

void AGvTGhoulGhostCharacter::BeginGhostScare(AActor* Target, FGameplayTag ScareTag)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[GhoulGhost] BeginGhostScare Target=%s Tag=%s"),
		*GetNameSafe(Target),
		*ScareTag.ToString());

	if (!ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Close()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GhoulGhost] Unsupported GhostScare tag: %s"), *ScareTag.ToString());
		return;
	}

	BeginCloseScarePresentation(Target);
}

void AGvTGhoulGhostCharacter::BeginGhostHaunt(AActor* Target, FGameplayTag HauntTag)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[GhoulGhost] BeginGhostHaunt Target=%s Tag=%s"),
		*GetNameSafe(Target),
		*HauntTag.ToString());

	Super::BeginGhostHaunt(Target, HauntTag);
}

void AGvTGhoulGhostCharacter::BeginHauntAction(AActor* Target, FGameplayTag HauntTag)
{
	if (!HauntTag.MatchesTagExact(GvTScareTags::GhostHaunt_Chase()))
	{
		StartHauntDespawnSequence();
		return;
	}

	APawn* VictimPawn = Cast<APawn>(Target);
	if (!VictimPawn)
	{
		StartHauntDespawnSequence();
		return;
	}

	if (HasAuthority())
	{
		Server_StartChase_Implementation(VictimPawn);
	}
	else
	{
		Server_StartChase(VictimPawn);
	}
}

void AGvTGhoulGhostCharacter::Server_StartChase_Implementation(APawn* Victim)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	SetGhostPresenceActive(true);
	TargetVictim = Victim;
	AssignedTargetVictim = Victim;
	SetAssignedChaseTarget(Victim);
	SetCurrentChaseTarget(Victim);
	bIsChasing = true;
	bIsSearching = false;
	bIsPerformingScare = false;
	SetHauntState(EGvTHauntGhostState::Chasing);
	RepathTimer = 0.f;
	DirectChaseStuckTime = 0.f;
	DoorProbeTimer = 0.f;
	SearchElapsedSeconds = 0.f;
	SearchRepathTimer = 0.f;
	ChaseStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	LastDistanceToVictim = FVector::Dist(GetActorLocation(), TargetVictim->GetActorLocation());
	LastChaseProgressTimeSeconds = ChaseStartTimeSeconds;
	LastKnownVictimLocation = TargetVictim->GetActorLocation();
	LastTimeVictimSeenSeconds = ChaseStartTimeSeconds;
	GetWorldTimerManager().ClearTimer(TimerHandle_SearchExpired);

	if (!GetController())
	{
		SpawnDefaultController();
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
		Move->MaxWalkSpeed = MaxSpeed;
		Move->MaxAcceleration = Acceleration;
		Move->BrakingDecelerationWalking = BrakingDecel;
	}

	PlayScareAudioStart(ChaseAudio, true);
	StartScareAudioSustain(ChaseAudio, true);

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		const FVector DesiredTargetLocation = GetBoundedTargetLocation();
		const EPathFollowingRequestResult::Type MoveResult = AI->MoveToLocation(DesiredTargetLocation, AcceptRadius, true, true, true, true, nullptr, true);
		UE_LOG(LogTemp, Warning, TEXT("[Ghoul] Initial MoveToLocation Result=%d Target=%s"), static_cast<int32>(MoveResult), *DesiredTargetLocation.ToString());
	}

	UE_LOG(LogTemp, Warning, TEXT("[Ghoul] StartChase Ghost=%s Victim=%s Controller=%s Loc=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Victim),
		*GetNameSafe(GetController()),
		*GetActorLocation().ToString());
}

void AGvTGhoulGhostCharacter::UpdateChase(float DeltaSeconds)
{
	// Ghoul owns its chase loop in ChaseTick().
	// The generic haunt base would otherwise run its test catch/stop flow and
	// reset the shared haunt state before the Ghoul-specific chase can finish.
}

void AGvTGhoulGhostCharacter::ChaseTick(float DeltaSeconds)
{
	if (!IsValid(TargetVictim))
	{
		StopAndVanish();
		return;
	}

	const float DistToVictim = FVector::Dist(GetActorLocation(), TargetVictim->GetActorLocation());
	const float NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const FVector PreMoveLocation = GetActorLocation();

	if (LastDistanceToVictim <= 0.f || DistToVictim < LastDistanceToVictim - ChaseProgressDistanceThreshold)
	{
		LastDistanceToVictim = DistToVictim;
		LastChaseProgressTimeSeconds = NowSeconds;
	}

	if (MaxChaseDistance > 0.f && DistToVictim > MaxChaseDistance)
	{
		StopAndVanish();
		return;
	}

	if (bRequireLineOfSightToKeepChasing)
	{
		const bool bHasLOS = HasLineOfSightToVictim();
		if (bHasLOS)
		{
			LastKnownVictimLocation = TargetVictim->GetActorLocation();
			LastTimeVictimSeenSeconds = NowSeconds;
		}
		else if (NowSeconds - LastTimeVictimSeenSeconds >= LostLineOfSightGraceSeconds)
		{
			StartSearchFromLastKnownLocation();
			return;
		}
	}

	RepathTimer += DeltaSeconds;
	if (RepathTimer >= RepathInterval)
	{
		RepathTimer = 0.f;
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			const FVector DesiredTargetLocation = GetBoundedTargetLocation();
			const EPathFollowingRequestResult::Type MoveResult = AI->MoveToLocation(DesiredTargetLocation, AcceptRadius, true, true, true, true, nullptr, true);
			UE_LOG(LogTemp, Verbose, TEXT("[Ghoul] Repath MoveToLocation Result=%d Dist=%.1f Target=%s"), static_cast<int32>(MoveResult), DistToVictim, *DesiredTargetLocation.ToString());
		}
	}

	// During haunt chase, face movement direction instead of staring at the target.
	// CharacterMovement handles normal path-following rotation; fallback movement rotates to its own direction.

	DoorProbeTimer += DeltaSeconds;
	if (DoorProbeTimer >= DoorProbeInterval)
	{
		DoorProbeTimer = 0.f;
		TryOpenNearbyDoors();
	}

	const float Speed2D = FVector(GetVelocity().X, GetVelocity().Y, 0.f).Size();
	bool bAppliedFallbackInput = false;
	if (bUseDirectChaseFallback && DistToVictim > AcceptRadius + 50.f && Speed2D <= DirectChaseMinSpeed)
	{
		DirectChaseStuckTime += DeltaSeconds;
		if (DirectChaseStuckTime >= DirectChaseFallbackForceDelay)
		{
			FVector FallbackDir = FVector::ZeroVector;
			if (FindPathAwareFallbackDirection(FallbackDir))
			{
				bAppliedFallbackInput = ApplyPathAwareFallbackMove(FallbackDir, DeltaSeconds);
			}
		}
	}
	else
	{
		DirectChaseStuckTime = 0.f;
	}

	const float PostMoveDist = FVector::Dist(GetActorLocation(), TargetVictim->GetActorLocation());
	if (PostMoveDist < LastDistanceToVictim - ChaseProgressDistanceThreshold)
	{
		LastDistanceToVictim = PostMoveDist;
		LastChaseProgressTimeSeconds = NowSeconds;
	}

	const float ActualMoveSpeed2D = FVector(GetActorLocation() - PreMoveLocation).Size2D() / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
	ReplicatedSpeed = FMath::Max(Speed2D, ActualMoveSpeed2D);

	UE_LOG(LogTemp, Warning, TEXT("[Ghoul] ChaseTick Victim=%s Dist=%.1f Vel=%.1f Actual=%.1f Fallback=%d Stuck=%.2f ProgressAge=%.2f"),
		*GetNameSafe(TargetVictim),
		PostMoveDist,
		Speed2D,
		ActualMoveSpeed2D,
		bAppliedFallbackInput ? 1 : 0,
		DirectChaseStuckTime,
		NowSeconds - LastChaseProgressTimeSeconds);

	TryCatchVictim();
}

bool AGvTGhoulGhostCharacter::ApplyPathAwareFallbackMove(const FVector& Direction, float DeltaSeconds)
{
	if (!HasAuthority() || Direction.IsNearlyZero() || DeltaSeconds <= 0.f)
	{
		return false;
	}

	FVector MoveDir = Direction;
	MoveDir.Z = 0.f;
	if (!MoveDir.Normalize())
	{
		return false;
	}

	const FVector Before = GetActorLocation();
	const FVector Delta = MoveDir * FMath::Max(MaxSpeed, 0.f) * DeltaSeconds;
	const FVector DesiredLocation = Before + Delta;
	if (!IsLocationInsideHauntBounds(DesiredLocation))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Ghoul] Fallback move rejected by haunt bounds Ghost=%s Desired=%s"), *GetNameSafe(this), *DesiredLocation.ToString());
		return false;
	}

	FaceMovementDirection(MoveDir, DeltaSeconds, 720.f);

	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit, ETeleportType::None);

	if (Hit.bBlockingHit)
	{
		if (AGvTDoorActor* Door = Cast<AGvTDoorActor>(Hit.GetActor()))
		{
			Door->OpenForGhost(this);
			MoveIgnoreActorAdd(Door);
		}

		UE_LOG(LogTemp, Verbose, TEXT("[Ghoul] Fallback sweep blocked Ghost=%s Hit=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Hit.GetActor()));
	}

	return FVector::DistSquared2D(Before, GetActorLocation()) > 1.f;
}

bool AGvTGhoulGhostCharacter::FindPathAwareFallbackDirection(FVector& OutDirection)
{
	OutDirection = FVector::ZeroVector;

	if (!HasAuthority() || !IsValid(TargetVictim))
	{
		return false;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector TargetLocation = GetBoundedTargetLocation();

	// Prefer the navmesh path's next corner. This avoids the old direct shove that
	// could cut through walls whenever path following stalled for a frame.
	if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
		this,
		CurrentLocation,
		TargetLocation))
	{
		if (Path->IsValid() && Path->PathPoints.Num() >= 2)
		{
			const FVector NextPoint = Path->PathPoints[1];
			if (!IsLocationInsideHauntBounds(NextPoint))
			{
				return false;
			}

			FVector ToNextPoint = NextPoint - CurrentLocation;
			ToNextPoint.Z = 0.f;

			if (!ToNextPoint.IsNearlyZero())
			{
				OutDirection = ToNextPoint.GetSafeNormal();
				return true;
			}
		}
	}

	// Only use direct pursuit if there is actual line of sight. If a wall is in the
	// way, do not "help" the ghost by forcing it through the wall. Doors are the
	// exception: open them and let the next tick/path update handle movement.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GhoulFallbackLOS), false, this);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(TargetVictim);

	const FVector TraceStart = CurrentLocation + FVector(0.f, 0.f, 40.f);
	const FVector TraceEnd = TargetLocation + FVector(0.f, 0.f, 40.f);
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

	if (bBlocked)
	{
		if (AGvTDoorActor* HitDoor = Cast<AGvTDoorActor>(Hit.GetActor()))
		{
			HitDoor->OpenForGhost(this);
			MoveIgnoreActorAdd(HitDoor);
		}

		return false;
	}

	FVector DirectDir = TargetLocation - CurrentLocation;
	DirectDir.Z = 0.f;
	if (DirectDir.IsNearlyZero())
	{
		return false;
	}

	OutDirection = DirectDir.GetSafeNormal();
	return true;
}


bool AGvTGhoulGhostCharacter::HasLineOfSightToVictim() const
{
	return HasLineOfSightToPawn(TargetVictim);
}

void AGvTGhoulGhostCharacter::SwitchTargetVictim(APawn* NewVictim)
{
	if (!HasAuthority() || !IsValid(NewVictim))
	{
		return;
	}

	TargetVictim = NewVictim;
	SetCurrentChaseTarget(NewVictim);

	const float NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	LastKnownVictimLocation = NewVictim->GetActorLocation();
	LastTimeVictimSeenSeconds = NowSeconds;
	LastDistanceToVictim = FVector::Dist(GetActorLocation(), NewVictim->GetActorLocation());
	LastChaseProgressTimeSeconds = NowSeconds;
	SearchElapsedSeconds = 0.f;
	SearchRepathTimer = 0.f;
	DirectChaseStuckTime = 0.f;

	bIsSearching = false;
	bIsChasing = true;
	SetHauntState(EGvTHauntGhostState::Chasing);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
		Move->MaxWalkSpeed = MaxSpeed;
	}

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->MoveToLocation(GetBoundedTargetLocation(), AcceptRadius, true, true, true, true, nullptr, true);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Ghoul] Switched target. Ghost=%s NewVictim=%s AssignedVictim=%s"),
		*GetNameSafe(this),
		*GetNameSafe(NewVictim),
		*GetNameSafe(GetAssignedChaseTarget()));
}

void AGvTGhoulGhostCharacter::StartSearchFromLastKnownLocation()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsChasing = false;
	bIsSearching = true;
	DirectChaseStuckTime = 0.f;
	SearchElapsedSeconds = 0.f;
	SearchRepathTimer = SearchRepathInterval;
	SetHauntState(EGvTHauntGhostState::Searching);
	SnapGhostToNavigationSafeHeight();

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
		Move->MaxWalkSpeed = FMath::Min(MaxSpeed, RoamSpeed > 0.f ? RoamSpeed : MaxSpeed);
	}

	MoveToSearchLocation(LastKnownVictimLocation);

	UE_LOG(LogTemp, Warning, TEXT("[Ghoul] Lost LOS. Entering search Ghost=%s Target=%s LastKnown=%s SearchSeconds=%.2f"),
		*GetNameSafe(this),
		*GetNameSafe(TargetVictim),
		*LastKnownVictimLocation.ToString(),
		SearchAfterLostSightSeconds);

	GetWorldTimerManager().ClearTimer(TimerHandle_SearchExpired);
}

void AGvTGhoulGhostCharacter::SearchTick(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	SearchElapsedSeconds += DeltaSeconds;

	// Readability window: after losing LOS, stay in Searching briefly so the animation/state is visible.
	// After that, the ghost can reacquire the assigned victim or opportunistically switch to another visible thief.
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

		FVector NextSearchLocation = LastKnownVictimLocation;
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			FNavLocation NavLocation;
			if (NavSys->GetRandomReachablePointInRadius(LastKnownVictimLocation, SearchPointRadius, NavLocation))
			{
				NextSearchLocation = NavLocation.Location;
			}
		}

		MoveToSearchLocation(NextSearchLocation);
	}

	DoorProbeTimer += DeltaSeconds;
	if (DoorProbeTimer >= DoorProbeInterval)
	{
		DoorProbeTimer = 0.f;
		TryOpenNearbyDoors();
	}
}

void AGvTGhoulGhostCharacter::MoveToSearchLocation(const FVector& SearchLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		const FVector SafeSearchLocation = GetNavigationSafeGhostLocation(SearchLocation);
		const EPathFollowingRequestResult::Type MoveResult = AI->MoveToLocation(SafeSearchLocation, AcceptRadius, true, true, true, true, nullptr, true);
		UE_LOG(LogTemp, Warning, TEXT("[Ghoul] Search MoveToLocation Result=%d Raw=%s Safe=%s"),
			static_cast<int32>(MoveResult),
			*SearchLocation.ToString(),
			*SafeSearchLocation.ToString());
	}
}

bool AGvTGhoulGhostCharacter::TryResumeChaseFromSearch()
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

	SwitchTargetVictim(VisibleVictim);
	UE_LOG(LogTemp, Warning, TEXT("[Ghoul] Search found visible victim. Resuming chase Ghost=%s Target=%s"),
		*GetNameSafe(this),
		*GetNameSafe(VisibleVictim));

	return true;
}

void AGvTGhoulGhostCharacter::HandleSearchExpired()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Ghoul] Search expired. Despawning Ghost=%s LastKnown=%s"),
		*GetNameSafe(this),
		*LastKnownVictimLocation.ToString());

	StopAndVanish();
}

void AGvTGhoulGhostCharacter::TryCatchVictim()
{
	if (!HasAuthority() || !IsValid(TargetVictim) || GhoulCatchDistance <= 0.f)
	{
		return;
	}

	const float ChaseAge = GetWorld() ? GetWorld()->GetTimeSeconds() - ChaseStartTimeSeconds : 0.f;
	const float Speed2D = FVector(GetVelocity().X, GetVelocity().Y, 0.f).Size();
	const float CatchSpeed = FMath::Max(Speed2D, ReplicatedSpeed);

	const float NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bMadeRecentProgress = (NowSeconds - LastChaseProgressTimeSeconds) <= MaxTimeSinceProgressForCatch;

	if (ChaseAge < MinimumChaseTimeBeforeCatch || !bMadeRecentProgress)
	{
		return;
	}

	// Path-following and direct fallback can both report zero Character velocity
	// while the ghost is still making real world-position progress. Do not let
	// that block a valid catch once the Ghoul has actually closed distance.
	if (MinimumSpeedToCatch > 0.f && CatchSpeed < MinimumSpeedToCatch && (NowSeconds - LastChaseProgressTimeSeconds) > 0.15f)
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), TargetVictim->GetActorLocation()) <= GhoulCatchDistance)
	{
		if (AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(TargetVictim))
		{
			Thief->Server_SetDead(this);
		}

		if (bContinueHuntAfterKill)
		{
			TargetVictim = nullptr;
			CurrentTarget = nullptr;
			StartSearchFromLastKnownLocation();
		}
		else
		{
			StopAndVanish();
		}
	}
}

void AGvTGhoulGhostCharacter::StopAndVanish()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsChasing = false;
	bIsSearching = false;
	bIsPerformingScare = false;
	TargetVictim = nullptr;
	AssignedTargetVictim = nullptr;
	SetCurrentChaseTarget(nullptr);
	DirectChaseStuckTime = 0.f;
	SearchElapsedSeconds = 0.f;
	SearchRepathTimer = 0.f;
	LastDistanceToVictim = 0.f;
	LastChaseProgressTimeSeconds = 0.f;

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}

	StopCurrentScareAudio(true, &ChaseAudio, true);
	StartHauntDespawnSequence();
}

void AGvTGhoulGhostCharacter::TryOpenNearbyDoors()
{
	if (!HasAuthority() || DoorProbeRadius <= 0.f)
	{
		return;
	}

	const FVector ProbeCenter = GetActorLocation() + GetActorForwardVector() * DoorProbeForwardOffset;
	TArray<AActor*> Doors;
	UGameplayStatics::GetAllActorsOfClass(this, AGvTDoorActor::StaticClass(), Doors);

	for (AActor* Actor : Doors)
	{
		AGvTDoorActor* Door = Cast<AGvTDoorActor>(Actor);
		if (!Door || Door->IsOpen())
		{
			continue;
		}

		if (FVector::DistSquared(ProbeCenter, Door->GetActorLocation()) > FMath::Square(DoorProbeRadius))
		{
			continue;
		}

		if (Door->OpenForGhost(this))
		{
			MoveIgnoreActorAdd(Door);
		}
	}
}

void AGvTGhoulGhostCharacter::BeginCloseScarePresentation(AActor* Target)
{
	bIsChasing = false;
	bIsPerformingScare = true;
	SetHauntState(EGvTHauntGhostState::PerformingScare);
	TargetVictim = Cast<APawn>(Target);
	DirectChaseStuckTime = 0.f;

	SetGhostPresenceActive(true);
	SetActorEnableCollision(false);

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
		Move->GravityScale = 0.f;
	}

	FVector ViewLoc = Target ? Target->GetActorLocation() + FVector(0.f, 0.f, 80.f) : GetActorLocation();
	FRotator ViewRot = Target ? Target->GetActorRotation() : GetActorRotation();

	if (const APawn* TargetPawn = Cast<APawn>(Target))
	{
		if (const APlayerController* PC = Cast<APlayerController>(TargetPawn->GetController()))
		{
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		}
	}

	FVector Forward = ViewRot.Vector();
	Forward.Z = 0.f;
	Forward.Normalize();
	if (Forward.IsNearlyZero())
	{
		Forward = Target ? Target->GetActorForwardVector() : GetActorForwardVector();
	}

	const FVector SpawnLoc = ViewLoc + Forward * CloseScareForwardOffset + FVector(0.f, 0.f, CloseScareCameraZOffset);
	SetActorLocation(SpawnLoc, false, nullptr, ETeleportType::TeleportPhysics);
	FaceTargetFlat(Target, 0.f, 0.f);

	PlayScareAudioStart(CloseScareAudio, true);

	if (AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(Target))
	{
		Thief->ApplyScareStun(CloseScareStunDuration);
	}

	GetWorldTimerManager().ClearTimer(TimerHandle_SearchExpired);
	GetWorldTimerManager().ClearTimer(TimerHandle_CloseScareEnd);
	GetWorldTimerManager().SetTimer(
		TimerHandle_CloseScareEnd,
		this,
		&AGvTGhoulGhostCharacter::EndCloseScarePresentation,
		CloseScareDuration,
		false);

	UE_LOG(LogTemp, Warning, TEXT("[GhoulGhost] Close scare presentation anchored at %s Target=%s"),
		*GetActorLocation().ToString(),
		*GetNameSafe(Target));
}

void AGvTGhoulGhostCharacter::EndCloseScarePresentation()
{
	bIsPerformingScare = false;
	SetHauntState(EGvTHauntGhostState::Idle);
	TargetVictim = nullptr;
	StopCurrentScareAudio(false, nullptr, true);
	Destroy();
}

FVector AGvTGhoulGhostCharacter::GetBoundedTargetLocation() const
{
	if (!IsValid(TargetVictim))
	{
		return GetActorLocation();
	}

	const FVector TargetLocation = TargetVictim->GetActorLocation();
	if (IsLocationInsideHauntBounds(TargetLocation))
	{
		return TargetLocation;
	}

	// Do not chase outside the haunt bounds. Until we add a smarter "closest point in volume"
	// helper, hold the ghost at its current legal position rather than pathing outside.
	return GetActorLocation();
}

bool AGvTGhoulGhostCharacter::IsLocationInsideHauntBounds(const FVector& Location) const
{
	if (!bRestrictToHauntBounds || HauntBoundsTag.IsNone())
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	bool bFoundBounds = false;
	for (TActorIterator<AVolume> It(World); It; ++It)
	{
		const AVolume* Volume = *It;
		if (!Volume || !Volume->ActorHasTag(HauntBoundsTag))
		{
			continue;
		}

		bFoundBounds = true;
		if (Volume->EncompassesPoint(Location, 0.f, nullptr))
		{
			return true;
		}
	}

	// No placed/tagged bounds volume means old behavior: unrestricted.
	return !bFoundBounds;
}

void AGvTGhoulGhostCharacter::FaceMovementDirection(const FVector& Direction, float DeltaSeconds, float TurnSpeedDegPerSecond)
{
	FVector FlatDir = Direction;
	FlatDir.Z = 0.f;
	if (!FlatDir.Normalize())
	{
		return;
	}

	const FRotator DesiredRot = FlatDir.Rotation();
	const FRotator NewRot = DeltaSeconds > 0.f && TurnSpeedDegPerSecond > 0.f
		? FMath::RInterpConstantTo(GetActorRotation(), DesiredRot, DeltaSeconds, TurnSpeedDegPerSecond)
		: DesiredRot;

	SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));
}

void AGvTGhoulGhostCharacter::FaceTargetFlat(const AActor* Target, float DeltaSeconds, float TurnSpeedDegPerSecond)
{
	if (!Target)
	{
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const FRotator DesiredRot = ToTarget.Rotation();
	const FRotator NewRot = DeltaSeconds > 0.f && TurnSpeedDegPerSecond > 0.f
		? FMath::RInterpConstantTo(GetActorRotation(), DesiredRot, DeltaSeconds, TurnSpeedDegPerSecond)
		: DesiredRot;

	SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));
}

void AGvTGhoulGhostCharacter::PlayScareAudioStart(const FGvTScareAudioSet& AudioSet, bool bAttachToActor)
{
	if (!AudioSet.StartSfx)
	{
		return;
	}

	if (bAttachToActor)
	{
		UGameplayStatics::SpawnSoundAttached(AudioSet.StartSfx, GetRootComponent());
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(this, AudioSet.StartSfx, GetActorLocation());
	}
}

void AGvTGhoulGhostCharacter::StartScareAudioSustain(const FGvTScareAudioSet& AudioSet, bool bAttachToActor)
{
	if (!AudioSet.SustainLoopSfx)
	{
		return;
	}

	if (ActiveSustainAudio)
	{
		ActiveSustainAudio->Stop();
		ActiveSustainAudio = nullptr;
	}

	ActiveSustainAudio = bAttachToActor
		? UGameplayStatics::SpawnSoundAttached(
			AudioSet.SustainLoopSfx,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset,
			true,
			AudioSet.VolumeMultiplier,
			AudioSet.PitchMultiplier)
		: UGameplayStatics::SpawnSoundAtLocation(
			this,
			AudioSet.SustainLoopSfx,
			GetActorLocation(),
			GetActorRotation(),
			AudioSet.VolumeMultiplier,
			AudioSet.PitchMultiplier,
			0.f,
			nullptr,
			nullptr,
			true);
}

void AGvTGhoulGhostCharacter::StopCurrentScareAudio(bool bPlayEnd, const FGvTScareAudioSet* AudioSet, bool bAttachToActor)
{
	if (ActiveSustainAudio)
	{
		ActiveSustainAudio->Stop();
		ActiveSustainAudio = nullptr;
	}

	if (bPlayEnd && AudioSet && AudioSet->EndSfx)
	{
		if (bAttachToActor)
		{
			UGameplayStatics::SpawnSoundAttached(AudioSet->EndSfx, GetRootComponent());
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(this, AudioSet->EndSfx, GetActorLocation());
		}
	}
}
