#include "World/Items/GvTFlashlightItem.h"
#include "Components/SpotLightComponent.h"
#include "Net/UnrealNetwork.h"

AGvTFlashlightItem::AGvTFlashlightItem()
{
	FlashlightBeam = CreateDefaultSubobject<USpotLightComponent>(TEXT("FlashlightBeam"));
	FlashlightBeam->SetupAttachment(Mesh);
	FlashlightBeam->SetVisibility(false);
	FlashlightBeam->SetCastShadows(true);
}

void AGvTFlashlightItem::ToggleFlashlight()
{
	if (!Carrier || !bIsEquipped)
	{
		return;
	}

	const bool bNewOn = !bIsFlashlightOn;
	if (HasAuthority())
	{
		SetFlashlightOnAuthoritative(bNewOn);
	}
	else
	{
		Server_SetFlashlightOn(bNewOn);
	}
}

void AGvTFlashlightItem::Server_SetFlashlightOn_Implementation(bool bNewOn)
{
	SetFlashlightOnAuthoritative(bNewOn);
}

void AGvTFlashlightItem::SetFlashlightOnAuthoritative(bool bNewOn)
{
	if (!HasAuthority())
	{
		return;
	}

	bIsFlashlightOn = bNewOn && Carrier && bIsEquipped;
	ApplyFlashlightState();
	ForceNetUpdate();
}

void AGvTFlashlightItem::ApplyCarryState()
{
	Super::ApplyCarryState();

	if (HasAuthority() && (!Carrier || !bIsEquipped) && bIsFlashlightOn)
	{
		bIsFlashlightOn = false;
		ForceNetUpdate();
	}

	ApplyFlashlightState();
}

void AGvTFlashlightItem::OnRep_FlashlightOn()
{
	ApplyFlashlightState();
}

void AGvTFlashlightItem::ApplyFlashlightState()
{
	if (!FlashlightBeam)
	{
		return;
	}

	FlashlightBeam->SetIntensity(BeamIntensity);
	FlashlightBeam->SetAttenuationRadius(BeamRange);
	FlashlightBeam->SetInnerConeAngle(BeamInnerConeAngle);
	FlashlightBeam->SetOuterConeAngle(BeamOuterConeAngle);

	const bool bShouldBeVisible = bIsFlashlightOn && Carrier && bIsEquipped;
	FlashlightBeam->SetVisibility(bShouldBeVisible, true);
}

void AGvTFlashlightItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTFlashlightItem, bIsFlashlightOn);
}
