#include "World/Items/GvTInteractableItem.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Systems/Noise/GvTNoiseEmitterComponent.h"
#include "GvTPlayerState.h"
#include "GvTPlayerController.h"
#include "Systems/Director/GvTDirectorSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Gameplay/Inventory/GvTInventoryComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/Noise/GvTNoiseSubsystem.h"

AGvTInteractableItem::AGvTInteractableItem()
{
	bReplicates = true;
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->OnComponentHit.AddDynamic(this, &AGvTInteractableItem::HandleMeshHit);

	InteractNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Interact"));
	PhotoNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Photo"));
	ScanNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Scan"));
	DropNoiseTag = InteractNoiseTag;
}

void AGvTInteractableItem::BeginPlay()
{
	Super::BeginPlay();

	if (Mesh)
	{
		// Loot must never block or physically push player/ghost character capsules.
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	if (!HasAuthority())
	{
		return;
	}

	if (MeshVariants.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, MeshVariants.Num() - 1);

		SelectedMesh = MeshVariants[Index];

		if (SelectedMesh)
		{
			Mesh->SetStaticMesh(SelectedMesh);
		}
	}

	if (bSnapToSurfaceOnBeginPlay)
	{
		SnapMeshToSurface();
	}
}

void AGvTInteractableItem::SnapMeshToSurface()
{
	if (!HasAuthority() || !Mesh || !Mesh->GetStaticMesh() || !GetWorld())
	{
		return;
	}

	const FBoxSphereBounds WorldBounds = Mesh->Bounds;
	const FVector TraceDirection = PlacementMode == EGvTItemPlacementMode::Wall
		? -GetActorForwardVector().GetSafeNormal()
		: FVector::DownVector;
	const FVector TraceStart = WorldBounds.Origin - (TraceDirection * 5.f);
	const FVector TraceEnd = TraceStart + (TraceDirection * FMath::Max(SurfaceTraceDistance, 1.f));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_ItemSurfaceSnap), false, this);
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		return;
	}

	if (PlacementMode == EGvTItemPlacementMode::Wall)
	{
		const FVector WallNormal = Hit.ImpactNormal.GetSafeNormal();
		const FRotator WallRotation = FRotationMatrix::MakeFromXZ(WallNormal, FVector::UpVector).Rotator();
		SetActorRotation(WallRotation, ETeleportType::TeleportPhysics);
		Mesh->UpdateBounds();

		const FBoxSphereBounds RotatedBounds = Mesh->Bounds;
		const FVector Extent = RotatedBounds.BoxExtent;
		const float NormalExtent =
			FMath::Abs(WallNormal.X) * Extent.X +
			FMath::Abs(WallNormal.Y) * Extent.Y +
			FMath::Abs(WallNormal.Z) * Extent.Z;
		const FVector DesiredCenter = Hit.ImpactPoint + WallNormal * (NormalExtent + SurfaceClearance);
		AddActorWorldOffset(DesiredCenter - RotatedBounds.Origin, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		const float MeshBottomZ = WorldBounds.Origin.Z - WorldBounds.BoxExtent.Z;
		const float VerticalOffset = (Hit.ImpactPoint.Z + SurfaceClearance) - MeshBottomZ;
		AddActorWorldOffset(FVector(0.f, 0.f, VerticalOffset), false, nullptr, ETeleportType::TeleportPhysics);
	}

	ForceNetUpdate();
}


