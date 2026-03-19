#include "Gameplay/Ghosts/Mirror/GvTMirrorActor.h"
#include "Gameplay/Ghosts/Mirror/GvTReflectGhostActor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Systems/Audio/GvTAmbientAudioDirector.h"
#include "Gameplay/Scare/GvTScareTags.h"

namespace
{
	static AGvTAmbientAudioDirector* FindAmbientAudioDirector(const UObject* WorldContextObject)
	{
		if (!WorldContextObject)
		{
			return nullptr;
		}

		return Cast<AGvTAmbientAudioDirector>(
			UGameplayStatics::GetActorOfClass(WorldContextObject, AGvTAmbientAudioDirector::StaticClass()));
	}
}

AGvTMirrorActor::AGvTMirrorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	MirrorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorMesh"));
	SetRootComponent(MirrorMesh);

	MirrorCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("MirrorCapture"));
	MirrorCapture->SetupAttachment(MirrorMesh);

	MirrorCapture->bCaptureEveryFrame = false;
	MirrorCapture->bCaptureOnMovement = false;

	MirrorCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	MirrorCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;

	MirrorCapture->ShowFlags.SetTemporalAA(false);
	MirrorCapture->ShowFlags.SetAntiAliasing(false);
	MirrorCapture->ShowFlags.SetMotionBlur(false);
	MirrorCapture->ShowFlags.SetBloom(false);
	MirrorCapture->PostProcessBlendWeight = 0.0f;

	MirrorSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorSurface"));
	MirrorSurface->SetupAttachment(MirrorMesh);
	MirrorSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MirrorSurface->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		MirrorSurface->SetStaticMesh(PlaneMesh.Object);
	}

	MirrorSurface->bReceivesDecals = false;
	MirrorSurface->SetHiddenInGame(true);
	MirrorSurface->SetVisibility(false, true);
}

void AGvTMirrorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AlignMirrorSurfaceToMesh();
}

bool AGvTMirrorActor::IsLocalClientWorld() const
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		return PC->IsLocalController();
	}

	return (GetNetMode() == NM_Standalone);
}

void AGvTMirrorActor::BeginPlay()
{
	Super::BeginPlay();

	SetScarePlaneVisible(true);

	if (MirrorMesh && MirrorMesh->GetStaticMesh())
	{
		const auto& B = MirrorMesh->GetStaticMesh()->GetBounds();
		UE_LOG(LogTemp, Warning, TEXT("[Mirror] StaticMesh=%s AssetBounds Origin=%s Extents=%s"),
			*MirrorMesh->GetStaticMesh()->GetName(),
			*B.Origin.ToString(),
			*B.BoxExtent.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mirror] MirrorMesh has NO static mesh assigned"));
	}

	AlignMirrorSurfaceToMesh();
	SetScarePlaneVisible(false);

	if (!IsLocalClientWorld())
	{
		return;
	}

	if (RenderTarget)
	{
		MirrorCapture->TextureTarget = RenderTarget;
		UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTarget, FLinearColor::Black);
	}


	if (MirrorSurface && ScareMirrorMaterial)
	{
		MirrorSurfaceMID = UMaterialInstanceDynamic::Create(ScareMirrorMaterial, this);
		MirrorSurface->SetMaterial(0, MirrorSurfaceMID);

		if (MirrorSurfaceMID && RenderTarget)
		{
			MirrorSurfaceMID->SetTextureParameterValue(MirrorTextureParam, RenderTarget);
		}
	}

	// Cache whatever is currently on the slot as the "normal" fallback
	CachedNormalMat = MirrorMesh ? MirrorMesh->GetMaterial(MirrorMaterialSlot) : nullptr;

	// If user provided an explicit normal material, use it. Otherwise keep cached.
	if (NormalMirrorMaterial)
	{
		ApplyNormalMaterial(NormalMirrorMaterial);
	}
	else if (CachedNormalMat)
	{
		ApplyNormalMaterial(CachedNormalMat);
	}
}

