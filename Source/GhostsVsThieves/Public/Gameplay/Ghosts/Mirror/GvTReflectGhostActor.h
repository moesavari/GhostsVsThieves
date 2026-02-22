#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GvTReflectGhostActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTReflectGhostActor : public AActor
{
	GENERATED_BODY()

public:
	AGvTReflectGhostActor();

	UFUNCTION(BlueprintCallable, Category = "GvT|Mirror")
	void StartReflect(float InIntensity01, float InLifeSeconds);

	UFUNCTION(BlueprintCallable, Category = "GvT|Mirror")
	void StopReflect();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	UStaticMeshComponent* GetMesh() const { return Mesh; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	UPROPERTY(EditAnywhere, Category = "GvT|Mirror")
	float MinOpacity = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Mirror")
	float MaxOpacity = 0.85f;

	UPROPERTY(EditAnywhere, Category = "GvT|Mirror")
	float MinEmissive = 2.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Mirror")
	float MaxEmissive = 8.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Mirror")
	bool bEnableFlicker = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Mirror")
	float FlickerSpeed = 12.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Mirror")
	FName OpacityParam = "Opacity";

	UPROPERTY(EditAnywhere, Category = "GvT|Mirror")
	FName EmissiveParam = "Emissive";

private:
	float Intensity01 = 0.f;
	float LifeSeconds = 0.f;
	float Elapsed = 0.f;
	bool bActive = false;
};
