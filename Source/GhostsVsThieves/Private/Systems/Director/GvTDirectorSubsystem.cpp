#include "Systems/Director/GvTDirectorSubsystem.h"

void UGvTDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("GvT Director Subsystem Initialized"));
}

void UGvTDirectorSubsystem::Deinitialize()
{
    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("GvT Director Subsystem Deinitialized"));
}
