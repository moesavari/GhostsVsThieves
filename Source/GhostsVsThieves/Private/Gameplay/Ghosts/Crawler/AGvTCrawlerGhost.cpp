#include "Gameplay/Ghosts/Crawler/AGvTCrawlerGhost.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"

AGvTCrawlerGhost::AGvTCrawlerGhost()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	RootComponent = Capsule;

	Capsule->InitCapsuleSize(34.f, 44.f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Mesh->SetIsReplicated(false);

    // Default tuning presets (can be edited per BP instance).
    PatrolAnimTuning.RefCrawlSpeed = 250.f;
    PatrolAnimTuning.MinPlayRate = 0.15f;
    PatrolAnimTuning.MaxPlayRate = 2.0f;
    PatrolAnimTuning.PlayRateScalar = 1.0f;

    ChaseAnimTuning.RefCrawlSpeed = 350.f;
    ChaseAnimTuning.MinPlayRate = 0.2f;
    ChaseAnimTuning.MaxPlayRate = 5.0f; // allow you to crank it up without recompiling
    ChaseAnimTuning.PlayRateScalar = 1.0f;

}

void AGvTCrawlerGhost::BeginPlay()
{
	Super::BeginPlay();

	PrevLoc = GetActorLocation();

	EnterState(State);
}

void AGvTCrawlerGhost::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	const FVector Now = GetActorLocation();
	const float Dist = FVector::Dist2D(Now, PrevLoc);
	ReplicatedSpeed = (DeltaSeconds > KINDA_SMALL_NUMBER) ? (Dist / DeltaSeconds) : 0.f;
	PrevLoc = Now;

	switch (State)
	{
	case EGvTCrawlerGhostState::DropScare:
	{
		
		DropT += DeltaSeconds;
		const float Alpha = FMath::Clamp(DropT / FMath::Max(0.01f, DropDurationRuntime), 0.f, 1.f);

		const FVector NewLoc = FMath::Lerp(DropStart, DropEnd, Alpha);
		SetActorLocation(NewLoc, false);

		// Rotate toward victim camera (reads much scarier than a static pose).
		if (bDropFaceVictimCamera && IsValid(TargetVictim))
		{
			FVector CamLoc = TargetVictim->GetActorLocation();

			if (APawn* Pawn = Cast<APawn>(TargetVictim))
			{
				if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
				{
					if (PC->PlayerCameraManager)
					{
						CamLoc = PC->PlayerCameraManager->GetCameraLocation();
					}
				}
			}

			FRotator WantRot = (CamLoc - GetActorLocation()).Rotation();
			if (bDropUpsideDown)
			{
				WantRot.Roll = 180.f;
			}

			const FRotator NewRot = FMath::RInterpConstantTo(GetActorRotation(), WantRot, DeltaSeconds, DropTurnSpeedDegPerSec);
			SetActorRotation(NewRot);
		}

		if (Alpha >= 1.f)
		{
			// Hold in the drop scare state to let the non-looping anim sequence play.
			DropHoldT += DeltaSeconds;

			const float HoldLimit = bDropUseAnimSequence ? DropScareAnimDuration : 0.f;
			if (HoldLimit <= 0.f || DropHoldT >= HoldLimit)
			{
				if (bChaseAfterDropScare && IsValid(TargetVictim))
				{
					StartHauntChase_Internal(TargetVictim);
				}
				else
				{
					Vanish_Internal();
				}
			}
		}

		break;
	}

	case EGvTCrawlerGhostState::HauntChase:
	{
		if (!IsValid(TargetVictim))
		{
			Vanish_Internal();
			break;
		}

		if (bSnapToGroundInChase)
		{
			SnapToGround();
		}

		const FVector MyLoc = GetActorLocation();
		const FVector TargetLoc = TargetVictim->GetActorLocation();

		const float Distance = FVector::Dist(MyLoc, TargetLoc);
		if (Distance > MaxChaseDistance)
		{
			Vanish_Internal();
			break;
		}

		FVector ToTarget = TargetLoc - MyLoc;
		ToTarget.Z = 0.f;

		if (!ToTarget.IsNearlyZero())
		{
			const FRotator WantRot = ToTarget.Rotation();
			const FRotator NewRot = FMath::RInterpConstantTo(GetActorRotation(), WantRot, DeltaSeconds, TurnSpeedDegPerSec);
			SetActorRotation(NewRot);
		}

		// Movement:
		// - If bUseDragStepMovement is enabled, movement comes ONLY from animation notifies (OnCrawlerDragStep).
		// - Otherwise we use the simple lunge timer here.
		if (!bUseDragStepMovement)
		{
			LungeTimer += DeltaSeconds;
			if (LungeTimer >= LungeInterval)
			{
				LungeTimer = 0.f;
				bIsLunging = true;
			}

			if (bIsLunging)
			{
				const FVector Dir = ToTarget.GetSafeNormal();
				const FVector Step = Dir * (LungeSpeed * DeltaSeconds);

				SetActorLocation(MyLoc + Step, true);

				// Small fixed lunge window.
				if (LungeTimer >= 0.12f)
				{
					bIsLunging = false;
				}
			}
		}

		TryKillVictim();
		break;
	}

	case EGvTCrawlerGhostState::IdleCeiling:
	case EGvTCrawlerGhostState::Vanish:
	default:
		break;
	}
}

