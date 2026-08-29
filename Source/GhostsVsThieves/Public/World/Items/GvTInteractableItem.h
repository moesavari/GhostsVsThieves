#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "Engine/StaticMesh.h"
#include "GvTInteractableItem.generated.h"

class UStaticMeshComponent;
class UGvTDirectorSubsystem;
class AGvTThiefCharacter;
class USoundBase;
class UPrimitiveComponent;
class UTexture2D;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EGvTItemTier : uint8
{
	Small			UMETA(DisplayName = "Small"),
	Medium			UMETA(DisplayName = "Medium"),
	Large			UMETA(DisplayName = "Large"),
	MainObjective	UMETA(DisplayName = "Main Objective")
};

UENUM(BlueprintType)
enum class EGvTItemPurpose : uint8
{
	Loot			UMETA(DisplayName = "Stolen Loot"),
	MainObjective	UMETA(DisplayName = "Main Objective"),
	Equipment		UMETA(DisplayName = "Equipment"),
	Consumable		UMETA(DisplayName = "Consumable")
};

UENUM(BlueprintType)
enum class EGvTItemPlacementMode : uint8
{
	Floor	UMETA(DisplayName = "Floor"),
	Wall	UMETA(DisplayName = "Wall")
};

UENUM(BlueprintType)
enum class EGvTItemGhostTrait : uint8
{
	Electrical			UMETA(DisplayName = "Electrical"),
	Valuable			UMETA(DisplayName = "Valuable"),
	Noisy				UMETA(DisplayName = "Noisy"),
	Cursed				UMETA(DisplayName = "Cursed"),
	Religious			UMETA(DisplayName = "Religious"),
	Historic			UMETA(DisplayName = "Historic"),
	Occult				UMETA(DisplayName = "Occult"),
	MirrorBound			UMETA(DisplayName = "Mirror Bound"),
	PersonalBelonging	UMETA(DisplayName = "Personal Belonging"),
	Possessed			UMETA(DisplayName = "Possessed")
};

UCLASS()
class GHOSTSVSTHIEVES_API AGvTInteractableItem : public AActor, public IGvTInteractable
{
	GENERATED_BODY()

public:
	AGvTInteractableItem();

	// IGvTInteractable
	virtual void GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const override;
	virtual void BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason) override;

	UFUNCTION(BlueprintPure, Category = "Item|Inventory")
	int32 GetInventorySpaceCost() const;

	UFUNCTION(BlueprintPure, Category = "Item|Inventory|UI")
	FText GetInventoryDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Item|Inventory|UI")
	FText GetInventoryDescription() const { return InventoryDescription; }

	UFUNCTION(BlueprintPure, Category = "Item|Inventory|UI")
	UTexture2D* GetInventoryIcon() const { return InventoryIcon; }

	UFUNCTION(BlueprintPure, Category = "Item|Inventory")
	bool IsCarried() const { return Carrier != nullptr; }
	AGvTThiefCharacter* GetCarrier() const { return Carrier; }

	UFUNCTION(BlueprintPure, Category = "Item|Scan")
	bool HasBeenScanned() const { return bHasBeenScanned; }

	UFUNCTION(BlueprintPure, Category = "Item|Inventory")
	FVector GetDropCollisionExtent() const;

	UFUNCTION(BlueprintPure, Category = "Item|Value")
	int32 GetSecuredLootValue() const { return FMath::Max(0, BaseValue); }

	UFUNCTION(BlueprintPure, Category = "Item|Rules")
	EGvTItemPurpose GetItemPurpose() const
	{
		return ItemTier == EGvTItemTier::MainObjective ? EGvTItemPurpose::MainObjective : ItemPurpose;
	}

	UFUNCTION(BlueprintPure, Category = "Item|Rules")
	bool IsStolenLoot() const
	{
		const EGvTItemPurpose Purpose = GetItemPurpose();
		return Purpose == EGvTItemPurpose::Loot || Purpose == EGvTItemPurpose::MainObjective;
	}

	UFUNCTION(BlueprintPure, Category = "Item|Rules")
	bool IsMainObjective() const { return GetItemPurpose() == EGvTItemPurpose::MainObjective; }

	void SetCarriedBy(AGvTThiefCharacter* NewCarrier, bool bNewEquipped);
	void DropFromInventory(const FVector& WorldLocation, const FRotator& WorldRotation);

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	bool ShouldUpsetGhostsOnInteract() const
	{
		return bUpsetsGhostsOnInteract;
	}

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	EGvTItemTier GetItemTier() const
	{
		return ItemTier;
	}

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	bool HasGhostTrait(EGvTItemGhostTrait Trait) const;

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	bool IsGhostValuable() const;

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	bool IsGhostNoisy() const;

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	bool IsGhostElectrical() const;

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	float GetGhostReactionChance() const;

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	float GetGhostTensionImpulse() const;

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	float GetHouseTensionMultiplier() const
	{
		return HouseTensionMultiplier;
	}

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	const TArray<FGameplayTag>& GetPreferredGhostEvents() const
	{
		return PreferredGhostEvents;
	}

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	bool ShouldForceHauntReaction() const
	{
		return ItemTier == EGvTItemTier::MainObjective && bMainObjectiveForcesHaunt;
	}

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	float GetGhostItemValue01() const;

