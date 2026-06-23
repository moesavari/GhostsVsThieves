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

	bAlwaysRelevant = true;
	bOnlyRelevantToOwner = false;
	NetDormancy = DORM_Never;
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

	bIsChasing = HauntState == EGvTHauntGhostState::Chasing;
	bIsSearching = HauntState == EGvTHauntGhostState::Searching;

	if (!HasAuthority())
	{
		return;
	}

	if (bIsChasing || bIsSearching)
	{
		DoorProbeTimer += DeltaSeconds;
		if (DoorProbeTimer >= DoorProbeInterval)
		{
			DoorProbeTimer = 0.f;
			TryOpenNearbyDoors();
		}
	}
}

void AGvTGhoulGhostCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTGhoulGhostCharacter, TargetVictim);
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
	Super::BeginHauntAction(Target, HauntTag);
}

void AGvTGhoulGhostCharacter::Server_StartChase_Implementation(APawn* Victim)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	StartGhostChase(Victim);
}

void AGvTGhoulGhostCharacter::OnHauntChaseStarted(AActor* Target)
{
	if (!HasAuthority())
	{
		return;
	}

	TargetVictim = Cast<APawn>(Target);
	bIsChasing = true;
	bIsSearching = false;
	bIsPerformingScare = false;
	DoorProbeTimer = 0.f;
	CatchDistance = GhoulCatchDistance;

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
}

void AGvTGhoulGhostCharacter::OnHauntChaseEnded()
{
	StopCurrentScareAudio(true, &ChaseAudio, true);
	TargetVictim = nullptr;
	bIsChasing = false;
	bIsSearching = false;
}

void AGvTGhoulGhostCharacter::OnHauntSearchStarted()
{
	bIsChasing = false;
	bIsSearching = true;
}

void AGvTGhoulGhostCharacter::OnHauntTargetCaught(AActor* Target)
{
	TargetVictim = Cast<APawn>(Target);
	StopCurrentScareAudio(true, &ChaseAudio, true);
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
	bIsSearching = false;
	bIsPerformingScare = true;
	SetHauntState(EGvTHauntGhostState::PerformingScare);
	TargetVictim = Cast<APawn>(Target);

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
}

void AGvTGhoulGhostCharacter::EndCloseScarePresentation()
{
	bIsPerformingScare = false;
	SetHauntState(EGvTHauntGhostState::Idle);
	TargetVictim = nullptr;
	StopCurrentScareAudio(false, nullptr, true);
	Destroy();
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
