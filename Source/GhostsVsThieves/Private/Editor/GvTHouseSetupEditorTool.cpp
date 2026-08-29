#include "Editor/GvTHouseSetupEditorTool.h"

#include "Components/BrushComponent.h"
#include "Components/LightComponent.h"
#include "Engine/TriggerVolume.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Gameplay/Ghosts/GvTGhostSpawnPoint.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Systems/GvTPowerBoxActor.h"
#include "Systems/Light/GvTLightFlickerComponent.h"
#include "Systems/World/GvTHouseManager.h"
#include "World/Extraction/GvTExtractionDepartureActor.h"
#include "World/Extraction/GvTReconDepositActor.h"
#include "World/Items/GvTInteractableItem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Selection.h"
#include "ScopedTransaction.h"
#endif

AGvTHouseSetupEditorTool::AGvTHouseSetupEditorTool()
{
	PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITORONLY_DATA
	bIsEditorOnlyActor = true;
#endif
}

AGvTHouseManager* AGvTHouseSetupEditorTool::ResolveHouseManager(bool bCreateIfMissing)
{
	if (IsValid(HouseManager))
	{
		return HouseManager;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGvTHouseManager> It(World); It; ++It)
	{
		HouseManager = *It;
		return HouseManager;
	}

#if WITH_EDITOR
	if (bCreateIfMissing)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("GvTHouseSetup", "CreateManager", "Create GvT House Manager"));
		World->Modify();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		HouseManager = World->SpawnActor<AGvTHouseManager>(GetActorLocation(), FRotator::ZeroRotator, Params);
		if (HouseManager)
		{
			HouseManager->SetActorLabel(TEXT("BP_HouseManager"));
			HouseManager->SetFolderPath(TEXT("Gameplay/House"));
		}
	}
#endif
	return HouseManager;
}

void AGvTHouseSetupEditorTool::CreateOrFindHouseManager()
{
	ResolveHouseManager(true);
}

void AGvTHouseSetupEditorTool::RegisterSelectedLightActors()
{
#if WITH_EDITOR
	AGvTHouseManager* Manager = ResolveHouseManager(true);
	if (!Manager || !GEditor)
	{
		return;
	}

	TArray<AActor*> LightActors;
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!IsValid(Actor) || Actor == this || Actor == Manager)
		{
			continue;
		}

		if (Actor->FindComponentByClass<ULightComponent>())
		{
			LightActors.AddUnique(Actor);
		}
	}

	if (UGvTLightFlickerComponent* Flicker = Manager->GetLightFlickerComponent())
	{
		const FScopedTransaction Transaction(NSLOCTEXT("GvTHouseSetup", "RegisterLights", "Register GvT House Lights"));
		Manager->Modify();
		Flicker->Modify();
		Flicker->SetExplicitLightActors(LightActors);
		Manager->MarkPackageDirty();
		UE_LOG(LogTemp, Display, TEXT("[HouseSetup] Registered %d selected light actors on %s."), LightActors.Num(), *GetNameSafe(Manager));
	}
#endif
}

void AGvTHouseSetupEditorTool::ConfigureSelectedHouseBounds()
{
#if WITH_EDITOR
	if (!GEditor)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("GvTHouseSetup", "ConfigureBounds", "Configure GvT House Bounds"));
	int32 Configured = 0;
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		AVolume* Volume = Cast<AVolume>(*It);
		if (!IsValid(Volume))
		{
			continue;
		}

		Volume->Modify();
		Volume->Tags.AddUnique(FName(TEXT("HouseBounds")));
		if (UBrushComponent* Brush = Volume->GetBrushComponent())
		{
			Brush->Modify();
			Brush->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Brush->SetCollisionResponseToAllChannels(ECR_Ignore);
			Brush->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			Brush->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
			Brush->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
			Brush->SetGenerateOverlapEvents(true);
		}
		Volume->MarkPackageDirty();
		++Configured;
	}

	UE_LOG(LogTemp, Display, TEXT("[HouseSetup] Configured %d selected HouseBounds volumes."), Configured);
#endif
}

void AGvTHouseSetupEditorTool::AssignManagerToAllPowerBoxes()
{
#if WITH_EDITOR
	AGvTHouseManager* Manager = ResolveHouseManager(true);
	if (!Manager || !GetWorld())
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("GvTHouseSetup", "AssignPowerBoxes", "Assign GvT House Manager To Power Boxes"));
	int32 Assigned = 0;
	for (TActorIterator<AGvTPowerBoxActor> It(GetWorld()); It; ++It)
	{
		It->Modify();
		It->SetHouseActor(Manager);
		It->MarkPackageDirty();
		++Assigned;
	}

	UE_LOG(LogTemp, Display, TEXT("[HouseSetup] Assigned %s to %d power boxes."), *GetNameSafe(Manager), Assigned);
