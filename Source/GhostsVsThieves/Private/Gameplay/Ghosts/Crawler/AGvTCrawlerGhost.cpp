#include "Gameplay/Ghosts/Crawler/AGvTCrawlerGhost.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"

AGvTCrawlerGhost::AGvTCrawlerGhost()
{
    PrimaryActorTick.bCanEverTick = true;

    bReplicates = true;
    SetReplicateMovement(true);

    Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
    RootComponent = Capsule;

    Capsule->InitCapsuleSize(34.f, 44.f);
    Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetIsReplicated(false);
}

void AGvTCrawlerGhost::BeginPlay()
{
    Super::BeginPlay();
    EnterState(State);
}

void AGvTCrawlerGhost::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority())
    {
        return;
    }

    switch (State)
    {
    case EGvTCrawlerGhostState::DropScare:
    {
        DropT += DeltaSeconds;
        const float Alpha = FMath::Clamp(DropT / FMath::Max(0.01f, DropDuration), 0.f, 1.f);

        const FVector NewLoc = FMath::Lerp(DropStart, DropEnd, Alpha);
        SetActorLocation(NewLoc, false);

        if (Alpha >= 1.f)
        {
            // Scripted scare ends -> vanish (MVP)
            Server_Vanish();
        }
        break;
    }

    case EGvTCrawlerGhostState::HauntChase:
    {
        if (!IsValid(TargetVictim))
        {
            Server_Vanish();
            break;
        }

        const FVector MyLoc = GetActorLocation();
        const FVector TargetLoc = TargetVictim->GetActorLocation();

        const float Dist = FVector::Dist(MyLoc, TargetLoc);
        if (Dist > MaxChaseDistance)
        {
            Server_Vanish();
            break;
        }

        // Rotate toward target (yaw only)
        FVector ToTarget = TargetLoc - MyLoc;
        ToTarget.Z = 0.f;

        if (!ToTarget.IsNearlyZero())
        {
            const FRotator WantRot = ToTarget.Rotation();
            const FRotator NewRot = FMath::RInterpConstantTo(GetActorRotation(), WantRot, DeltaSeconds, TurnSpeedDegPerSec);
            SetActorRotation(NewRot);
        }

        // Lunge pacing
        LungeTimer += DeltaSeconds;
        if (LungeTimer >= LungeInterval)
        {
            LungeTimer = 0.f;
            bIsLunging = true;
        }

        if (bIsLunging)
        {
            const FVector Dir = ToTarget.GetSafeNormal();
            const FVector Step = Dir * (LungeSpeed * DeltaSeconds);

            SetActorLocation(MyLoc + Step, true);

            // End lunge quickly so it feels like pounces
            if (LungeTimer >= 0.12f)
            {
                bIsLunging = false;
            }
        }

        TryKillVictim();
        break;
    }

    case EGvTCrawlerGhostState::IdleCeiling:
    case EGvTCrawlerGhostState::Vanish:
    default:
        break;
    }
}

void AGvTCrawlerGhost::TryKillVictim()
{
    if (!HasAuthority() || !IsValid(TargetVictim))
    {
        return;
    }

    if (FVector::Dist(GetActorLocation(), TargetVictim->GetActorLocation()) <= KillDistance)
    {
        if (AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(TargetVictim))
        {
            // We’ll add this function to Thief below
            Thief->Server_SetDead(this);
        }

        Server_Vanish();
    }
}

void AGvTCrawlerGhost::Server_StartDropScare_Implementation(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly)
{
    // MVP: visible to everyone, so we ignore bVictimOnly.
    TargetVictim = Victim;

    SetActorRotation(FaceRot);

    DropStart = CeilingWorldPos;
    DropEnd = CeilingWorldPos - FVector(0.f, 0.f, DropOffsetDown);
    DropT = 0.f;

    SetActorLocation(DropStart, false);

    EnterState(EGvTCrawlerGhostState::DropScare);
}

void AGvTCrawlerGhost::Server_StartHauntChase_Implementation(AActor* Victim)
{
    TargetVictim = Victim;

    LungeTimer = 0.f;
    bIsLunging = false;

    EnterState(EGvTCrawlerGhostState::HauntChase);
}

void AGvTCrawlerGhost::Server_Vanish_Implementation()
{
    EnterState(EGvTCrawlerGhostState::Vanish);

    Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetVisibility(false, true);

    SetLifeSpan(1.5f);
}

void AGvTCrawlerGhost::EnterState(EGvTCrawlerGhostState NewState)
{
    const EGvTCrawlerGhostState Prev = State;
    State = NewState;
    OnRep_State(Prev); // run same logic on server
}

void AGvTCrawlerGhost::OnRep_State(EGvTCrawlerGhostState PrevState)
{
    // Keep lightweight: AnimBP reads State via GetState().
    // Later: play sounds/VFX here.
}

void AGvTCrawlerGhost::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AGvTCrawlerGhost, State);
    DOREPLIFETIME(AGvTCrawlerGhost, TargetVictim);
}