void AGvTInteractableItem::GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const
{
	OutSpec = FGvTInteractionSpec{};

	if (Verb == EGvTInteractionVerb::Scan)
	{
		OutSpec.CastTime = ScanCastTime;
		OutSpec.bLockMovement = true;
		OutSpec.bLockLook = true;
		OutSpec.bCancelable = true;

		OutSpec.bEmitNoiseOnCancel = true;
		OutSpec.CancelNoiseRadius = ScanNoiseRadius * 0.75f;
		OutSpec.CancelNoiseLoudness = 1.0f;
		OutSpec.InteractionTag = ScanNoiseTag;

		OutSpec.LoopSfx = ScanLoopSfx;
		OutSpec.EndSfx = ScanEndSfx;
		OutSpec.CancelSfx = ScanCancelSfx;
	}
	else if (Verb == EGvTInteractionVerb::Photo)
	{
		OutSpec.CastTime = PhotoCastTime;
		OutSpec.bLockMovement = true;
		OutSpec.bLockLook = true;
		OutSpec.bCancelable = true;

		OutSpec.bEmitNoiseOnCancel = true;
		OutSpec.CancelNoiseRadius = PhotoNoiseRadius * 0.75f;
		OutSpec.CancelNoiseLoudness = 1.0f;
		OutSpec.InteractionTag = PhotoNoiseTag;
	}
	else
	{
		OutSpec.CastTime = InteractCastTime;
		OutSpec.bLockMovement = bLockMoveDuringInteract;
		OutSpec.bLockLook = bLockLookDuringInteract;
		OutSpec.bCancelable = true;

		OutSpec.bEmitNoiseOnCancel = true;
		OutSpec.CancelNoiseRadius = InteractNoiseRadius * 0.75f;
		OutSpec.CancelNoiseLoudness = 1.0f;
		OutSpec.InteractionTag = InteractNoiseTag;

		OutSpec.LoopSfx = InteractLoopSfx;
		OutSpec.EndSfx = InteractEndSfx;
		OutSpec.CancelSfx = InteractCancelSfx;
	}
}

bool AGvTInteractableItem::CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const
{
	if (bIsConsumed || Carrier)
	{
		return false;
	}

	if (Verb == EGvTInteractionVerb::Scan)
	{
		return !bHasBeenScanned;
	}

	if (Verb == EGvTInteractionVerb::Interact)
	{
		const AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(InstigatorPawn);
		const UGvTInventoryComponent* Inventory = Thief ? Thief->GetInventoryComponent() : nullptr;
		return Inventory && Inventory->CanAddItem(this);
	}

	return true;
}

void AGvTInteractableItem::BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	// Optional: play a local SFX/FX via multicast later.
}

