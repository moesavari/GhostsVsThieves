#include "Gameplay/Ghosts/Crawler/GvTCrawlerGhostCharacter.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Systems/Audio/GvTAmbientAudioDirector.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "GameFramework/Character.h"

namespace
{
	static AGvTAmbientAudioDirector* FindAmbientAudioDirector_Crawler(const UObject* WorldContextObject)
	{
		if (!WorldContextObject)
		{
			return nullptr;
		}

		return Cast<AGvTAmbientAudioDirector>(
			UGameplayStatics::GetActorOfClass(WorldContextObject, AGvTAmbientAudioDirector::StaticClass()));
	}
}

AGvTCrawlerGhostCharacter::AGvTCrawlerGhostCharacter()
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

void AGvTCrawlerGhostCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = MaxSpeed;
		Move->MaxAcceleration = Acceleration;
		Move->BrakingDecelerationWalking = BrakingDecel;
	}
}

void AGvTCrawlerGhostCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bOverheadActive)
	{
		if (HasAuthority() || bLocalOverheadOnly)
		{
			OverheadTick(DeltaSeconds);
		}
		return;
	}

	const FVector Vel2D(GetVelocity().X, GetVelocity().Y, 0.f);
	ReplicatedSpeed = Vel2D.Size();

	if (!HasAuthority())
	{
		return;
	}

	if (HauntState == EGvTHauntGhostState::Chasing || HauntState == EGvTHauntGhostState::Searching)
	{
		SetState(EGvTCrawlerGhostState::HauntChase);
	}
	else if (HauntState == EGvTHauntGhostState::Despawning)
	{
		SetState(EGvTCrawlerGhostState::Vanish);
	}
	else
	{
		SetState(EGvTCrawlerGhostState::IdleCeiling);
	}
}

void AGvTCrawlerGhostCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTCrawlerGhostCharacter, TargetVictim);
	DOREPLIFETIME(AGvTCrawlerGhostCharacter, State);
	DOREPLIFETIME(AGvTCrawlerGhostCharacter, ReplicatedSpeed);
}

void AGvTCrawlerGhostCharacter::OnRep_State()
{
}

void AGvTCrawlerGhostCharacter::SetState(EGvTCrawlerGhostState NewState)
{
	if (State == NewState)
	{
		return;
	}

	State = NewState;
	ForceNetUpdate();
	OnRep_State();
}

void AGvTCrawlerGhostCharacter::BeginGhostScare(AActor* Target, FGameplayTag ScareTag)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[CrawlerGhost] BeginGhostScare Target=%s Tag=%s"),
		*GetNameSafe(Target),
		*ScareTag.ToString());

	ACharacter* TargetCharacter = Cast<ACharacter>(Target);
	if (!TargetCharacter)
	{
		return;
	}

	if (ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Close()))
	{
		StartOverhead_Internal(TargetCharacter);
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[CrawlerGhost] Unsupported GhostScare tag: %s"),
		*ScareTag.ToString());
}

void AGvTCrawlerGhostCharacter::BeginGhostHaunt(AActor* Target, FGameplayTag HauntTag)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[CrawlerGhost] BeginGhostHaunt Target=%s Tag=%s"),
		*GetNameSafe(Target),
		*HauntTag.ToString());

	Super::BeginGhostHaunt(Target, HauntTag);
}

void AGvTCrawlerGhostCharacter::Server_StartChase_Implementation(APawn* Victim)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	StartGhostChase(Victim);
}

void AGvTCrawlerGhostCharacter::Server_StartOverheadScare_Implementation(APawn* Victim, bool /*bVictimOnly*/)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	StartOverhead_Internal(Victim);
}

void AGvTCrawlerGhostCharacter::StartLocalOverheadScare(APawn* Victim)
{
	if (!Victim)
	{
		return;
	}

	bLocalOverheadOnly = true;
	bDisableOverheadVictimTracking = true;
	bOverheadAutoStartChase = false;
	bOverheadAutoVanish = true;

	SetReplicates(false);
	SetReplicateMovement(false);

	StartOverhead_Internal(Victim);
}

void AGvTCrawlerGhostCharacter::StopAndDie()
{
	if (!HasAuthority())
	{
		return;
	}

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}

	StopCurrentScareAudio(true, &ChaseAudio, true);

	if (AGvTAmbientAudioDirector* AmbientDirector = FindAmbientAudioDirector_Crawler(this))
	{
		AmbientDirector->HandleScareEnded(GvTScareTags::CrawlerChase(), GetActorLocation(), 1.0f);
	}

	StartHauntDespawnSequence();
}

