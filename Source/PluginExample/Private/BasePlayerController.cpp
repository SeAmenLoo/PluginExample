#include "BasePlayerController.h"
#include "WorldSetupWidget.h"
#include "LoadingWidget.h"
#include "ChunkManagerComponent.h"
#include "PluginGameInstance.h"
#include "Kismet/GameplayStatics.h"

ABasePlayerController::ABasePlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableTouchEvents = true;
}

void ABasePlayerController::BeginPlay()
{
    Super::BeginPlay();

    PluginGameInstance = Cast<UPluginGameInstance>(GetGameInstance());
    if (PluginGameInstance)
    {
        ChunkManager = PluginGameInstance->GetChunkManager();
    }

    if (!WorldSetupWidgetClass)
    {
        if (PluginGameInstance)
        {
            WorldSetupWidgetClass = PluginGameInstance->WorldSetupWidgetClass;
        }
    }

    ShowWorldSetup();
}

void ABasePlayerController::ShowWorldSetup()
{
    if (WorldSetupWidget)
    {
        WorldSetupWidget->RemoveFromParent();
        WorldSetupWidget = nullptr;
    }

    if (WorldSetupWidgetClass)
    {
        WorldSetupWidget = CreateWidget<UWorldSetupWidget>(this, WorldSetupWidgetClass);
        if (WorldSetupWidget)
        {
            WorldSetupWidget->AddToViewport();
            WorldSetupWidget->OnWorldSetupComplete.AddDynamic(this, &ABasePlayerController::OnWorldSetupComplete);

            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(WorldSetupWidget->TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            SetInputMode(InputMode);
            bShowMouseCursor = true;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("WorldSetupWidgetClass not set in BasePlayerController"));
    }
}

void ABasePlayerController::HideWorldSetup()
{
    if (WorldSetupWidget)
    {
        WorldSetupWidget->RemoveFromParent();
        WorldSetupWidget = nullptr;
    }
}

void ABasePlayerController::OnWorldSetupComplete(const FString& WorldPrompt)
{
    HideWorldSetup();
    ShowLoadingScreen();

    if (ChunkManager)
    {
        ChunkManager->OnWorldGenerationComplete.AddDynamic(this, &ABasePlayerController::OnWorldGenerationComplete);
        ChunkManager->OnWorldGenerationFailed.AddDynamic(this, &ABasePlayerController::OnWorldGenerationFailed);
        ChunkManager->GenerateWorldFromPrompt(WorldPrompt);
    }
}

void ABasePlayerController::OnWorldGenerationComplete()
{
    HideLoadingScreen();

    if (ChunkManager)
    {
        ChunkManager->OnWorldGenerationComplete.RemoveDynamic(this, &ABasePlayerController::OnWorldGenerationComplete);
        ChunkManager->OnWorldGenerationFailed.RemoveDynamic(this, &ABasePlayerController::OnWorldGenerationFailed);
    }

    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
    bShowMouseCursor = false;
}

void ABasePlayerController::OnWorldGenerationFailed(const FString& Error)
{
    HideLoadingScreen();
    ShowWorldSetup();

    UE_LOG(LogTemp, Error, TEXT("World generation failed: %s"), *Error);
}

void ABasePlayerController::ShowLoadingScreen()
{
    if (LoadingWidget)
    {
        LoadingWidget->RemoveFromParent();
        LoadingWidget = nullptr;
    }

    if (!LoadingWidgetClass && PluginGameInstance)
    {
        UClass* FoundClass = PluginGameInstance->GetClass();
    }

    if (LoadingWidgetClass)
    {
        LoadingWidget = CreateWidget<ULoadingWidget>(this, LoadingWidgetClass);
        if (LoadingWidget)
        {
            LoadingWidget->SetLoadingText(TEXT("Generating world"));
            LoadingWidget->SetAnimatedDots(true);
            LoadingWidget->AddToViewport(10);

            FInputModeUIOnly InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            SetInputMode(InputMode);
            bShowMouseCursor = true;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("LoadingWidgetClass not set in BasePlayerController, using text log only"));
    }
}

void ABasePlayerController::HideLoadingScreen()
{
    if (LoadingWidget)
    {
        LoadingWidget->RemoveFromParent();
        LoadingWidget = nullptr;
    }
}