void AGvTMirrorActor::EnsureMID()
{
	if (MirrorMID) 
	{
		return;
	}

	int32 SlotToUse = MirrorMaterialSlot;
	const int32 NumMats = MirrorMesh ? MirrorMesh->GetNumMaterials() : 0;

	if (!MirrorMesh || NumMats <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mirror] MirrorMesh missing or has no materials."));
		return;
	}

	if (SlotToUse < 0 || SlotToUse >= NumMats)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mirror] MirrorMaterialSlot %d invalid (NumMats=%d). Falling back to 0."),
			SlotToUse, NumMats);
		SlotToUse = 0;
	}

	UMaterialInterface* Mat = MirrorMesh->GetMaterial(SlotToUse);
	if (!Mat)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mirror] No material found on slot %d."), SlotToUse);
		return;
	}

	MirrorMID = UMaterialInstanceDynamic::Create(Mat, this);
	MirrorMesh->SetMaterial(SlotToUse, MirrorMID);

	if (RenderTarget)
	{
		MirrorMID->SetTextureParameterValue(MirrorTextureParam, RenderTarget);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mirror] RenderTarget is NULL on %s"), *GetName());
	}
}

void AGvTMirrorActor::ApplyMaterialToSlot(UMaterialInterface* Mat)
{
	if (!MirrorMesh || !Mat) return;

	CachedMID = UMaterialInstanceDynamic::Create(Mat, this);
	MirrorMesh->SetMaterial(MirrorMaterialSlot, CachedMID);

	if (CachedMID && RenderTarget)
	{
		CachedMID->SetTextureParameterValue(MirrorTextureParam, RenderTarget);
	}
}

void AGvTMirrorActor::EnsureGhost()
{
	if (!IsLocalClientWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mirror] EnsureGhost aborted: not local client world."));
		return;
	}

	if (!ReflectGhostClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[Mirror] EnsureGhost aborted: ReflectGhostClass is NULL on %s"), *GetName());
		return;
	}

	if (GhostInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mirror] EnsureGhost skipped: already have GhostInstance=%s"), *GetNameSafe(GhostInstance));
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	GhostInstance = GetWorld()->SpawnActor<AGvTReflectGhostActor>(
		ReflectGhostClass,
		GetActorLocation(),
		GetActorRotation(),
		Params
	);

	if (GhostInstance)
	{
		GhostInstance->StopReflect();
		EnsureGhostConfiguredForCaptureOnly();
		UE_LOG(LogTemp, Warning, TEXT("[Mirror] EnsureGhost spawned %s"), *GetNameSafe(GhostInstance));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Mirror] EnsureGhost failed to spawn ghost on %s"), *GetName());
	}
}

void AGvTMirrorActor::TriggerReflectLocal(float Intensity01, float LifeSeconds)
{
	if (!IsLocalClientWorld())
	{
		return;
	}

	SetScarePlaneVisible(true);

	//if (ScareStartSfx)
	//{
	//	UGameplayStatics::PlaySoundAtLocation(this, ScareStartSfx, GetActorLocation(), ScareSfxVolume, ScareSfxPitch);
	//}

	PlayMirrorStartAudio();
	StartMirrorSustainAudio();

	EnsureGhost();

	if (!GhostInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("[Mirror] TriggerReflectLocal failed: GhostInstance is null after EnsureGhost on %s"), *GetName());

		// Don't leave the player with a cursed black rectangle.
		SetScarePlaneVisible(false);

		if (RenderTarget)
		{
			UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTarget, FLinearColor::Black);
		}
		return;
	}

	GhostInstance->StartReflect(Intensity01, LifeSeconds);

	MirrorCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	MirrorCapture->ClearShowOnlyComponents();
	MirrorCapture->ShowOnlyActorComponents(GhostInstance);

	StartScareCapture(LifeSeconds);

	UE_LOG(LogTemp, Warning, TEXT("[Mirror] TriggerReflectLocal Mirror=%s Intensity=%.2f Life=%.2f LocalWorld=%d"),
		*GetName(), Intensity01, LifeSeconds, IsLocalClientWorld() ? 1 : 0);
}

void AGvTMirrorActor::TriggerScare(float Intensity01, float LifeSeconds)
{
	// Server is authoritative for triggering. Clients render locally.
	if (HasAuthority())
	{
		MulticastTriggerScare(Intensity01, LifeSeconds);
	}
	else
	{
		ServerTriggerScare(Intensity01, LifeSeconds);
	}
}

void AGvTMirrorActor::ServerTriggerScare_Implementation(float Intensity01, float LifeSeconds)
{
	MulticastTriggerScare(Intensity01, LifeSeconds);
}

void AGvTMirrorActor::MulticastTriggerScare_Implementation(float Intensity01, float LifeSeconds)
{
	// Never do rendering work on dedicated server.
	if (!IsLocalClientWorld())
	{
		return;
	}

	TriggerReflectLocal(Intensity01, LifeSeconds);
}