void AGvTCrawlerGhostCharacter::OnHauntChaseStarted(AActor* Target)
{
	if (!HasAuthority())
	{
		return;
	}

	TargetVictim = Cast<APawn>(Target);
	SetState(EGvTCrawlerGhostState::HauntChase);
	CatchDistance = KillDistance;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = MaxSpeed;
		Move->MaxAcceleration = Acceleration;
		Move->BrakingDecelerationWalking = BrakingDecel;
	}

	PlayScareAudioStart(ChaseAudio, true);
	StartScareAudioSustain(ChaseAudio, true);
}

void AGvTCrawlerGhostCharacter::OnHauntChaseEnded()
{
	StopCurrentScareAudio(true, &ChaseAudio, true);
	TargetVictim = nullptr;
	SetState(EGvTCrawlerGhostState::IdleCeiling);
}

void AGvTCrawlerGhostCharacter::OnHauntSearchStarted()
{
	SetState(EGvTCrawlerGhostState::HauntChase);
}

void AGvTCrawlerGhostCharacter::OnHauntTargetCaught(AActor* Target)
{
	TargetVictim = Cast<APawn>(Target);
	StopCurrentScareAudio(true, &ChaseAudio, true);
}

bool AGvTCrawlerGhostCharacter::GetVictimCamera(FVector& OutCamLoc, FRotator& OutCamRot) const
{
	APawn* VictimPawn = Cast<APawn>(TargetVictim);
	if (!VictimPawn)
	{
		return false;
	}

	APlayerController* PC = Cast<APlayerController>(VictimPawn->GetController());
	if (!PC)
	{
		return false;
	}

	if (PC->PlayerCameraManager)
	{
		OutCamLoc = PC->PlayerCameraManager->GetCameraLocation();
		OutCamRot = PC->PlayerCameraManager->GetCameraRotation();
		return true;
	}

	PC->GetPlayerViewPoint(OutCamLoc, OutCamRot);
	return true;
}

void AGvTCrawlerGhostCharacter::StartOverhead_Internal(APawn* Victim)
{
	TargetVictim = Victim;

	FVector CamLoc;
	FRotator CamRot;
	if (!GetVictimCamera(CamLoc, CamRot))
	{
		return;
	}

	SetGhostPresenceActive(true);

	if (AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(Victim))
	{
		Thief->ApplyScareStun(OverheadDuration);
	}

	SetState(EGvTCrawlerGhostState::OverheadScare);

	// Trace upward to find a ceiling above the player's camera.
	FVector CeilingPos = CamLoc + FVector(0.f, 0.f, OverheadTraceUpDistance);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_CrawlerOverheadTrace), false, this);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Victim);

	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(
		Hit,
		CamLoc,
		CamLoc + FVector(0.f, 0.f, OverheadTraceUpDistance),
		ECC_Visibility,
		Params))
	{
		CeilingPos = Hit.ImpactPoint - FVector(0.f, 0.f, OverheadCeilingClearance);
	}

	// Camera-relative offsets for face-to-face framing.
	CeilingPos += CamRot.Vector() * OverheadForwardOffset;
	CeilingPos += CamRot.Vector() * OverheadFaceForward;
	CeilingPos.Z -= OverheadFaceDown;

	SetActorLocation(CeilingPos, false);

	FRotator NewRot = (CamLoc - CeilingPos).Rotation();
	NewRot.Pitch = OverheadPitchStartDeg;
	NewRot.Roll = 180.f;
	SetActorRotation(NewRot);

	// Freeze movement during overhead.
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	OverheadStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	bOverheadActive = true;
	PlayScareAudioStart(OverheadAudio, true);
	StartScareAudioSustain(OverheadAudio, true);

	SetState(EGvTCrawlerGhostState::OverheadScare);
}

