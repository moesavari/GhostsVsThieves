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

	// Skeletal meshes don't replicate; the actor does.
	Mesh->SetIsReplicated(false);
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

	// Track movement for ReplicatedSpeed (computed AFTER state update so anim matches what happened this frame).
	const FVector StartLocThisTick = GetActorLocation();

	switch (State)
	{
	case EGvTCrawlerGhostState::DropScare:
	{
		DropT += DeltaSeconds;
		const float Alpha = FMath::Clamp(DropT / FMath::Max(0.01f, DropDuration), 0.f, 1.f);

		const FVector NewLoc = FMath::Lerp(DropStart, DropEnd, Alpha);
		SetActorLocation(NewLoc, false);

		if (Alpha >= 1.f)
		{
			Vanish_Internal();
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

		// Face the victim
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator WantRot = ToTarget.Rotation();
			const FRotator NewRot = FMath::RInterpConstantTo(GetActorRotation(), WantRot, DeltaSeconds, TurnSpeedDegPerSec);
			SetActorRotation(NewRot);
		}

		// Move
		if (bUseLungeMovement)
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

				// Time-window for the burst
				if (LungeTimer >= LungeDuration)
				{
					bIsLunging = false;
				}
			}
		}
		else
		{
			// Smooth continuous crawl
			const FVector Dir = ToTarget.GetSafeNormal();
			const FVector Step = Dir * (CrawlSpeed * DeltaSeconds);
			SetActorLocation(MyLoc + Step, true);
		}

		TryKillVictim();
		break;
	}

	case EGvTCrawlerGhostState::IdleCeiling:
	case EGvTCrawlerGhostState::Vanish:
	default:
		break;
	}

	// Compute replicated speed from actual movement this tick.
	const FVector Now = GetActorLocation();
	const float Dist2D = FVector::Dist2D(Now, StartLocThisTick);
	ReplicatedSpeed = (DeltaSeconds > KINDA_SMALL_NUMBER) ? (Dist2D / DeltaSeconds) : 0.f;
	PrevLoc = Now;
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

void AGvTCrawlerGhost::StartDropScare(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly)
{
	if (!Victim)
	{
		return;
	}

	if (HasAuthority())
	{
		StartDropScare_Internal(Victim, CeilingWorldPos, FaceRot, bVictimOnly);
	}
	else
	{
		Server_StartDropScare(Victim, CeilingWorldPos, FaceRot, bVictimOnly);
	}
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

	SetActorRotation(FaceRot);

	DropStart = CeilingWorldPos;
	DropEnd = CeilingWorldPos - FVector(0.f, 0.f, DropOffsetDown);
	DropT = 0.f;

	SetActorLocation(DropStart, false);

	EnterState(EGvTCrawlerGhostState::DropScare);
}

void AGvTCrawlerGhost::StartHauntChase_Internal(AActor* Victim)
{
	TargetVictim = Victim;

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
