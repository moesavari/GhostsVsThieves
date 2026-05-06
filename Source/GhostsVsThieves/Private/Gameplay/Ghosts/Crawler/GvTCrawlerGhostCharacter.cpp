#include "Gameplay/Ghosts/Crawler/GvTCrawlerGhostCharacter.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "AIController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Components/CapsuleComponent.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "DrawDebugHelpers.h"
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

	if (!HasAuthority())
	{
		return;
	}

	SetState(IsValid(TargetVictim) ? EGvTCrawlerGhostState::HauntChase : EGvTCrawlerGhostState::IdleCeiling);

	const FVector Vel2D(GetVelocity().X, GetVelocity().Y, 0.f);
	ReplicatedSpeed = Vel2D.Size();
	ChaseTick(DeltaSeconds);
}

void AGvTCrawlerGhostCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTCrawlerGhostCharacter, TargetVictim);
	DOREPLIFETIME(AGvTCrawlerGhostCharacter, State);
}

void AGvTCrawlerGhostCharacter::OnRep_State()
{
	// Keeping this function for debugging + future hooks.
}

void AGvTCrawlerGhostCharacter::SetState(EGvTCrawlerGhostState NewState)
{
	if (State == NewState) return;

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
		UE_LOG(LogTemp, Warning, TEXT("[CrawlerGhost] Starting crawler close scare presentation."));

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

void AGvTCrawlerGhostCharacter::Server_StartChase_Implementation(APawn* Victim)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	SetGhostPresenceActive(true);

	TargetVictim = Victim;
	SetState(EGvTCrawlerGhostState::HauntChase);

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

	if (bSnapToGroundInChase)
	{
		SnapToGround();
	}

	PlayScareAudioStart(ChaseAudio, true);
	StartScareAudioSustain(ChaseAudio, true);

	RepathTimer = 0.f;

	if (bUseDragStepMovement)
	{
		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			AIC->StopMovement();
		}
	}
	else
	{
		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			AIC->MoveToActor(TargetVictim, AcceptRadius, true, true);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Crawler] StartChase Ghost=%s Victim=%s Controller=%s Loc=%s"),
		*GetName(),
		*GetNameSafe(Victim),
		*GetNameSafe(GetController()),
		*GetActorLocation().ToString());
}

void AGvTCrawlerGhostCharacter::Server_StartOverheadScare_Implementation(APawn* Victim, bool /*bVictimOnly*/)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	StartOverhead_Internal(Victim);
}

void AGvTCrawlerGhostCharacter::Server_DragStep_Implementation()
{
	ApplyDragStep();
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

	Destroy();
}

void AGvTCrawlerGhostCharacter::OnCrawlerDragStep()
{
	if (!HasAuthority() || !bUseDragStepMovement) return;
	if (!GetCharacterMovement()) return;
	if (DragStepDistance <= 0.f || DragStepDuration <= KINDA_SMALL_NUMBER) return;
	if (!IsValid(TargetVictim)) return;

	FVector Dir = TargetVictim->GetActorLocation() - GetActorLocation();
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();

	if (Dir.IsNearlyZero())
	{
		return;
	}

	const FVector Start = GetActorLocation();
	const FVector Target = Start + Dir * DragStepDistance * DragStepForwardBias;

	TSharedPtr<FRootMotionSource_MoveToForce> RMS = MakeShared<FRootMotionSource_MoveToForce>();
	RMS->InstanceName = FName("CrawlerDragStep");
	RMS->AccumulateMode = ERootMotionAccumulateMode::Override;
	RMS->Priority = 200;
	RMS->Duration = DragStepDuration;
	RMS->StartLocation = Start;
	RMS->TargetLocation = Target;

	GetCharacterMovement()->ApplyRootMotionSource(RMS);

	if (bSnapToGroundInChase)
	{
		SnapToGround();
	}
}

void AGvTCrawlerGhostCharacter::ChaseTick(float DeltaSeconds)
{
	if (!IsValid(TargetVictim))
	{
		return;
	}

	const FVector MyLoc = GetActorLocation();
	const FVector VictimLoc = TargetVictim->GetActorLocation();
	const float DistToVictim = FVector::Dist(MyLoc, VictimLoc);

	if (MaxChaseDistance > 0.f && DistToVictim > MaxChaseDistance)
	{
		StopAndDie();
		return;
	}

	FVector ToVictim = VictimLoc - MyLoc;
	ToVictim.Z = 0.f;

	if (!ToVictim.IsNearlyZero())
	{
		const FRotator WantRot = ToVictim.Rotation();
		const FRotator NewRot = FMath::RInterpConstantTo(GetActorRotation(), WantRot, DeltaSeconds, 720.f);
		SetActorRotation(NewRot);
	}

	if (!bUseDragStepMovement)
	{
		RepathTimer += DeltaSeconds;
		if (RepathTimer >= RepathInterval)
		{
			RepathTimer = 0.f;

			if (AAIController* AIC = Cast<AAIController>(GetController()))
			{
				AIC->MoveToActor(TargetVictim, AcceptRadius, true, true);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Crawler] Tick State=%d Victim=%s Controller=%s Vel=%s Dist=%.1f"),
		(int32)State,
		*GetNameSafe(TargetVictim),
		*GetNameSafe(GetController()),
		*GetVelocity().ToString(),
		DistToVictim);

	TryKillVictim();
}

void AGvTCrawlerGhostCharacter::TryKillVictim()
{
	if (!HasAuthority() || bLocalOverheadOnly || State != EGvTCrawlerGhostState::HauntChase)
	{
		return;
	}

	if (!IsValid(TargetVictim))
	{
		return;
	}

	if (KillDistance <= 0.f)
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), TargetVictim->GetActorLocation()) <= KillDistance)
	{
		if (AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(TargetVictim))
		{
			Thief->Server_SetDead(this);
		}

		StopAndDie();
	}
}

void AGvTCrawlerGhostCharacter::ApplyDragStep()
{
	if (!GetCharacterMovement() || DragStepDistance <= 1.f || DragStepDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Choose direction:
	FVector Dir = GetActorForwardVector();

	if (bDragStepUseVelocityDir)
	{
		const FVector Vel2D = FVector(GetVelocity().X, GetVelocity().Y, 0.f);
		if (!Vel2D.IsNearlyZero())
		{
			Dir = Vel2D.GetSafeNormal();
		}
	}

	const FVector Start = GetActorLocation();
	const FVector Target = Start + Dir * DragStepDistance * DragStepForwardBias;

	// RootMotionSource: MoveToForce = smooth movement with collision/replication.
	TSharedPtr<FRootMotionSource_MoveToForce> RMS = MakeShared<FRootMotionSource_MoveToForce>();
	RMS->InstanceName = FName("CrawlerDragStep");
	RMS->AccumulateMode = ERootMotionAccumulateMode::Override;
	RMS->Priority = 200; // high priority so it actually pulls
	RMS->Duration = DragStepDuration;
	RMS->StartLocation = Start;
	RMS->TargetLocation = Target;

	GetCharacterMovement()->ApplyRootMotionSource(RMS);
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

void AGvTCrawlerGhostCharacter::SnapToGround()
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrawlerGroundSnap), false, this);
	Params.AddIgnoredActor(this);

	const FVector Start = GetActorLocation() + FVector(0, 0, GroundTraceUp);
	const FVector End = GetActorLocation() - FVector(0, 0, GroundTraceDown);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		FVector NewLoc = GetActorLocation();
		NewLoc.Z = Hit.Location.Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		SetActorLocation(NewLoc, false, nullptr, ETeleportType::None);
	}
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
