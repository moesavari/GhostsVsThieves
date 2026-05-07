#include "Gameplay/Ghosts/Ghoul/GvTGhoulGhostCharacter.h"

#include "AIController.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"

AGvTGhoulGhostCharacter::AGvTGhoulGhostCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
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
}

void AGvTGhoulGhostCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTGhoulGhostCharacter, TargetVictim);
	DOREPLIFETIME(AGvTGhoulGhostCharacter, ReplicatedSpeed);
	DOREPLIFETIME(AGvTGhoulGhostCharacter, bIsChasing);
	DOREPLIFETIME(AGvTGhoulGhostCharacter, bIsPerformingScare);
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

	if (!HauntTag.MatchesTagExact(GvTScareTags::GhostHaunt_Chase()))
	{
		return;
	}

	APawn* VictimPawn = Cast<APawn>(Target);
	if (!VictimPawn)
	{
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
	CurrentTarget = Victim;
	bIsChasing = true;
	bIsPerformingScare = false;
	SetHauntState(EGvTHauntGhostState::Chasing);
	RepathTimer = 0.f;
	DirectChaseStuckTime = 0.f;
	ChaseStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	LastDistanceToVictim = FVector::Dist(GetActorLocation(), TargetVictim->GetActorLocation());
	LastChaseProgressTimeSeconds = ChaseStartTimeSeconds;

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
		const EPathFollowingRequestResult::Type MoveResult = AI->MoveToActor(TargetVictim, AcceptRadius, true, true);
		UE_LOG(LogTemp, Warning, TEXT("[Ghoul] Initial MoveToActor Result=%d"), static_cast<int32>(MoveResult));
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

	RepathTimer += DeltaSeconds;
	if (RepathTimer >= RepathInterval)
	{
		RepathTimer = 0.f;
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			const EPathFollowingRequestResult::Type MoveResult = AI->MoveToActor(TargetVictim, AcceptRadius, true, true);
			UE_LOG(LogTemp, Verbose, TEXT("[Ghoul] Repath MoveToActor Result=%d Dist=%.1f"), static_cast<int32>(MoveResult), DistToVictim);
		}
	}

	FaceTargetFlat(TargetVictim, DeltaSeconds, 720.f);

	const float Speed2D = FVector(GetVelocity().X, GetVelocity().Y, 0.f).Size();
	if (bUseDirectChaseFallback && DistToVictim > AcceptRadius + 50.f && Speed2D <= DirectChaseMinSpeed)
	{
		DirectChaseStuckTime += DeltaSeconds;
		if (DirectChaseStuckTime >= DirectChaseFallbackForceDelay)
		{
			FVector Dir = TargetVictim->GetActorLocation() - GetActorLocation();
			Dir.Z = 0.f;
			Dir.Normalize();

			if (!Dir.IsNearlyZero())
			{
				AddMovementInput(Dir, 1.f);

				// Fallback is intentionally movement-only, not animation/mesh tuning.
				// If nav/path following accepts but the pawn produces no velocity, slide the
				// Character toward the victim with sweep so collision still has a say.
				FHitResult Hit;
				const FVector Step = Dir * MaxSpeed * DeltaSeconds;
				AddActorWorldOffset(Step, true, &Hit, ETeleportType::None);
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

	UE_LOG(LogTemp, Warning, TEXT("[Ghoul] ChaseTick Victim=%s Dist=%.1f Vel=%.1f Actual=%.1f Stuck=%.2f ProgressAge=%.2f"),
		*GetNameSafe(TargetVictim),
		PostMoveDist,
		Speed2D,
		ActualMoveSpeed2D,
		DirectChaseStuckTime,
		NowSeconds - LastChaseProgressTimeSeconds);

	TryCatchVictim();
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

		StopAndVanish();
	}
}

void AGvTGhoulGhostCharacter::StopAndVanish()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsChasing = false;
	bIsPerformingScare = false;
	SetHauntState(EGvTHauntGhostState::Idle);
	TargetVictim = nullptr;
	CurrentTarget = nullptr;
	DirectChaseStuckTime = 0.f;
	LastDistanceToVictim = 0.f;
	LastChaseProgressTimeSeconds = 0.f;

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}

	StopCurrentScareAudio(true, &ChaseAudio, true);
	Destroy();
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