protected:
	virtual void BeginPlay() override;

	/** Snaps the selected mesh to its configured display surface. */
	void SnapMeshToSurface();

	UPROPERTY(VisibleAnywhere, Category = "Item")
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, Category = "Item|Rules")
	bool bConsumedOnInteract = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Rules")
	EGvTItemPurpose ItemPurpose = EGvTItemPurpose::Loot;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Inventory", meta = (ClampMin = "0"))
	int32 InventorySpaceOverride = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Inventory")
	FVector HeldRelativeLocation = FVector(35.f, 18.f, -18.f);

	/** Rotation used while the item is attached to the held-item anchor. Use this instead of rotating the root Mesh component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Inventory", meta = (DisplayName = "Held Mesh Rotation"))
	FRotator HeldRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Inventory")
	FVector HeldRelativeScale = FVector::OneVector;

	/** Added to HeldRelativeLocation only for Large-tier items while equipped. Never changes world or dropped scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Inventory|Large Held Offset")
	FVector LargeHeldLocationOffset = FVector(15.f, 25.f, -15.f);

	/** Added to HeldRelativeRotation only for Large-tier items while equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Inventory|Large Held Offset")
	FRotator LargeHeldRotationOffset = FRotator::ZeroRotator;

	// -------------------------------------------------------------------------
	// Physical drop presentation
	// -------------------------------------------------------------------------


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drop")
	FGameplayTag DropNoiseTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drop", meta = (ClampMin = "0.0"))
	float MinimumImpactSpeedForSound = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drop", meta = (ClampMin = "0.0"))
	float ImpactSoundCooldown = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drop", meta = (ClampMin = "0.0"))
	float DropNoiseRadius = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drop", meta = (ClampMin = "0.0"))
	float DropNoiseLoudness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drop", meta = (ClampMin = "0.0"))
	float DropForwardImpulse = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drop", meta = (ClampMin = "0.0"))
	float DropDownwardImpulse = 35.f;

	/** Slows sliding after a dropped item lands without making the initial toss feel frozen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drop|Physics", meta = (ClampMin = "0.0"))
	float DroppedLinearDamping = 0.4f;

	/** Higher values help cylindrical props settle instead of rolling indefinitely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drop|Physics", meta = (ClampMin = "0.0"))
	float DroppedAngularDamping = 4.0f;

	/** Continuous collision detection helps fast or thin dropped props avoid tunneling through floors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Drop|Physics")
	bool bUseContinuousCollisionDetectionWhenDropped = true;


	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	float InteractCastTime = 0.75f;

	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	float PhotoCastTime = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Item|Interaction")
	float ScanCastTime = 0.85f;

	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	bool bLockMoveDuringInteract = true;

	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	bool bLockLookDuringInteract = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Appearance")
	TArray<TObjectPtr<UStaticMesh>> MeshVariants;

	/** Opt-in held-item surprise. Intended for TV screen material slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Appearance|Held Screen Scare")
	bool bEnableHeldScreenScare = false;

	/** Rolled once per item instance when it is first actively equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Appearance|Held Screen Scare", meta = (EditCondition = "bEnableHeldScreenScare", ClampMin = "0.0", ClampMax = "1.0"))
	float HeldScreenScareChance = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Appearance|Held Screen Scare", meta = (EditCondition = "bEnableHeldScreenScare", ClampMin = "0"))
	int32 HeldScreenMaterialIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Appearance|Held Screen Scare", meta = (EditCondition = "bEnableHeldScreenScare"))
	TObjectPtr<UMaterialInterface> NormalHeldScreenMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Appearance|Held Screen Scare", meta = (EditCondition = "bEnableHeldScreenScare"))
	TObjectPtr<UMaterialInterface> ScaryHeldScreenMaterial = nullptr;

	/** Owner-only sting played when the frightening screen material is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Appearance|Held Screen Scare", meta = (EditCondition = "bEnableHeldScreenScare"))
	TObjectPtr<USoundBase> HeldScreenScareSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Placement")
	bool bSnapToSurfaceOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Placement")
	EGvTItemPlacementMode PlacementMode = EGvTItemPlacementMode::Floor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Placement", meta = (ClampMin = "1.0"))
	float SurfaceTraceDistance = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Placement", meta = (ClampMin = "0.0"))
	float SurfaceClearance = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Scan")
	FText DisplayName = FText::FromString(TEXT("Item"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Inventory|UI", meta=(MultiLine="true"))
	FText InventoryDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Inventory|UI")
	TObjectPtr<UTexture2D> InventoryIcon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Scan", meta = (ClampMin = "0.0"))
	float ScanMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Item|Value")
	int32 BaseValue = 50;

	UPROPERTY(EditAnywhere, Category = "Item|Value")
	float PhotoMultiplier = 1.0f;

	// -------------------------------------------------------------------------
	// Data-driven ghost reaction setup
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	EGvTItemTier ItemTier = EGvTItemTier::Small;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	TArray<EGvTItemGhostTrait> GhostTraits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction",
		meta = (ClampMin = "0.0"))
	float HouseTensionMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	TArray<FGameplayTag> PreferredGhostEvents;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	bool bMainObjectiveForcesHaunt = true;

	// -------------------------------------------------------------------------
	// Legacy compatibility
	//
	// These remain so existing item Blueprints continue functioning. New items
	// should primarily use ItemTier and GhostTraits.
	// -------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	bool bUpsetsGhostsOnInteract = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	bool bTreatAsValuableForGhosts = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	bool bTreatAsNoisyForGhosts = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	bool bTreatAsElectricalForGhosts = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Reaction chance is now determined only by ItemTier."))
	float GhostReactionChanceBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction", meta = (ClampMin = "0"))
	int32 ValuableGhostReactionValueThreshold = 25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction", meta = (ClampMin = "1"))
	int32 HighValueGhostReactionValue = 250;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	float InteractNoiseRadius = 800.f;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	float PhotoNoiseRadius = 300.f;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	float ScanNoiseRadius = 250.f;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	FGameplayTag InteractNoiseTag;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	FGameplayTag PhotoNoiseTag;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	FGameplayTag ScanNoiseTag;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> InteractLoopSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> InteractEndSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> InteractCancelSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> ScanLoopSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> ScanEndSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> ScanCancelSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TArray<TObjectPtr<USoundBase>> DropImpactSounds;

	UPROPERTY(ReplicatedUsing=OnRep_CarryState)
	TObjectPtr<AGvTThiefCharacter> Carrier = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_CarryState)
	bool bIsEquipped = false;

	UPROPERTY(ReplicatedUsing = OnRep_HeldScreenScare)
	bool bShowHeldScreenScare = false;

	bool bHasRolledHeldScreenScare = false;

	UPROPERTY(Replicated)
	bool bHasTriggeredTheftReaction = false;

	UFUNCTION()
	void OnRep_CarryState();

	UFUNCTION()
	void OnRep_HeldScreenScare();

	UFUNCTION(Client, Reliable)
	void Client_PlayHeldScreenScareSound();

	UFUNCTION()
	void HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayDropImpactSound(USoundBase* Sound, FVector Location, float ImpactSpeed);

	virtual void ApplyCarryState();
	void ApplyHeldScreenMaterial();
	float LastImpactSoundTime = -1000.f;
	bool bImpactArmed = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsConsumed)
	bool bIsConsumed = false;

	UPROPERTY(ReplicatedUsing = OnRep_HasPhoto)
	bool bHasBeenPhotographed = false;

	UPROPERTY(ReplicatedUsing = OnRep_HasBeenScanned)
	bool bHasBeenScanned = false;

	UPROPERTY(ReplicatedUsing=OnRep_SelectedMesh)
	TObjectPtr<UStaticMesh> SelectedMesh;

	UFUNCTION()
	void OnRep_SelectedMesh();

	UPROPERTY(Replicated)
	int32 AppraisedValue = 0;

	UFUNCTION()
	void OnRep_IsConsumed();

	UFUNCTION()
	void OnRep_HasPhoto();

	UFUNCTION()
	void OnRep_HasBeenScanned();

	void ApplyConsumedState(bool bConsumed);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
