#include "Gameplay/Ghosts/Mirror/GvTReflectGhostActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/ArrowComponent.h"

AGvTReflectGhostActor::AGvTReflectGhostActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;

	GetCapsuleComponent()->SetVisibility(false, true);
	GetCapsuleComponent()->SetHiddenInGame(true);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->bHiddenInSceneCapture = true;

	if (GetMesh())
	{
		GetMesh()->SetVisibility(false, true);
		GetMesh()->SetHiddenInGame(true);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetMesh()->bHiddenInSceneCapture = true;
	}

	if (UArrowComponent* Arrow = GetArrowComponent())
	{
		Arrow->SetVisibility(false, true);
		Arrow->SetHiddenInGame(true);
		Arrow->bHiddenInSceneCapture = true;
	}

	ReflectMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReflectMesh"));
	ReflectMesh->SetupAttachment(GetRootComponent());
	ReflectMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReflectMesh->SetGenerateOverlapEvents(false);
}

void AGvTReflectGhostActor::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInterface* Mat = ReflectMesh->GetMaterial(0))
	{
		MID = UMaterialInstanceDynamic::Create(Mat, this);
		ReflectMesh->SetMaterial(0, MID);
	}

	StopReflect();
}

void AGvTReflectGhostActor::StartReflect(float InIntensity01, float InLifeSeconds)
{
	StartEventPresentation(InIntensity01, InLifeSeconds);
}

void AGvTReflectGhostActor::StopReflect()
{
	StopEventPresentation();
}

void AGvTReflectGhostActor::StartEventPresentation(float InIntensity01, float InLifeSeconds)
{
	Super::StartEventPresentation(InIntensity01, InLifeSeconds);

	Intensity01 = FMath::Clamp(InIntensity01, 0.0f, 1.0f);

	LifeSeconds = FMath::Max(InLifeSeconds, 0.01f);

	Elapsed = 0.0f;
	bActive = true;

	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[ReflectGhost] StartPresentation Ghost=%s Intensity=%.2f Life=%.2f"),
		*GetNameSafe(this),
		Intensity01,
		LifeSeconds);
}

void AGvTReflectGhostActor::StopEventPresentation()
{
	bActive = false;
	Intensity01 = 0.0f;
	LifeSeconds = 0.0f;
	Elapsed = 0.0f;

	if (MID)
	{
		MID->SetScalarParameterValue(OpacityParam, 0.0f);

		MID->SetScalarParameterValue(EmissiveParam, 0.0f);
	}

	Super::StopEventPresentation();
}

void AGvTReflectGhostActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bActive)
	{
		return;
	}

	Elapsed += DeltaSeconds;
	const float T = FMath::Clamp(Elapsed / LifeSeconds, 0.f, 1.f);

	// Fade in quickly, fade out slowly
	const float FadeIn = FMath::SmoothStep(0.f, 0.15f, T);
	const float FadeOut = 1.f - FMath::SmoothStep(0.65f, 1.f, T);
	float Alpha = FadeIn * FadeOut;

	if (bEnableFlicker)
	{
		const float Flicker = 0.75f + 0.25f * FMath::Sin(Elapsed * FlickerSpeed);
		Alpha *= Flicker;
	}

	const float Opacity = FMath::Lerp(MinOpacity, MaxOpacity, Intensity01) * Alpha;
	const float Emissive = FMath::Lerp(MinEmissive, MaxEmissive, Intensity01) * Alpha;

	if (MID)
	{
		MID->SetScalarParameterValue(OpacityParam, Opacity);
		MID->SetScalarParameterValue(EmissiveParam, Emissive);
	}

	if (T >= 1.0f)
	{
		StopEventPresentation();
	}
}

void AGvTReflectGhostActor::BeginGhostEvent(AActor* Target, AActor* SourceActor, FGameplayTag EventTag)
{
	Super::BeginGhostEvent(Target, SourceActor, EventTag);

	UE_LOG(LogTemp, Log, 
		TEXT("[ReflectGhost] BeginGhostEvent Ghost=%s Target=%s Source=%s Tag=%s"), 
		*GetNameSafe(this), 
		*GetNameSafe(Target), 
		*GetNameSafe(SourceActor), 
		*EventTag.ToString());
}