void AGvTMirrorActor::EnsureGhostConfiguredForCaptureOnly()
{
	if (!GhostInstance) return;

	TArray<UPrimitiveComponent*> PrimComps;
	GhostInstance->GetComponents<UPrimitiveComponent>(PrimComps);

	for (UPrimitiveComponent* Prim : PrimComps)
	{
		if (!Prim) continue;

		// Remove world shadow artifacts
		Prim->SetHiddenInGame(false);
		Prim->SetVisibility(true, true);
		Prim->bVisibleInSceneCaptureOnly = true;
		Prim->CastShadow = false;
		Prim->bCastHiddenShadow = false;
	}
}

void AGvTMirrorActor::UpdateGhostForCaptureCamera()
{
	if (!GhostInstance || !MirrorCapture) return;

	const FTransform CT = MirrorCapture->GetComponentTransform();
	const FVector CLoc = CT.GetLocation();
	const FVector Fwd = CT.GetRotation().GetForwardVector();
	const FVector Right = CT.GetRotation().GetRightVector();
	const FVector Up = CT.GetRotation().GetUpVector();

	const FVector GhostPos = CLoc + Fwd * GhostDistanceFromCapture + Right * GhostRightOffset + Up * GhostUpOffset;
	GhostInstance->SetActorLocation(GhostPos);

	if (bGhostFacesCapture)
	{
		// Face the capture camera (upright)
		const FVector ToCam = (CLoc - GhostPos).GetSafeNormal();
		const FRotator GhostRot = FRotationMatrix::MakeFromXZ(ToCam, FVector::UpVector).Rotator();
		GhostInstance->SetActorRotation(GhostRot);
	}
}

void AGvTMirrorActor::StartScareCapture(float LifeSeconds)
{
	if (bScareActive) return;
	bScareActive = true;

	const float Interval = (CaptureFPS <= 0.f) ? 0.066f : (1.f / CaptureFPS);
	GetWorldTimerManager().SetTimer(CaptureTimer, this, &AGvTMirrorActor::CaptureOnce, Interval, true);

	GetWorldTimerManager().ClearTimer(StopTimer);
	if (LifeSeconds > 0.f)
	{
		const float FinalLife = (LifeSeconds <= 0.f) ? 1.0f : LifeSeconds;
		GetWorldTimerManager().SetTimer(StopTimer, this, &AGvTMirrorActor::StopScareCapture, FinalLife, false);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Mirror] StartScareCapture FPS=%.2f RT=%s Ghost=%s"),
		CaptureFPS,
		*GetNameSafe(RenderTarget),
		*GetNameSafe(GhostInstance));
}

void AGvTMirrorActor::StopScareCapture()
{
	bScareActive = false;
	SetScarePlaneVisible(false);
	GetWorldTimerManager().ClearTimer(CaptureTimer);

	if (RenderTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTarget, FLinearColor::Black);
	}

	// Optional: kill ghost after scare
	if (GhostInstance)
	{
		GhostInstance->Destroy();
		GhostInstance = nullptr;
	}

	StopMirrorSustainAudio();
	PlayMirrorEndAudio();

	if (HasAuthority())
	{
		if (AGvTAmbientAudioDirector* AmbientDirector = FindAmbientAudioDirector(this))
		{
			AmbientDirector->HandleScareEnded(GvTScareTags::Mirror(), GetActorLocation(), 1.0f);
		}
	}

	if (MirrorCapture)
	{
		MirrorCapture->ClearShowOnlyComponents();
	}
}

void AGvTMirrorActor::SetScarePlaneVisible(bool bVisible)
{
	if (!MirrorSurface) return;

	MirrorSurface->SetHiddenInGame(!bVisible);
	MirrorSurface->SetVisibility(bVisible, true);

	MirrorSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MirrorSurface->SetCastShadow(false);
}

void AGvTMirrorActor::CaptureOnce()
{
	if (!IsLocalClientWorld()) return;
	if (!MirrorCapture || !RenderTarget) return;

	UpdateGhostForCaptureCamera();
	MirrorCapture->CaptureScene();

	UE_LOG(LogTemp, VeryVerbose, TEXT("[Mirror] CaptureOnce RT=%s Capture=%s"),
		*GetNameSafe(RenderTarget),
		*GetNameSafe(MirrorCapture));
}

void AGvTMirrorActor::ApplyNormalMaterial(UMaterialInterface* Mat)
{
	if (!MirrorMesh || !Mat) return;

	CachedMID = nullptr;
	MirrorMesh->SetMaterial(MirrorMaterialSlot, Mat);
}

