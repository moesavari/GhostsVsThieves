#include "Mirror/GvTReflectGhostActor.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AGvTReflectGhostActor::AGvTReflectGhostActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
}

void AGvTReflectGhostActor::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInterface* Mat = Mesh->GetMaterial(0))
	{
		MID = UMaterialInstanceDynamic::Create(Mat, this);
		Mesh->SetMaterial(0, MID);
	}

	StopReflect();
}

void AGvTReflectGhostActor::StartReflect(float InIntensity01, float InLifeSeconds)
{
	Intensity01 = FMath::Clamp(InIntensity01, 0.f, 1.f);
	LifeSeconds = FMath::Max(0.01f, InLifeSeconds);
	Elapsed = 0.f;
	bActive = true;

	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
}

void AGvTReflectGhostActor::StopReflect()
{
	bActive = false;
	Intensity01 = 0.f;
	LifeSeconds = 0.f;
	Elapsed = 0.f;

	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
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

	if (T >= 1.f)
	{
		StopReflect();
	}
}
