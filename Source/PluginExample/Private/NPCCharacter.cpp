#include "NPCCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "DialogueWidget.h"
#include "PaperFlipbookComponent.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "Engine/Texture2D.h"

ANPCCharacter::ANPCCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(150.0f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionObjectType(ECC_WorldStatic);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    NPCNameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NPCName"));
    NPCNameLabel->SetupAttachment(RootComponent);
    NPCNameLabel->SetRelativeLocation(FVector(0, 0, 120.0f));
    NPCNameLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    NPCNameLabel->SetVerticalAlignment(EVRTA_TextCenter);
    NPCNameLabel->SetTextRenderColor(FColor::White);
    NPCNameLabel->SetWorldSize(32.0f);

    InteractionPromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPrompt"));
    InteractionPromptWidget->SetupAttachment(RootComponent);
    InteractionPromptWidget->SetRelativeLocation(FVector(0, 0, 180.0f));
    InteractionPromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
    InteractionPromptWidget->SetDrawSize(FVector2D(200.0f, 50.0f));
    InteractionPromptWidget->SetVisibility(false);

    // Pixel-art flipbook (default visible, used for in-world display)
    PixelFlipbookComp = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("PixelFlipbookComp"));
    PixelFlipbookComp->SetupAttachment(RootComponent);
    PixelFlipbookComp->SetRelativeLocation(FVector(0, 0, 0.0f));
    PixelFlipbookComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PixelFlipbookComp->SetVisibility(true);
}

void ANPCCharacter::BeginPlay()
{
    Super::BeginPlay();

    InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ANPCCharacter::OnInteractionOverlap);
    InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ANPCCharacter::OnInteractionEndOverlap);
}

void ANPCCharacter::SetNPCData(const FNPCData& InData)
{
    NPCData = InData;

    NPCNameLabel->SetText(FText::FromString(InData.Name));
    SetActorLabel(InData.Name);
}

void ANPCCharacter::SetPixelFlipbook(UPaperFlipbook* InFlipbook)
{
    PixelFlipbook = InFlipbook;
    if (PixelFlipbookComp)
    {
        if (InFlipbook)
        {
            PixelFlipbookComp->SetFlipbook(InFlipbook);
            PixelFlipbookComp->SetVisibility(true);
            PixelFlipbookComp->Play();
        }
        else
        {
            PixelFlipbookComp->SetVisibility(false);
        }
    }
}

void ANPCCharacter::SetSpriteFromTexture(UTexture2D* InTexture, bool bUseAsPixelArt)
{
    if (PixelFlipbookComp && InTexture)
    {
        PixelFlipbookComp->SetVisibility(true);
    }
}

void ANPCCharacter::OnInteractionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA<APawn>() && !bPlayerInRange)
    {
        bPlayerInRange = true;
        UpdateInteractionPrompt();
        OnPlayerEnterRange.Broadcast(this, true);
    }
}

void ANPCCharacter::OnInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (OtherActor && OtherActor->IsA<APawn>())
    {
        bPlayerInRange = false;
        InteractionPromptWidget->SetVisibility(false);

        if (bIsInDialogue)
        {
            EndDialogue();
        }

        OnPlayerExitRange.Broadcast(this, false);
    }
}

void ANPCCharacter::OnInteractPressed()
{
    if (bPlayerInRange && !bIsInDialogue)
    {
        StartDialogue();
    }
}

void ANPCCharacter::StartDialogue()
{
    if (bIsInDialogue)
    {
        return;
    }

    bIsInDialogue = true;

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->DisableInput(PC);
    }

    UpdateInteractionPrompt();
}

void ANPCCharacter::EndDialogue()
{
    if (!bIsInDialogue)
    {
        return;
    }

    bIsInDialogue = false;

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->EnableInput(PC);
    }

    UpdateInteractionPrompt();
}

void ANPCCharacter::UpdateInteractionPrompt()
{
    if (bIsInDialogue)
    {
        InteractionPromptWidget->SetVisibility(false);
    }
    else
    {
        InteractionPromptWidget->SetVisibility(bPlayerInRange);
    }
}
