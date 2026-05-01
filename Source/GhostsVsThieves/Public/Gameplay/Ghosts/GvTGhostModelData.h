#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GvTGhostModelData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UPhysicsAsset;
class USoundBase;
class ACharacter;
class AGvTGhostCharacterBase;

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API UGvTGhostModelData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Identity")
	FName ModelId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Presentation")
	TSubclassOf<AGvTGhostCharacterBase> GhostActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Presentation")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Presentation")
	TSubclassOf<UAnimInstance> AnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Presentation")
	TObjectPtr<UPhysicsAsset> PhysicsAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Collision")
	float CapsuleRadius = 34.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Collision")
	float CapsuleHalfHeight = 88.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Movement")
	float BaseWalkSpeed = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Movement")
	float BaseRunSpeed = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Movement")
	float BaseAcceleration = 2048.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Movement")
	float BaseBrakingDecel = 2048.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Offsets")
	FVector OverheadSpawnOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Offsets")
	FVector ChaseVisualOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost|Tags")
	FGameplayTagContainer ModelTags;
};