#include "Systems/Audio/GvTAmbientAudioPoint.h"
#include "Components/SceneComponent.h"

AGvTAmbientAudioPoint::AGvTAmbientAudioPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}