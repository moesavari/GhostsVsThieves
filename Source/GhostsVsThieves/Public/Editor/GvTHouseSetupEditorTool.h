#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GvTHouseSetupEditorTool.generated.h"

class AGvTHouseManager;

/** Place temporarily in a level to expose repeatable house-setup buttons in Details. */
UCLASS(Blueprintable)
class GHOSTSVSTHIEVES_API AGvTHouseSetupEditorTool : public AActor
{
	GENERATED_BODY()

public:
	AGvTHouseSetupEditorTool();

	UFUNCTION(CallInEditor, Category = "GvT|House Setup")
	void CreateOrFindHouseManager();

	/** Select every level actor that owns a LightComponent, then press this button. */
	UFUNCTION(CallInEditor, Category = "GvT|House Setup")
	void RegisterSelectedLightActors();

	/** Select one or more volumes, then press this button. */
	UFUNCTION(CallInEditor, Category = "GvT|House Setup")
	void ConfigureSelectedHouseBounds();

	UFUNCTION(CallInEditor, Category = "GvT|House Setup")
	void AssignManagerToAllPowerBoxes();

	UFUNCTION(CallInEditor, Category = "GvT|House Setup")
	void ValidateCurrentLevel();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "GvT|House Setup")
	TObjectPtr<AGvTHouseManager> HouseManager;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|House Setup", meta = (MultiLine = "true"))
	FString LastValidationReport;

private:
	AGvTHouseManager* ResolveHouseManager(bool bCreateIfMissing);
};
