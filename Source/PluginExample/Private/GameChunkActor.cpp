#include "GameChunkActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "NPCCharacter.h"
#include "ChunkManagerComponent.h"
#include "PortalActor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

AGameChunkActor::AGameChunkActor()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    ChunkMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChunkMesh"));
    ChunkMesh->SetupAttachment(RootComponent);
    ChunkMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    // Ground plane lies flat on the XY plane (Plane mesh default normal is +Z).
    ChunkMesh->SetRelativeRotation(FRotator::ZeroRotator);
    ChunkMesh->SetRelativeLocation(FVector::ZeroVector);

    ChunkNameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ChunkName"));
    ChunkNameLabel->SetupAttachment(RootComponent);
    ChunkNameLabel->SetRelativeLocation(FVector(0, 0, 400.0f));
    ChunkNameLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    ChunkNameLabel->SetVerticalAlignment(EVRTA_TextCenter);
    ChunkNameLabel->SetTextRenderColor(FColor::White);
    ChunkNameLabel->SetWorldSize(60.0f);

    ChunkSize = 1000.0f;

    // Default to using PortalActor class
    static ConstructorHelpers::FClassFinder<APortalActor> PortalClassFinder(TEXT("/Game/Blueprints/BP_PortalActor.BP_PortalActor_C"));
    if (PortalClassFinder.Succeeded())
    {
        PortalClass = PortalClassFinder.Class;
    }
}

void AGameChunkActor::BeginPlay()
{
    Super::BeginPlay();

    // Load ground plane mesh and scale it to cover the whole chunk.
    // Engine BasicShapes/Plane is 100x100 units, so scale = ChunkSize / 100.
    UStaticMesh* PlaneMesh = Cast<UStaticMesh>(GroundMeshAsset.TryLoad());
    if (PlaneMesh)
    {
        ChunkMesh->SetStaticMesh(PlaneMesh);
        const float PlaneScale = ChunkSize / 100.0f;
        ChunkMesh->SetRelativeScale3D(FVector(PlaneScale, PlaneScale, 1.0f));
        ChunkMesh->SetRelativeLocation(FVector::ZeroVector);
        ChunkMesh->SetRelativeRotation(FRotator::ZeroRotator);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameChunkActor] Failed to load ground plane mesh from %s"), *GroundMeshAsset.ToString());
    }

    // Create dynamic material instance for runtime texture switching.
    UMaterialInterface* BaseMat = Cast<UMaterialInterface>(GroundBaseMaterial.TryLoad());
    if (BaseMat)
    {
        GroundMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
        ChunkMesh->SetMaterial(0, GroundMaterial);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameChunkActor] Failed to load ground base material from %s"), *GroundBaseMaterial.ToString());
    }

    RebuildExitPortals();
}

void AGameChunkActor::SetChunkData(const FChunkData& InData)
{
    ChunkData = InData;
    ChunkSize = InData.ChunkSize;

    ChunkNameLabel->SetText(FText::FromString(InData.Name));

    SetActorLabel(InData.Name);

    RebuildExitPortals();
}

FVector AGameChunkActor::ComputePortalLocation(float ExitDirection) const
{
    float HalfSize = ChunkSize * 0.5f - PortalEdgeOffset;
    float Rad = FMath::DegreesToRadians(ExitDirection);
    FVector Dir = FRotationMatrix(FRotator(0, ExitDirection, 0)).GetScaledAxis(EAxis::X);

    float T = HALF_WORLD_MAX;
    if (FMath::Abs(Dir.X) > KINDA_SMALL_NUMBER)
    {
        T = FMath::Min(T, HalfSize / FMath::Abs(Dir.X));
    }
    if (FMath::Abs(Dir.Y) > KINDA_SMALL_NUMBER)
    {
        T = FMath::Min(T, HalfSize / FMath::Abs(Dir.Y));
    }

    FVector Location = Dir * T;
    Location.Z = SpawnZOffset;
    return Location;
}

FVector AGameChunkActor::ComputeNPCLocation(int32 NPCIndex, int32 TotalNPCs) const
{
    if (TotalNPCs <= 0)
    {
        return FVector::ZeroVector;
    }

    float TotalWidth = (TotalNPCs - 1) * NPCSpacing;
    float StartX = -TotalWidth * 0.5f;
    float X = StartX + NPCIndex * NPCSpacing;

    return FVector(X, 0.0f, SpawnZOffset);
}