void AGvTCrawlerGhost::TryKillVictim()
{
	if (!HasAuthority() || !IsValid(TargetVictim))
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), TargetVictim->GetActorLocation()) <= KillDistance)
	{
		if (AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(TargetVictim))
		{
			Thief->Server_SetDead(this);
		}

		Vanish_Internal();
	}
}

FVector AGvTCrawlerGhost::GetVictimCameraLocation() const
{
	if (!IsValid(TargetVictim))
	{
		return GetActorLocation();
	}

	if (const APawn* Pawn = Cast<APawn>(TargetVictim))
	{
		if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (PC->PlayerCameraManager)
			{
				return PC->PlayerCameraManager->GetCameraLocation();
			}
		}
	}

	return TargetVictim->GetActorLocation();
}

void AGvTCrawlerGhost::SnapToGround()
{
	if (!GetWorld() || !Capsule)
	{
		return;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, GroundTraceUp);
	const FVector End = GetActorLocation() - FVector(0.f, 0.f, GroundTraceDown);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_CrawlerGroundTrace), false, this);
	Params.AddIgnoredActor(this);

	// Use Visibility so it works even if meshes aren't marked WorldStatic (and respects "Can Ever Affect Navigation" setups).
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		const FVector NewLoc = Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight);
		SetActorLocation(NewLoc, false);
	}
}

void AGvTCrawlerGhost::OnCrawlerDragStep()
{
    // AnimNotifies fire on both server and clients. This actor has no owning connection,
    // so NEVER try to RPC from client -> server here.
    if (!HasAuthority())
    {
        return;
    }

    if (State != EGvTCrawlerGhostState::HauntChase || !bUseDragStepMovement || !IsValid(TargetVictim))
    {
        return;
    }

    // Step forward toward the victim on the nav-plane.
    FVector ToVictim = TargetVictim->GetActorLocation() - GetActorLocation();
    ToVictim.Z = 0.f;

    if (!ToVictim.Normalize())
    {
        return;
    }

    const FVector Step = ToVictim * DragStepDistance;

    // Use SetActorLocation so ReplicatedSpeed reflects the real movement (and collision stays sane).
    FHitResult Hit;
    SetActorLocation(GetActorLocation() + Step, true, &Hit, ETeleportType::None);
}

void AGvTCrawlerGhost::StartDropScare(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly)
{
	if (!Victim)
	{
		return;
	}

	bLastDropWasOverhead = false;
	bOverheadPitchSnapping = false;

	if (HasAuthority())
	{
		StartDropScare_Internal(Victim, CeilingWorldPos, FaceRot, bVictimOnly);
	}
	else
	{
		Server_StartDropScare(Victim, CeilingWorldPos, FaceRot, bVictimOnly);
	}
}

