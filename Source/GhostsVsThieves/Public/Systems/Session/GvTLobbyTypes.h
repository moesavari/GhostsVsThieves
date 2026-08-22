#pragma once

#include "CoreMinimal.h"
#include "GvTLobbyTypes.generated.h"

UENUM(BlueprintType)
enum class EGvTPlayableMap : uint8
{
	MVPHouse UMETA(DisplayName = "MVP House"),
	ModernVilla UMETA(DisplayName = "Modern Villa")
};

USTRUCT(BlueprintType)
struct FGvTLobbyPlayerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Sessions|Lobby")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Sessions|Lobby")
	bool bReady = false;
};