FTransform AGameChunkActor::GetPlayerStartTransformFromExit(float EntryExitDirection) const
{
    float OppositeDirection = EntryExitDirection + 180.0f;
    FVector EntryLocation = ComputePortalLocation(OppositeDirection);
    const FVector PortalOutwardDirection = FRotationMatrix(FRotator(0, OppositeDirection, 0)).GetScaledAxis(EAxis::X).GetSafeNormal();
    const FVector SpawnLocation = GetActorLocation() + EntryLocation - (PortalOutwardDirection * PlayerSpawnOffsetFromPortal);

    FTransform StartTransform;
    StartTransform.SetLocation(SpawnLocation);

    FRotator Rotator = FRotator(0, EntryExitDirection, 0);
    StartTransform.SetRotation(Rotator.Quaternion());

    return StartTransform;
}

void AGameChunkActor::RebuildExitPortals()
{
    for (APortalActor* Portal : ExitPortals)
    {
        if (Portal)
        {
            Portal->Destroy();
        }
    }
    ExitPortals.Empty();

    TSet<FString> CreatedTargetChunks;

    // Only create portals for exits that have valid TargetChunkId
    for (const FExitPoint& Exit : ChunkData.ExitPoints)
    {
        if (!PortalClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("PortalClass not set, skipping portal creation for exit: %s"), *Exit.ExitId);
            continue;
        }

        // Only create portal if TargetChunkId is set (meaning the destination chunk exists)
        if (Exit.TargetChunkId.IsEmpty())
        {
            UE_LOG(LogTemp, Verbose, TEXT("Skipping portal for exit %s: no target chunk assigned yet"), *Exit.ExitId);
            continue;
        }

        if (CreatedTargetChunks.Contains(Exit.TargetChunkId))
        {
            UE_LOG(LogTemp, Warning, TEXT("Skipping duplicate portal for exit %s: target %s already has a portal on this chunk"),
                *Exit.ExitId, *Exit.TargetChunkId);
            continue;
        }
        CreatedTargetChunks.Add(Exit.TargetChunkId);

        // Check if the target chunk is actually loaded
        if (UChunkManagerComponent* Manager = UGameplayStatics::GetGameInstance(this)->GetSubsystem<UChunkManagerComponent>())
        {
            if (!Manager->GetLoadedChunks().ContainsByPredicate([&Exit](AGameChunkActor* Chunk)
            {
                return Chunk && Chunk->GetChunkId() == Exit.TargetChunkId;
            }))
            {
                UE_LOG(LogTemp, Verbose, TEXT("Skipping portal for exit %s: target chunk %s not loaded yet"), *Exit.ExitId, *Exit.TargetChunkId);
                continue;
            }
        }

        // Create the portal
        CreatePortalForExit(Exit);
    }
}

void AGameChunkActor::CreatePortalForExit(const FExitPoint& Exit)
{
    if (!PortalClass || Exit.TargetChunkId.IsEmpty())
    {
        return;
    }

    // Check if portal already exists
    if (ExitPortals.ContainsByPredicate([&Exit](APortalActor* Portal)
    {
        return Portal && Portal->PortalId == Exit.ExitId;
    }))
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = this;

    FVector RelativePortalLoc = ComputePortalLocation(Exit.ExitDirection);
    FVector PortalLocation = GetActorLocation() + RelativePortalLoc;
    FRotator PortalRotation = FRotator(0, 0, 0);

    APortalActor* NewPortal = GetWorld()->SpawnActor<APortalActor>(PortalClass, PortalLocation, PortalRotation, SpawnParams);
    if (NewPortal)
    {
        NewPortal->PortalId = Exit.ExitId;
        NewPortal->ExitDirection = Exit.ExitDirection;
        NewPortal->TargetChunkId = Exit.TargetChunkId;
        NewPortal->bIsLocked = Exit.bIsLocked;
        NewPortal->UnlockCondition = Exit.UnlockCondition;

        // Set portal label to show target chunk name
        if (UChunkManagerComponent* Manager = UGameplayStatics::GetGameInstance(this)->GetSubsystem<UChunkManagerComponent>())
        {
            FString TargetChunkName = Manager->GetChunkNameById(Exit.TargetChunkId);
            if (!TargetChunkName.IsEmpty())
            {
                NewPortal->SetPortalLabel(TargetChunkName);
            }
        }

        NewPortal->OnPlayerEntered.AddDynamic(this, &AGameChunkActor::HandlePortalEntered);

        ExitPortals.Add(NewPortal);
        UE_LOG(LogTemp, Log, TEXT("Created portal for exit %s -> %s"), *Exit.ExitId, *Exit.TargetChunkId);

        // Push player away if they're too close to the newly spawned portal
        if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
        {
            FVector PlayerLoc = PlayerPawn->GetActorLocation();
            float DistToPortal = FVector::Dist(PlayerLoc, PortalLocation);
            const float SafeDistance = 150.0f;

            if (DistToPortal < SafeDistance)
            {
                FVector PushDir = (PlayerLoc - PortalLocation).GetSafeNormal();
                if (PushDir.IsNearlyZero())
                {
                    // Player is exactly at portal center - push towards chunk center
                    PushDir = (GetActorLocation() - PortalLocation).GetSafeNormal();
                }
                FVector NewPlayerLoc = PortalLocation + PushDir * SafeDistance;
                NewPlayerLoc.Z = PlayerLoc.Z;
                PlayerPawn->SetActorLocation(NewPlayerLoc, false, nullptr, ETeleportType::TeleportPhysics);
                UE_LOG(LogTemp, Log, TEXT("Pushed player away from new portal %s (was %.0f units away)"),
                    *Exit.ExitId, DistToPortal);
            }
        }
    }
}