void AGvTInteractableItem::CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Verb == EGvTInteractionVerb::Scan)
	{
		if (bHasBeenScanned || bIsConsumed)
		{
			return;
		}

		bHasBeenScanned = true;

		if (InstigatorPawn)
		{
			if (UGvTNoiseEmitterComponent* Noise = InstigatorPawn->FindComponentByClass<UGvTNoiseEmitterComponent>())
			{
				Noise->EmitNoise(ScanNoiseTag, ScanNoiseRadius, 1.0f);
			}
		}

		AppraisedValue = FMath::RoundToInt(float(BaseValue) * ScanMultiplier);

		if (AGvTPlayerController* PC = InstigatorPawn ? Cast<AGvTPlayerController>(InstigatorPawn->GetController()) : nullptr)
		{
			PC->Client_ShowScanResult(this, DisplayName, AppraisedValue);
		}

		return;
	}

	if (Verb != EGvTInteractionVerb::Interact || bIsConsumed || Carrier)
	{
		return;
	}

	AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(InstigatorPawn);
	UGvTInventoryComponent* Inventory = Thief ? Thief->GetInventoryComponent() : nullptr;
	if (!Inventory || !Inventory->TryAddItem(this))
	{
		UE_LOG(LogTemp, Log, TEXT("[ItemPickup] Item=%s Player=%s Result=FAILED"), *GetNameSafe(this), *GetNameSafe(InstigatorPawn));
		return;
	}

	if (UGvTNoiseEmitterComponent* Noise = Thief->FindComponentByClass<UGvTNoiseEmitterComponent>())
	{
		Noise->EmitNoise(InteractNoiseTag, InteractNoiseRadius, 1.f);
	}

	if (!bHasTriggeredTheftReaction)
	{
		bHasTriggeredTheftReaction = true;
		if (IsStolenLoot())
		{
			if (UWorld* World = GetWorld())
			{
				if (UGvTDirectorSubsystem* Director = World->GetGameInstance()->GetSubsystem<UGvTDirectorSubsystem>())
				{
					Director->OnPlayerInteractionEvent(Thief, this, EGvTInteractionVerb::Interact);
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ItemPickup] Item=%s Player=%s Result=SUCCESS FirstTheft=%d"), *GetNameSafe(this), *GetNameSafe(Thief), bHasTriggeredTheftReaction ? 1 : 0);
}

void AGvTInteractableItem::CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason)
{
	// Optional: stop SFX/FX. Noise-on-cancel is handled by InteractionComponent.
}

bool AGvTInteractableItem::HasGhostTrait(EGvTItemGhostTrait Trait) const
{
	return GhostTraits.Contains(Trait);
}

bool AGvTInteractableItem::IsGhostValuable() const
{
	return HasGhostTrait(EGvTItemGhostTrait::Valuable)
		|| bTreatAsValuableForGhosts
		|| BaseValue >= ValuableGhostReactionValueThreshold;
}

bool AGvTInteractableItem::IsGhostNoisy() const
{
	return HasGhostTrait(EGvTItemGhostTrait::Noisy)
		|| bTreatAsNoisyForGhosts;
}

bool AGvTInteractableItem::IsGhostElectrical() const
{
	return HasGhostTrait(EGvTItemGhostTrait::Electrical)
		|| bTreatAsElectricalForGhosts;
}

float AGvTInteractableItem::GetGhostReactionChance() const
{
	float TierChance = 0.20f;

	switch (ItemTier)
	{
		case EGvTItemTier::Small:
			TierChance = 0.20f;
			break;

		case EGvTItemTier::Medium:
			TierChance = 0.35f;
			break;

		case EGvTItemTier::Large:
			TierChance = 0.55f;
			break;

		case EGvTItemTier::MainObjective:
			TierChance = 1.00f;
			break;

		default:
			break;
	}

	return FMath::Clamp(TierChance, 0.0f, 1.0f);
}

float AGvTInteractableItem::GetGhostTensionImpulse() const
{
	float BaseImpulse = 0.03f;

	switch (ItemTier)
	{
		case EGvTItemTier::Small:
			BaseImpulse = 0.03f;
			break;

		case EGvTItemTier::Medium:
			BaseImpulse = 0.07f;
			break;

		case EGvTItemTier::Large:
			BaseImpulse = 0.12f;
			break;

		case EGvTItemTier::MainObjective:
			BaseImpulse = 0.25f;
			break;

		default:
			break;
	}

	return FMath::Clamp(
		BaseImpulse * FMath::Max(HouseTensionMultiplier, 0.0f),
		0.0f,
		1.0f);
}

float AGvTInteractableItem::GetGhostItemValue01() const
{
	const float Denominator = FMath::Max(1.f, float(HighValueGhostReactionValue));
	return FMath::Clamp(float(BaseValue) / Denominator, 0.f, 1.f);
}

int32 AGvTInteractableItem::GetInventorySpaceCost() const
{
	if (InventorySpaceOverride > 0)
	{
		return InventorySpaceOverride;
	}

	switch (ItemTier)
	{
		case EGvTItemTier::Small: return 1;
		case EGvTItemTier::Medium: return 2;
		case EGvTItemTier::Large: return 3;
		case EGvTItemTier::MainObjective: return 1;
		default: return 1;
	}
}

FVector AGvTInteractableItem::GetDropCollisionExtent() const
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return FVector(15.f);
	}

	const FVector LocalExtent = Mesh->GetStaticMesh()->GetBounds().BoxExtent;
	const FVector ComponentScale = Mesh->GetComponentScale();
	const FVector AbsoluteScale(FMath::Abs(ComponentScale.X), FMath::Abs(ComponentScale.Y), FMath::Abs(ComponentScale.Z));
	return LocalExtent * AbsoluteScale;
}

void AGvTInteractableItem::SetCarriedBy(AGvTThiefCharacter* NewCarrier, bool bNewEquipped)
{
	if (!HasAuthority())
	{
		return;
	}

	Carrier = NewCarrier;
	bIsEquipped = NewCarrier && bNewEquipped;
	SetOwner(NewCarrier);
	if (bEnableHeldScreenScare && bIsEquipped && !bHasRolledHeldScreenScare)
	{
		bHasRolledHeldScreenScare = true;
		bShowHeldScreenScare = NormalHeldScreenMaterial
			&& ScaryHeldScreenMaterial
			&& FMath::FRand() <= FMath::Clamp(HeldScreenScareChance, 0.0f, 1.0f);
		if (bShowHeldScreenScare && HeldScreenScareSound)
		{
			Client_PlayHeldScreenScareSound();
		}
	}
	ApplyCarryState();
	ForceNetUpdate();
}

