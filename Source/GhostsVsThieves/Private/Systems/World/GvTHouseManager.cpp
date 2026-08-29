#include "Systems/World/GvTHouseManager.h"

#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Systems/GvTPowerBoxActor.h"
#include "Systems/Light/GvTLightFlickerComponent.h"

AGvTHouseManager::AGvTHouseManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	LightFlickerComponent = CreateDefaultSubobject<UGvTLightFlickerComponent>(TEXT("LightFlickerComponent"));
	Tags.AddUnique(FName(TEXT("HouseManager")));
}

void AGvTHouseManager::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && bChooseOneRandomPowerBox)
	{
		ChooseRandomActivePowerBox();
	}
}

void AGvTHouseManager::ChooseRandomActivePowerBox()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	TArray<AGvTPowerBoxActor*> AssignedPowerBoxes;
	for (TActorIterator<AGvTPowerBoxActor> It(GetWorld()); It; ++It)
	{
		AGvTPowerBoxActor* PowerBox = *It;
		if (IsValid(PowerBox) && PowerBox->GetHouseActor() == this)
		{
			AssignedPowerBoxes.Add(PowerBox);
		}
	}

	if (AssignedPowerBoxes.IsEmpty())
	{
		ActivePowerBox = nullptr;
		UE_LOG(LogTemp, Warning, TEXT("[HousePower] %s found no assigned power boxes."), *GetNameSafe(this));
		return;
	}

	ActivePowerBox = AssignedPowerBoxes[FMath::RandRange(0, AssignedPowerBoxes.Num() - 1)];
	for (AGvTPowerBoxActor* PowerBox : AssignedPowerBoxes)
	{
		PowerBox->SetActiveBreaker(PowerBox == ActivePowerBox);
	}

	ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("[HousePower] %s selected %s from %d assigned breakers."),
		*GetNameSafe(this), *GetNameSafe(ActivePowerBox), AssignedPowerBoxes.Num());
}

void AGvTHouseManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTHouseManager, ActivePowerBox);
}