void AGvTCrawlerGhostCharacter::OverheadTick(float DeltaSeconds)
{
	FVector CamLoc;
	FRotator CamRot;
	if (!GetVictimCamera(CamLoc, CamRot))
	{
		EndOverhead();
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float Alpha = OverheadDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp((Now - OverheadStartTime) / OverheadDuration, 0.f, 1.f)
		: 1.f;

	FRotator Desired = (CamLoc - GetActorLocation()).Rotation();
	Desired.Pitch = FMath::Lerp(OverheadPitchStartDeg, OverheadPitchEndDeg, Alpha);
	Desired.Roll = 180.f;

	FRotator Current = GetActorRotation();
	const float YawStep = OverheadTurnSpeedDegPerSec * DeltaSeconds;
	Current.Yaw = FMath::FixedTurn(Current.Yaw, Desired.Yaw, YawStep);
	Current.Pitch = Desired.Pitch;
	Current.Roll = Desired.Roll;
	SetActorRotation(Current);

	SetState(EGvTCrawlerGhostState::OverheadScare);

	if (Alpha >= 1.f)
	{
		EndOverhead();
	}
}

void AGvTCrawlerGhostCharacter::EndOverhead()
{
	bOverheadActive = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(EMovementMode::MOVE_Walking);
	}

	APawn* VictimPawn = Cast<APawn>(TargetVictim);

	if (bLocalOverheadOnly)
	{
		Destroy();
		return;
	}

	if (bOverheadAutoStartChase && VictimPawn)
	{
		Server_StartChase(VictimPawn);
		return;
	}

	if (bOverheadAutoVanish)
	{
		Destroy();
		return;
	}

	SetState(IsValid(TargetVictim) ? EGvTCrawlerGhostState::HauntChase : EGvTCrawlerGhostState::IdleCeiling);

	if (HasAuthority())
	{
		if (AGvTAmbientAudioDirector* AmbientDirector = FindAmbientAudioDirector_Crawler(this))
		{
			AmbientDirector->HandleScareEnded(GvTScareTags::CrawlerOverhead(), GetActorLocation(), 1.0f);
		}
	}

	StopCurrentScareAudio(true, &OverheadAudio, true);
}

void AGvTCrawlerGhostCharacter::PlayScareAudioStart(const FGvTScareAudioSet& AudioSet, bool bAttachToActor)
{
	if (!AudioSet.StartSfx) return;

	if (bAttachToActor)
	{
		UGameplayStatics::SpawnSoundAttached(
			AudioSet.StartSfx,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset,
			false,
			AudioSet.VolumeMultiplier,
			AudioSet.PitchMultiplier
		);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			AudioSet.StartSfx,
			GetActorLocation(),
			AudioSet.VolumeMultiplier,
			AudioSet.PitchMultiplier
		);
	}
}

void AGvTCrawlerGhostCharacter::StartScareAudioSustain(const FGvTScareAudioSet& AudioSet, bool bAttachToActor)
{
	if (!AudioSet.SustainLoopSfx) return;

	if (ActiveSustainAudio)
	{
		ActiveSustainAudio->Stop();
		ActiveSustainAudio = nullptr;
	}

	if (AudioSet.bSustainIs2D)
	{
		ActiveSustainAudio = UGameplayStatics::SpawnSound2D(
			this,
			AudioSet.SustainLoopSfx,
			AudioSet.VolumeMultiplier,
			AudioSet.PitchMultiplier
		);
	}
	else if (bAttachToActor)
	{
		ActiveSustainAudio = UGameplayStatics::SpawnSoundAttached(
			AudioSet.SustainLoopSfx,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset,
			true,
			AudioSet.VolumeMultiplier,
			AudioSet.PitchMultiplier
		);
	}
	else
	{
		ActiveSustainAudio = UGameplayStatics::SpawnSoundAtLocation(
			this,
			AudioSet.SustainLoopSfx,
			GetActorLocation(),
			GetActorRotation(),
			AudioSet.VolumeMultiplier,
			AudioSet.PitchMultiplier,
			0.f,
			nullptr,
			nullptr,
			true
		);
	}
}

void AGvTCrawlerGhostCharacter::StopScareAudioSustain(const FGvTScareAudioSet& AudioSet)
{
	if (!ActiveSustainAudio) return;

	if (AudioSet.SustainFadeOutSeconds > 0.f)
	{
		ActiveSustainAudio->FadeOut(AudioSet.SustainFadeOutSeconds, 0.f);
	}
	else
	{
		ActiveSustainAudio->Stop();
	}

	ActiveSustainAudio = nullptr;
}

void AGvTCrawlerGhostCharacter::PlayScareAudioEnd(const FGvTScareAudioSet& AudioSet, bool bAttachToActor)
{
	if (!AudioSet.EndSfx) return;

	if (bAttachToActor)
	{
		UGameplayStatics::SpawnSoundAttached(
			AudioSet.EndSfx,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset,
			false,
			AudioSet.VolumeMultiplier,
			AudioSet.PitchMultiplier
		);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			AudioSet.EndSfx,
			GetActorLocation(),
			AudioSet.VolumeMultiplier,
			AudioSet.PitchMultiplier
		);
	}
}

void AGvTCrawlerGhostCharacter::StopCurrentScareAudio(bool bPlayEnd, const FGvTScareAudioSet* AudioSet, bool bAttachToActor)
{
	if (!AudioSet) return;

	StopScareAudioSustain(*AudioSet);

	if (bPlayEnd)
	{
		PlayScareAudioEnd(*AudioSet, bAttachToActor);
	}
}