void AGvTCrawlerGhost::StartOverheadScare(AActor* Victim, bool bVictimOnly)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	bLastDropWasOverhead = true;
	bOverheadPitchSnapping = true;
	OverheadPitchStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	APawn* Pawn = Cast<APawn>(Victim);
	if (!Pawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator CamRot = PC->PlayerCameraManager->GetCameraRotation();

	// Trace upward to find ceiling
	FVector CeilingPos = CamLoc + FVector(0.f, 0.f, OverheadTraceUpDistance);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_CrawlerOverheadTrace), false, this);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Victim);

	const FVector TraceStart = CamLoc;
	const FVector TraceEnd = CamLoc + FVector(0.f, 0.f, OverheadTraceUpDistance);

	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		CeilingPos = Hit.ImpactPoint - FVector(0.f, 0.f, OverheadCeilingClearance);
	}

	// Push forward slightly so it feels in front of camera
	CeilingPos += CamRot.Vector() * OverheadForwardOffset;

	// Extra camera-relative offsets for face-to-face framing.
	CeilingPos += CamRot.Vector() * OverheadFaceForward;
	CeilingPos.Z -= OverheadFaceDown;

	FRotator FaceRot = (CamLoc - CeilingPos).Rotation();

	if (bDropUpsideDown)
	{
		FaceRot.Roll = 180.f;
	}

	StartDropScare_Internal(Victim, CeilingPos, FaceRot, bVictimOnly);
}

void AGvTCrawlerGhost::StartHauntChase(AActor* Victim)
{
	if (!Victim)
	{
		return;
	}

	if (HasAuthority())
	{
		StartHauntChase_Internal(Victim);
	}
	else
	{
		Server_StartHauntChase(Victim);
	}
}

void AGvTCrawlerGhost::Vanish()
{
	if (HasAuthority())
	{
		Vanish_Internal();
	}
	else
	{
		Server_Vanish();
	}
}

void AGvTCrawlerGhost::Server_StartDropScare_Implementation(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly)
{
	StartDropScare_Internal(Victim, CeilingWorldPos, FaceRot, bVictimOnly);
}


void AGvTCrawlerGhost::Server_StartOverheadScare_Implementation(AActor* Victim, bool bVictimOnly)
{
	StartOverheadScare(Victim, bVictimOnly);
}

void AGvTCrawlerGhost::Server_StartHauntChase_Implementation(AActor* Victim)
{
	StartHauntChase_Internal(Victim);
}

void AGvTCrawlerGhost::Server_Vanish_Implementation()
{
	Vanish_Internal();
}

void AGvTCrawlerGhost::StartDropScare_Internal(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool /*bVictimOnly*/)
{
	TargetVictim = Victim;

	FRotator UseRot = FaceRot;
	if (bDropUpsideDown)
	{
		UseRot.Roll = 180.f;
	}
	SetActorRotation(UseRot);

	DropStart = CeilingWorldPos;
	DropEnd = bOverheadNoDrop ? CeilingWorldPos : (CeilingWorldPos - FVector(0.f, 0.f, DropOffsetDown));

	DropT = 0.f;
	DropHoldT = 0.f;

	DropDurationRuntime = (bOverheadNoDrop && bDropUseAnimSequence) ? 0.01f : DropDuration;

	SetActorLocation(DropStart, false);

	EnterState(EGvTCrawlerGhostState::DropScare);
}

void AGvTCrawlerGhost::StartHauntChase_Internal(AActor* Victim)
{
	TargetVictim = Victim;

	// Snap to ground so chase doesn't "hover" (or sink) if we came from an overhead scare.
	SnapToGround();

	// Ensure chase starts with a sane upright rotation (no upside-down roll carryover).
	FRotator Rot = GetActorRotation();
	Rot.Roll = 0.f;
	SetActorRotation(Rot);

	LungeTimer = 0.f;
	bIsLunging = false;

	EnterState(EGvTCrawlerGhostState::HauntChase);
}

void AGvTCrawlerGhost::Vanish_Internal()
{
	EnterState(EGvTCrawlerGhostState::Vanish);

	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetVisibility(false, true);

	SetLifeSpan(1.5f);
}

void AGvTCrawlerGhost::EnterState(EGvTCrawlerGhostState NewState)
{
	const EGvTCrawlerGhostState Prev = State;
	State = NewState;
	OnRep_State(Prev); 
}

void AGvTCrawlerGhost::OnRep_State(EGvTCrawlerGhostState /*PrevState*/)
{
	// Keep lightweight: AnimBP reads State via GetState().
	// Later: play sounds/VFX here.
}

void AGvTCrawlerGhost::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTCrawlerGhost, State);
	DOREPLIFETIME(AGvTCrawlerGhost, TargetVictim);
	DOREPLIFETIME(AGvTCrawlerGhost, ReplicatedSpeed);
}