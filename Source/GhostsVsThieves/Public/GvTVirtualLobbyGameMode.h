#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GvTVirtualLobbyGameMode.generated.h"

/** Network host for the menu-only virtual lobby. It intentionally spawns no gameplay pawn or HUD. */
UCLASS()
class GHOSTSVSTHIEVES_API AGvTVirtualLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGvTVirtualLobbyGameMode();
};