void AGvTMirrorActor::ApplyScareMaterial(UMaterialInterface* Mat)
{
	if (!MirrorMesh || !Mat) return;

	CachedMID = UMaterialInstanceDynamic::Create(Mat, this);
	MirrorMesh->SetMaterial(MirrorMaterialSlot, CachedMID);

	if (CachedMID && RenderTarget)
	{
		CachedMID->SetTextureParameterValue(MirrorTextureParam, RenderTarget);
	}
}

void AGvTMirrorActor::AlignMirrorSurfaceToMesh()
{
	if (!MirrorMesh || !MirrorSurface) return;

	UStaticMesh* SM = MirrorMesh->GetStaticMesh();
	if (!SM)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mirror] MirrorMesh has no StaticMesh assigned on %s"), *GetName());
		return;
	}

	const FBoxSphereBounds B = SM->GetBounds();
	const FVector Origin = B.Origin;        
	const FVector Extents = B.BoxExtent;   
	const FVector FullSize = Extents * 2.f;

	// For SM_Mirror, thickness is X (small), face is Y/Z (large)
	const float FaceWidth = FullSize.Y;
	const float FaceHeight = FullSize.Z;
	const float Thickness = FullSize.X;

	const float PitchToPointNormalAlongX = bFlipMirrorNormal ? 90.f : -90.f;

	FRotator SurfaceRot(PitchToPointNormalAlongX, 0.f, 0.f);
	MirrorSurface->SetRelativeRotation(SurfaceRot);

	// Scale plane to match mirror face (Y/Z). After that rotation:
	constexpr float BasePlaneSize = 100.f;
	const float ScaleX = (FaceHeight / BasePlaneSize) * SurfaceScalePadding;
	const float ScaleY = (FaceWidth / BasePlaneSize) * SurfaceScalePadding; 

	MirrorSurface->SetRelativeScale3D(FVector(ScaleX, ScaleY, 1.f));

	// Position the plane on the front face along X:
	const float Sign = bFlipMirrorNormal ? -1.f : 1.f;
	const float XOnFace = Origin.X + Sign * (Extents.X + SurfacePushOut);

	// Keep the plane centered on the mirror face:
	const FVector LocalPos(XOnFace, Origin.Y, Origin.Z);
	MirrorSurface->SetRelativeLocation(LocalPos);

	MirrorSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MirrorSurface->SetCastShadow(false);

	UE_LOG(LogTemp, Warning, TEXT("[Mirror] Align using AssetBounds OK. LocalPos=%s Scale=%s Rot=%s"),
		*LocalPos.ToString(),
		*MirrorSurface->GetRelativeScale3D().ToString(),
		*MirrorSurface->GetRelativeRotation().ToString());
}

void AGvTMirrorActor::PlayMirrorStartAudio()
{
	if (!MirrorAudio.StartSfx) return;

	UGameplayStatics::PlaySoundAtLocation(
		this,
		MirrorAudio.StartSfx,
		GetActorLocation(),
		MirrorAudio.VolumeMultiplier,
		MirrorAudio.PitchMultiplier
	);
}

void AGvTMirrorActor::StartMirrorSustainAudio()
{
	if (!MirrorAudio.SustainLoopSfx) return;

	if (ActiveMirrorSustainAudio)
	{
		ActiveMirrorSustainAudio->Stop();
		ActiveMirrorSustainAudio = nullptr;
	}

	ActiveMirrorSustainAudio = UGameplayStatics::SpawnSoundAttached(
		MirrorAudio.SustainLoopSfx,
		GetRootComponent(),
		NAME_None,
		FVector::ZeroVector,
		EAttachLocation::KeepRelativeOffset,
		true,
		MirrorAudio.VolumeMultiplier,
		MirrorAudio.PitchMultiplier
	);
}

void AGvTMirrorActor::StopMirrorSustainAudio()
{
	if (!ActiveMirrorSustainAudio) return;

	if (MirrorAudio.SustainFadeOutSeconds > 0.f)
	{
		ActiveMirrorSustainAudio->FadeOut(MirrorAudio.SustainFadeOutSeconds, 0.f);
	}
	else
	{
		ActiveMirrorSustainAudio->Stop();
	}

	ActiveMirrorSustainAudio = nullptr;
}

void AGvTMirrorActor::PlayMirrorEndAudio()
{
	if (!MirrorAudio.EndSfx) return;

	UGameplayStatics::PlaySoundAtLocation(
		this,
		MirrorAudio.EndSfx,
		GetActorLocation(),
		MirrorAudio.VolumeMultiplier,
		MirrorAudio.PitchMultiplier
	);
}