void AGameChunkActor::CreatePortalForExitById(const FString& ExitId)
{
    for (const FExitPoint& Exit : ChunkData.ExitPoints)
    {
        if (Exit.ExitId == ExitId)
        {
            CreatePortalForExit(Exit);
            break;
        }
    }
}

void AGameChunkActor::UpdateExitTargetChunkId(const FString& ExitId, const FString& TargetChunkId)
{
    for (FExitPoint& Exit : ChunkData.ExitPoints)
    {
        if (Exit.ExitId == ExitId)
        {
            Exit.TargetChunkId = TargetChunkId;
            UE_LOG(LogTemp, Log, TEXT("Updated exit %s -> %s in chunk %s"), *ExitId, *TargetChunkId, *ChunkData.Name);
            break;
        }
    }
}

void AGameChunkActor::SetReverseExitTarget(const FString& SourceChunkId, float EntryDirection)
{
    float ReverseDirection = FMath::Fmod(EntryDirection + 180.0f, 360.0f);
    if (ReverseDirection < 0.0f)
    {
        ReverseDirection += 360.0f;
    }

    for (FExitPoint& Exit : ChunkData.ExitPoints)
    {
        float ExitDir = Exit.ExitDirection;
        // Normalize to 0-360 range
        while (ExitDir < 0.0f) ExitDir += 360.0f;
        while (ExitDir >= 360.0f) ExitDir -= 360.0f;

        // Check if this exit is in the reverse direction (with small tolerance)
        float DirectionDiff = FMath::Abs(ExitDir - ReverseDirection);
        if (DirectionDiff > 180.0f)
        {
            DirectionDiff = 360.0f - DirectionDiff;
        }

        if (DirectionDiff < 5.0f && Exit.TargetChunkId.IsEmpty())
        {
            Exit.TargetChunkId = SourceChunkId;
            UE_LOG(LogTemp, Log, TEXT("Set reverse exit %s -> %s in chunk %s (reverse of %s)"),
                *Exit.ExitId, *SourceChunkId, *ChunkData.Name, *SourceChunkId);
            break;
        }
    }
}

FTransform AGameChunkActor::GetPlayerStartTransform() const
{
    if (ChunkData.ExitPoints.Num() > 0)
    {
        const FExitPoint& EntryExit = ChunkData.ExitPoints[0];
        return GetPlayerStartTransformFromExit(EntryExit.ExitDirection);
    }

    return GetActorTransform();
}

void AGameChunkActor::SetGroundTexture(UTexture2D* InTexture)
{
    CurrentGroundTexture = InTexture;
    if (GroundMaterial)
    {
        GroundMaterial->SetTextureParameterValue(GroundTextureParameterName, InTexture);
        if (InTexture)
        {
            // Enable seamless wrapping so the texture tiles cleanly across the ground plane.
            InTexture->AddressX = TA_Wrap;
            InTexture->AddressY = TA_Wrap;
            InTexture->UpdateResource();
            ChunkMesh->SetVisibility(true);
        }
        else
        {
            ChunkMesh->SetVisibility(false);
        }
    }
}

void AGameChunkActor::HandlePortalEntered(APortalActor* Portal, AActor* OtherActor)
{
    if (!Portal || !OtherActor)
    {
        return;
    }

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn)
    {
        return;
    }

    AController* Controller = Pawn->GetController();
    if (!Controller || !Controller->IsA<APlayerController>())
    {
        return;
    }

    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (CurrentTime - LastExitTriggerTime < ExitTriggerCooldown)
    {
        return;
    }
    LastExitTriggerTime = CurrentTime;

    if (UChunkManagerComponent* Manager = UGameplayStatics::GetGameInstance(this)->GetSubsystem<UChunkManagerComponent>())
    {
        Manager->TryTransitionToNextChunk(this, Portal->PortalId);
    }
}