void AGvTInteractableItem::DropFromInventory(const FVector& WorldLocation, const FRotator& WorldRotation)
{
	if (!HasAuthority())
	{
		return;
	}

	AGvTThiefCharacter* PreviousCarrier = Carrier;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Carrier = nullptr;
	bIsEquipped = false;
	bShowHeldScreenScare = false;
	SetOwner(nullptr);
	SetActorLocationAndRotation(WorldLocation, WorldRotation, false, nullptr, ETeleportType::TeleportPhysics);
	ApplyCarryState();

	if (Mesh)
	{
		// Configure the body completely before applying velocity or impulse. This
		// prevents thin props from taking their first physics step with stale collision.
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Mesh->SetEnableGravity(true);
		Mesh->SetLinearDamping(DroppedLinearDamping);
		Mesh->SetAngularDamping(DroppedAngularDamping);
		Mesh->SetUseCCD(bUseContinuousCollisionDetectionWhenDropped);
		Mesh->SetSimulatePhysics(true);
		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Mesh->WakeAllRigidBodies();
	}

	bImpactArmed = true;
	if (Mesh && Mesh->IsSimulatingPhysics())
	{
		const FVector TossImpulse = PreviousCarrier ? PreviousCarrier->GetActorForwardVector() * DropForwardImpulse - FVector::UpVector * DropDownwardImpulse : -FVector::UpVector * DropDownwardImpulse;
		Mesh->AddImpulse(TossImpulse, NAME_None, true);
	}

	ForceNetUpdate();
}

void AGvTInteractableItem::ApplyCarryState()
{
	if (Carrier)
	{
		bImpactArmed = false;

		if (USceneComponent* Anchor = Carrier->GetHeldItemAnchor())
		{
			AttachToComponent(Anchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			const bool bUseLargeHeldOffset = ItemTier == EGvTItemTier::Large;
			const FVector EquippedLocation = HeldRelativeLocation + (bUseLargeHeldOffset ? LargeHeldLocationOffset : FVector::ZeroVector);
			const FRotator EquippedRotation = HeldRelativeRotation + (bUseLargeHeldOffset ? LargeHeldRotationOffset : FRotator::ZeroRotator);
			SetActorRelativeLocation(EquippedLocation);
			SetActorRelativeRotation(EquippedRotation);

			SetActorRelativeScale3D(HeldRelativeScale);
		}

		SetActorEnableCollision(false);
		Mesh->SetSimulatePhysics(false);
		SetActorHiddenInGame(!bIsEquipped);
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorHiddenInGame(false);
		SetActorEnableCollision(true);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Mesh->SetEnableGravity(true);
		Mesh->SetLinearDamping(DroppedLinearDamping);
		Mesh->SetAngularDamping(DroppedAngularDamping);
		Mesh->SetUseCCD(bUseContinuousCollisionDetectionWhenDropped);
		Mesh->SetSimulatePhysics(true);
		Mesh->WakeAllRigidBodies();
	}

	ApplyHeldScreenMaterial();
}

void AGvTInteractableItem::ApplyHeldScreenMaterial()
{
	if (!bEnableHeldScreenScare
		|| !Mesh
		|| !NormalHeldScreenMaterial
		|| !ScaryHeldScreenMaterial
		|| HeldScreenMaterialIndex < 0
		|| HeldScreenMaterialIndex >= Mesh->GetNumMaterials())
	{
		return;
	}

	UMaterialInterface* Material = Carrier && bIsEquipped && bShowHeldScreenScare
		? ScaryHeldScreenMaterial.Get()
		: NormalHeldScreenMaterial.Get();

	if (Material)
	{
		Mesh->SetMaterial(HeldScreenMaterialIndex, Material);
	}
}

void AGvTInteractableItem::HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || !bImpactArmed || !Mesh || Carrier)
	{
		return;
	}

	const float CurrentSpeed = Mesh->GetPhysicsLinearVelocity().Size();
	const float Mass = FMath::Max(Mesh->GetMass(), 1.f);
	const float ImpulseSpeed = NormalImpulse.Size() / Mass;
	const float ImpactSpeed = FMath::Max(CurrentSpeed, ImpulseSpeed);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (ImpactSpeed < MinimumImpactSpeedForSound || (Now - LastImpactSoundTime) < ImpactSoundCooldown)
	{
		return;
	}

	// A dropped item may bounce or roll several times. Only the first meaningful
	// impact after each drop should play audio and emit ghost-hearing noise.
	bImpactArmed = false;
	LastImpactSoundTime = Now;
	FVector ImpactLocation = GetActorLocation();

	if (!Hit.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(Hit.ImpactPoint);
	}

	if (DropImpactSounds.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, DropImpactSounds.Num() - 1);
		Multicast_PlayDropImpactSound(DropImpactSounds[Index], ImpactLocation, ImpactSpeed);
	}

	if (UWorld* World = GetWorld())
	{
		if (UGvTNoiseSubsystem* NoiseSubsystem = World->GetGameInstance()->GetSubsystem<UGvTNoiseSubsystem>())
		{
			FGvTNoiseEvent NoiseEvent;
			NoiseEvent.Location = ImpactLocation;
			NoiseEvent.Radius = DropNoiseRadius;
			NoiseEvent.Loudness = DropNoiseLoudness;
			NoiseEvent.NoiseTag = DropNoiseTag;
			NoiseSubsystem->EmitNoise(NoiseEvent);
		}
	}
}

