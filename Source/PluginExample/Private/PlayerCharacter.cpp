#include "PlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "NPCCharacter.h"
#include "ChunkManagerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

APlayerCharacter::APlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 90.0f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    GetCharacterMovement()->JumpZVelocity = 600.f;
    GetCharacterMovement()->AirControl = 0.2f;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 300.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    CharacterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CharacterMesh"));
    CharacterMesh->SetupAttachment(RootComponent);
    //CharacterMesh->SetRelativeLocation(FVector(0, 0, -90.0f));
    CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CharacterMesh->SetVisibility(true);

    NearbyNPC = nullptr;
    InteractionRadius = 200.0f;
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    UStaticMesh* PlaneMesh = Cast<UStaticMesh>(PlaneMeshAsset.TryLoad());
    if (PlaneMesh)
    {
        CharacterMesh->SetStaticMesh(PlaneMesh);
        CharacterMesh->SetRelativeScale3D(PlaneScale);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load plane mesh from %s"), *PlaneMeshAsset.ToString());
    }

    UMaterialInterface* BaseMat = Cast<UMaterialInterface>(BaseMaterial.TryLoad());
    if (BaseMat)
    {
        CharacterMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
        CharacterMesh->SetMaterial(0, CharacterMaterial);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load base material from %s"), *BaseMaterial.ToString());
    }
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (InputMappingContext)
        {
            if (APlayerController* PC = Cast<APlayerController>(GetController()))
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    Subsystem->AddMappingContext(InputMappingContext, 0);
                }
            }
        }

        if (InputActionJump)
        {
            EnhancedInputComponent->BindAction(InputActionJump, ETriggerEvent::Started, this, &APlayerCharacter::OnJumpStarted);
            EnhancedInputComponent->BindAction(InputActionJump, ETriggerEvent::Completed, this, &APlayerCharacter::OnJumpEnded);
        }

        if (InputActionMoveForward)
        {
            EnhancedInputComponent->BindAction(InputActionMoveForward, ETriggerEvent::Triggered, this, &APlayerCharacter::OnMoveForward);
        }

        if (InputActionMoveRight)
        {
            EnhancedInputComponent->BindAction(InputActionMoveRight, ETriggerEvent::Triggered, this, &APlayerCharacter::OnMoveRight);
        }

        if (InputActionInteract)
        {
            EnhancedInputComponent->BindAction(InputActionInteract, ETriggerEvent::Started, this, &APlayerCharacter::OnInteract);
        }
    }
}

void APlayerCharacter::MoveForward(float Value)
{
    if ((Controller != nullptr) && (Value != 0.0f))
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        AddMovementInput(Direction, Value);
    }
}

void APlayerCharacter::MoveRight(float Value)
{
    if ((Controller != nullptr) && (Value != 0.0f))
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        AddMovementInput(Direction, Value);
    }
}

void APlayerCharacter::OnJumpStarted(const FInputActionValue& Value)
{
    Jump();
}

void APlayerCharacter::OnJumpEnded(const FInputActionValue& Value)
{
    StopJumping();
}

void APlayerCharacter::OnMoveForward(const FInputActionValue& Value)
{
    const FVector2D MoveInput = Value.Get<FVector2D>();
    MoveForward(MoveInput.X);
}

void APlayerCharacter::OnMoveRight(const FInputActionValue& Value)
{
    const FVector2D MoveInput = Value.Get<FVector2D>();
    MoveRight(MoveInput.Y);
}

void APlayerCharacter::OnInteract(const FInputActionValue& Value)
{
    OnInteractPressed();
}

void APlayerCharacter::OnInteractPressed()
{
    UE_LOG(LogTemp, Verbose, TEXT("Interact pressed"));

    CheckNearbyNPCs();

    if (!NearbyNPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("No nearby NPC found. InteractionRadius=%.0f"), InteractionRadius);
    }

    if (NearbyNPC)
    {
        UE_LOG(LogTemp, Log, TEXT("Interacting with NPC: %s"), *NearbyNPC->GetNPCData().Name);

        if (UChunkManagerComponent* ChunkMgr = GetWorld()->GetGameInstance()->GetSubsystem<UChunkManagerComponent>())
        {
            ChunkMgr->StartDialogue(NearbyNPC);
        }
    }
}

void APlayerCharacter::SetSpriteFromTexture(UTexture2D* InTexture)
{
    CurrentPixelTexture = InTexture;
    if (CharacterMaterial)
    {
        CharacterMaterial->SetTextureParameterValue(TextureParameterName, InTexture);
        if (InTexture)
        {
            CharacterMesh->SetVisibility(true);
        }
        else
        {
            CharacterMesh->SetVisibility(false);
        }
    }
}

void APlayerCharacter::SetWalkFrames(UTexture2D* Frame1, UTexture2D* Frame2)
{
    WalkFrameTextures.Empty();
    if (Frame1) WalkFrameTextures.Add(Frame1);
    if (Frame2) WalkFrameTextures.Add(Frame2);
}

void APlayerCharacter::CheckNearbyNPCs()
{
    NearbyNPC = nullptr;

    TArray<AActor*> OverlappingActors;
    FVector Location = GetActorLocation();
    float Radius = InteractionRadius;

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    TArray<FHitResult> HitResults;
    UKismetSystemLibrary::SphereTraceMultiForObjects(
        this,
        Location,
        Location + FVector(0, 0, 1),
        Radius,
        ObjectTypes,
        false,
        ActorsToIgnore,
        EDrawDebugTrace::None,
        HitResults,
        true
    );

    float ClosestDistance = Radius;

    for (const FHitResult& Hit : HitResults)
    {
        if (ANPCCharacter* NPC = Cast<ANPCCharacter>(Hit.GetActor()))
        {
            float Distance = FVector::Dist(Location, NPC->GetActorLocation());
            if (Distance < ClosestDistance)
            {
                ClosestDistance = Distance;
                NearbyNPC = NPC;
            }
        }
    }
}

void APlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    CheckNearbyNPCs();

    // Simple walk animation: switch between 2 frames when moving
    if (WalkFrameTextures.Num() >= 2 && CharacterMaterial)
    {
        const FVector Velocity = GetVelocity();
        const bool bIsMoving = Velocity.SizeSquared2D() > 10.0f;

        if (bIsMoving)
        {
            // Use timer to cycle frames every 0.3 seconds
            if (!GetWorld()->GetTimerManager().IsTimerActive(WalkAnimTimer))
            {
                GetWorld()->GetTimerManager().SetTimer(WalkAnimTimer, [this]()
                {
                    if (WalkFrameTextures.Num() >= 2 && CharacterMaterial)
                    {
                        CurrentWalkFrame = (CurrentWalkFrame + 1) % WalkFrameTextures.Num();
                        CharacterMaterial->SetTextureParameterValue(TextureParameterName, WalkFrameTextures[CurrentWalkFrame]);
                    }
                }, 0.3f, true);
            }
        }
        else
        {
            // Stop animation, show idle frame (first frame or current texture)
            GetWorld()->GetTimerManager().ClearTimer(WalkAnimTimer);
            if (CurrentPixelTexture && CharacterMaterial)
            {
                CharacterMaterial->SetTextureParameterValue(TextureParameterName, CurrentPixelTexture);
            }
        }
    }
}
