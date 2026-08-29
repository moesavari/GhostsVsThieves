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
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;

private:
	int32 PendingInitialMapValue = 0;
};