#endif
}

void AGvTHouseSetupEditorTool::ValidateCurrentLevel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		LastValidationReport = TEXT("FAIL: No editor world was available.");
		return;
	}

	int32 HouseManagerCount = 0;
	int32 RegisteredLightActorCount = 0;
	int32 PowerBoxCount = 0;
	int32 UnassignedPowerBoxCount = 0;
	int32 HouseBoundsCount = 0;
	int32 GhostSpawnCount = 0;
	int32 NavBoundsCount = 0;
	int32 PlayerStartCount = 0;
	int32 ExtractionCount = 0;
	int32 DepositCount = 0;
	int32 MainObjectiveCount = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (AGvTHouseManager* Manager = Cast<AGvTHouseManager>(Actor))
		{
			++HouseManagerCount;
			if (const UGvTLightFlickerComponent* Flicker = Manager->GetLightFlickerComponent())
			{
				RegisteredLightActorCount += Flicker->GetExplicitLightActorCount();
			}
		}
		if (AGvTPowerBoxActor* PowerBox = Cast<AGvTPowerBoxActor>(Actor))
		{
			++PowerBoxCount;
			UnassignedPowerBoxCount += IsValid(PowerBox->GetHouseActor()) ? 0 : 1;
		}
		if (const AVolume* Volume = Cast<AVolume>(Actor))
		{
			HouseBoundsCount += Volume->ActorHasTag(TEXT("HouseBounds")) ? 1 : 0;
		}
		GhostSpawnCount += Actor->IsA<AGvTGhostSpawnPoint>() ? 1 : 0;
		NavBoundsCount += Actor->IsA<ANavMeshBoundsVolume>() ? 1 : 0;
		PlayerStartCount += Actor->IsA<APlayerStart>() ? 1 : 0;
		ExtractionCount += Actor->IsA<AGvTExtractionDepartureActor>() ? 1 : 0;
		DepositCount += Actor->IsA<AGvTReconDepositActor>() ? 1 : 0;
		if (const AGvTInteractableItem* Item = Cast<AGvTInteractableItem>(Actor))
		{
			MainObjectiveCount += Item->IsMainObjective() ? 1 : 0;
		}
	}

	TArray<FString> Lines;
	auto AddCheck = [&Lines](bool bPass, const FString& Message)
	{
		Lines.Add(FString::Printf(TEXT("%s: %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *Message));
	};
	AddCheck(HouseManagerCount == 1, FString::Printf(TEXT("House Managers: %d (expected exactly 1)"), HouseManagerCount));
	AddCheck(RegisteredLightActorCount > 0, FString::Printf(TEXT("Registered light actors: %d"), RegisteredLightActorCount));
	AddCheck(PowerBoxCount > 0, FString::Printf(TEXT("Power boxes: %d"), PowerBoxCount));
	AddCheck(UnassignedPowerBoxCount == 0, FString::Printf(TEXT("Unassigned power boxes: %d"), UnassignedPowerBoxCount));
	AddCheck(HouseBoundsCount > 0, FString::Printf(TEXT("HouseBounds volumes: %d"), HouseBoundsCount));
	AddCheck(GhostSpawnCount > 0, FString::Printf(TEXT("Ghost spawn points: %d"), GhostSpawnCount));
	AddCheck(NavBoundsCount > 0, FString::Printf(TEXT("NavMesh bounds: %d"), NavBoundsCount));
	AddCheck(PlayerStartCount >= 1, FString::Printf(TEXT("Player starts: %d (target 6)"), PlayerStartCount));
	AddCheck(ExtractionCount > 0, FString::Printf(TEXT("Extraction departure actors: %d"), ExtractionCount));
	AddCheck(DepositCount > 0, FString::Printf(TEXT("Recon deposit actors: %d"), DepositCount));
	AddCheck(MainObjectiveCount > 0, FString::Printf(TEXT("Main objectives: %d"), MainObjectiveCount));

	LastValidationReport = FString::Join(Lines, TEXT("\n"));
	UE_LOG(LogTemp, Display, TEXT("[HouseSetup]\n%s"), *LastValidationReport);
}
