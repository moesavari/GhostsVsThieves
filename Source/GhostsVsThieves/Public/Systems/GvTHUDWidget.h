#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GvTHUDWidget.generated.h"

UENUM(BlueprintType)
enum class EGvTPanicCueTier : uint8
{
	Calm UMETA(DisplayName="Calm"),
	Uneasy UMETA(DisplayName="Uneasy"),
	Danger UMETA(DisplayName="Danger"),
	Critical UMETA(DisplayName="Critical")
};

UCLASS()
class GHOSTSVSTHIEVES_API UGvTHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent)
	void SetLootValue(int32 Value);

	/** Shared value already secured in the van; distinct from this player's carried loot. */
	UFUNCTION(BlueprintImplementableEvent, Category="GvT|HUD")
	void SetTeamSecuredLoot(int32 Value);

	UFUNCTION()
	void HandleLootChanged(int32 NewLoot);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void ShowScanValueNamed(AActor* Item, const FText& DisplayName, int32 Value);

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|HUD")
	void UpdatePanicDisplay(float NewPanic01);

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|HUD|Panic")
	void UpdatePanicCues(float NewPanic01, EGvTPanicCueTier Tier, bool bVisualEffectsEnabled, bool bAudioEffectsEnabled);

	/** Displays short player-facing feedback such as interaction failures or mission status. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|HUD")
	void ShowHUDMessage(const FText& Message, bool bSuccess);
};
