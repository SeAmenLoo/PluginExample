#include "PortalActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

APortalActor::APortalActor()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
    PortalMesh->SetupAttachment(RootComponent);
    PortalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetupAttachment(RootComponent);
    TriggerBox->SetBoxExtent(FVector(50.0f, 80.0f, 120.0f));
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionObjectType(ECC_WorldStatic);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    PortalLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PortalLabel"));
    PortalLabel->SetupAttachment(RootComponent);
    PortalLabel->SetRelativeLocation(FVector(0, 0, 150.0f));
    PortalLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    PortalLabel->SetVerticalAlignment(EVRTA_TextCenter);
    PortalLabel->SetTextRenderColor(FColor::Cyan);
    PortalLabel->SetWorldSize(40.0f);
}

void APortalActor::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &APortalActor::OnTriggerBeginOverlap);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &APortalActor::OnTriggerEndOverlap);

    PortalLabel->SetText(FText::FromString(PortalId.IsEmpty() ? TEXT("EXIT") : *PortalId));
}

bool APortalActor::IsPlayerPawn(AActor* OtherActor) const
{
    if (!OtherActor)
    {
        return false;
    }

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn)
    {
        return false;
    }

    AController* Controller = Pawn->GetController();
    if (!Controller)
    {
        return false;
    }

    return Controller->IsA<APlayerController>();
}

void APortalActor::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!IsPlayerPawn(OtherActor))
    {
        return;
    }

    if (CanUsePortal())
    {
        OnPlayerEntered.Broadcast(this, OtherActor);
        OnPlayerEnterPortal(OtherActor);
    }
}

void APortalActor::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // Optional: handle when player leaves portal range
}

bool APortalActor::CanUsePortal() const
{
    return !bIsLocked;
}

void APortalActor::SetPortalLabel(const FString& LabelText)
{
    if (PortalLabel)
    {
        PortalLabel->SetText(FText::FromString(LabelText));
    }
}
