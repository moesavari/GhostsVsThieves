#include "Systems/Audio/GvTHouseEntryTrigger.h"

#include "Components/BoxComponent.h"
#include "Systems/Audio/GvTHouseVoiceDirector.h"

AGvTHouseEntryTrigger::AGvTHouseEntryTrigger()
{
	EntryTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryTrigger"));
	SetRootComponent(EntryTrigger);
	EntryTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EntryTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	EntryTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AGvTHouseEntryTrigger::BeginPlay()
{
	Super::BeginPlay();
	EntryTrigger->OnComponentBeginOverlap.AddDynamic(this, &AGvTHouseEntryTrigger::HandleEntryOverlap);
}

void AGvTHouseEntryTrigger::HandleEntryOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (HasAuthority() && HouseVoiceDirector)
	{
		HouseVoiceDirector->TryPlayFirstEntryVoice(OtherActor);
	}
}
