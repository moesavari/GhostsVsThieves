#include "Gameplay/Ghosts/Crawler/GvTAnimNotify_CrawlerDragStep.h"
#include "Gameplay/Ghosts/Crawler/GvTCrawlerGhostCharacter.h"

void UGvTAnimNotify_CrawlerDragStep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	AGvTCrawlerGhostCharacter* Ghost = Cast<AGvTCrawlerGhostCharacter>(Owner);
	if (!Ghost) return;

	if (Ghost->HasAuthority())
	{
		Ghost->OnCrawlerDragStep();
	}
}