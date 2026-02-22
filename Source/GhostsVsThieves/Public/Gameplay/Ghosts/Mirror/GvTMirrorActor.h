#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GvTMirrorActor.generated.h"

class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class AGvTReflectGhostActor;

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTMirrorActor : public AActor
{
	GENERATED_BODY()

public:
	AGvTMirrorActor();

	virtual void BeginPlay() override;

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "GvT|Mirror")
	void TriggerReflectLocal(float Intensity01, float LifeSeconds);
	
	UFUNCTION(BlueprintCallable, Category = "GvT|Mirror")
	void TriggerScare(float Intensity01 = 1.f, float LifeSeconds = 1.5f);

	UFUNCTION(Server, Reliable)
	void ServerTriggerScare(float Intensity01, float LifeSeconds);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastTriggerScare(float Intensity01, float LifeSeconds);

	UStaticMeshComponent* GetMirrorMesh() const { return MirrorMesh; }
	USceneCaptureComponent2D* GetCapture() const { return MirrorCapture; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	TObjectPtr<UStaticMeshComponent> MirrorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	TObjectPtr<USceneCaptureComponent2D> MirrorCapture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	TSubclassOf<AGvTReflectGhostActor> ReflectGhostClass;

	// Which material slot on the mirror mesh should get the capture material
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	int32 MirrorMaterialSlot = 1;

	// Parameter name in the mirror material
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	FName MirrorTextureParam = "MirrorTex";

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror|Materials")
	TObjectPtr<UMaterialInterface> NormalMirrorMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror|Materials")
	TObjectPtr<UMaterialInterface> ScareMirrorMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror|Ghost")
	float GhostDistanceFromCapture = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror|Ghost")
	float GhostRightOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror|Ghost")
	float GhostUpOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror|Ghost")
	bool bGhostFacesCapture = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror|Perf")
	float CaptureFPS = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	float GhostDepth = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	float GhostZOffset = -40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	float GhostYawOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	float GhostPitchOffset = 0.f;

	// If the reflection looks "backwards", flip the mirror normal.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	bool bFlipMirrorNormal = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror")
	float ClipPlaneBias = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror")
	UStaticMeshComponent* MirrorSurface = nullptr;

	UPROPERTY(EditAnywhere, Category = "GvT|Mirror|Surface")
	float SurfacePushOut = 1.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Mirror|Surface")
	float SurfaceScalePadding = 0.98f; 

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror|Audio")
	TObjectPtr<USoundBase> ScareStartSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror|Audio")
	TObjectPtr<USoundBase> ScareEndSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror|Audio")
	float ScareSfxVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Mirror|Audio")
	float ScareSfxPitch = 1.0f;

private:
	TObjectPtr<UMaterialInstanceDynamic> MirrorMID;
	TObjectPtr<UMaterialInstanceDynamic> MirrorSurfaceMID;

	TObjectPtr<AGvTReflectGhostActor> GhostInstance;
	
	FTimerHandle CaptureTimer;
	FTimerHandle StopTimer;

	bool bMirrorActive = false;
	bool bScareActive = false;

	void ApplyMaterialToSlot(UMaterialInterface* Mat);
	void StartScareCapture(float LifeSeconds);
	void StopScareCapture();
	void CaptureOnce();
	void SetScarePlaneVisible(bool bVisible);

	TObjectPtr<UMaterialInterface> CachedNormalMat;
	TObjectPtr<UMaterialInstanceDynamic> CachedMID;

	bool IsLocalClientWorld() const;
	void EnsureMID();
	void EnsureGhost();

	void UpdateGhostForCaptureCamera();
	void EnsureGhostConfiguredForCaptureOnly();

	void ApplyNormalMaterial(UMaterialInterface* Mat);
	void ApplyScareMaterial(UMaterialInterface* Mat);

	void AlignMirrorSurfaceToMesh();
};
