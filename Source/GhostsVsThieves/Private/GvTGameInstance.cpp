// Fill out your copyright notice in the Description page of Project Settings.


#include "GvTGameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

void UGvTGameInstance::Init()
{
	Super::Init();
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &ThisClass::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

void UGvTGameInstance::Shutdown()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Super::Shutdown();
}

void UGvTGameInstance::HandlePreLoadMap(const FString& MapName)
{
	if (!LoadingScreenWidgetClass || LoadingScreenWidget)
	{
		return;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		LoadingScreenWidget = CreateWidget<UUserWidget>(PC, LoadingScreenWidgetClass);
		if (LoadingScreenWidget)
		{
			LoadingScreenWidget->AddToViewport(10000);
		}
	}
}

void UGvTGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->RemoveFromParent();
		LoadingScreenWidget = nullptr;
	}
}