void AGvTInteractableItem::Multicast_PlayDropImpactSound_Implementation(USoundBase* Sound, FVector Location, float ImpactSpeed)
{
	if (!Sound)
	{
		return;
	}

	const float Volume = FMath::GetMappedRangeValueClamped(
		FVector2D(MinimumImpactSpeedForSound, 1200.f),
		FVector2D(0.4f, 1.0f),
		ImpactSpeed);

	const float Pitch = FMath::FRandRange(0.96f, 1.04f);

	UGameplayStatics::PlaySoundAtLocation(
		this,
		Sound,
		Location,
		Volume,
		Pitch);
}


void AGvTInteractableItem::OnRep_CarryState()
{
	ApplyCarryState();
}

void AGvTInteractableItem::OnRep_HeldScreenScare()
{
	ApplyHeldScreenMaterial();
}

void AGvTInteractableItem::Client_PlayHeldScreenScareSound_Implementation()
{
	if (HeldScreenScareSound)
	{
		UGameplayStatics::PlaySound2D(this, HeldScreenScareSound);
	}
}

void AGvTInteractableItem::ApplyConsumedState(bool bConsumed)
{
	SetActorEnableCollision(!bConsumed);
	SetActorHiddenInGame(bConsumed);
}

void AGvTInteractableItem::OnRep_IsConsumed()
{
	ApplyConsumedState(bIsConsumed);
}

void AGvTInteractableItem::OnRep_HasPhoto()
{
	// Placeholder
}

void AGvTInteractableItem::OnRep_HasBeenScanned()
{
	// Optional: update material/outline/UI later
}

void AGvTInteractableItem::OnRep_SelectedMesh()
{
	if (SelectedMesh)
	{
		Mesh->SetStaticMesh(SelectedMesh);
		ApplyHeldScreenMaterial();
	}
}

void AGvTInteractableItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTInteractableItem, Carrier);
	DOREPLIFETIME(AGvTInteractableItem, bIsEquipped);
	DOREPLIFETIME(AGvTInteractableItem, bShowHeldScreenScare);
	DOREPLIFETIME(AGvTInteractableItem, bHasTriggeredTheftReaction);
	DOREPLIFETIME(AGvTInteractableItem, bIsConsumed);
	DOREPLIFETIME(AGvTInteractableItem, bHasBeenPhotographed);
	DOREPLIFETIME(AGvTInteractableItem, AppraisedValue);
	DOREPLIFETIME(AGvTInteractableItem, bHasBeenScanned);
	DOREPLIFETIME(AGvTInteractableItem, SelectedMesh);
}
