#include "PluginGameInstance.h"
#include "WorldSetupWidget.h"
#include "ChunkManagerComponent.h"
#include "GameChunkActor.h"
#include "NPCCharacter.h"
#include "DialogueWidget.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "UObject/SoftObjectPath.h"

void UPluginGameInstance::Init()
{
    Super::Init();

    ChunkManager = GetSubsystem<UChunkManagerComponent>();

    if (ChunkManager)
    {
        if (ChunkActorClass)
        {
            ChunkManager->ChunkActorClass = ChunkActorClass;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ChunkActorClass not set in %s."), *GetName());
        }

        if (NPCCharacterClass)
        {
            ChunkManager->NPCCharacterClass = NPCCharacterClass;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("NPCCharacterClass not set in %s."), *GetName());
        }

        if (DialogueWidgetClass)
        {
            ChunkManager->DialogueWidgetClass = DialogueWidgetClass;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("DialogueWidgetClass not set in %s."), *GetName());
        }

        ChunkManager->ChunkSpacing = 2000.0f;

        ChunkManager->OnWorldGenerationComplete.AddDynamic(this, &UPluginGameInstance::OnWorldGenerationComplete);
        ChunkManager->OnWorldGenerationFailed.AddDynamic(this, &UPluginGameInstance::OnWorldGenerationFailed);
    }
}

void UPluginGameInstance::Shutdown()
{
    Super::Shutdown();
}

void UPluginGameInstance::StartGameWithPrompt(const FString& WorldPrompt)
{
    if (ChunkManager)
    {
        ChunkManager->GenerateWorldFromPrompt(WorldPrompt);
    }
}

FWorldConfig UPluginGameInstance::GetWorldConfig() const
{
    if (ChunkManager)
    {
        return ChunkManager->GetWorldConfig();
    }
    return FWorldConfig();
}

void UPluginGameInstance::OnWorldGenerationComplete()
{
    UE_LOG(LogTemp, Log, TEXT("World generation complete!"));

    CurrentWorldConfig = ChunkManager ? ChunkManager->GetWorldConfig() : FWorldConfig();
}

void UPluginGameInstance::OnWorldGenerationFailed(const FString& Error)
{
    UE_LOG(LogTemp, Error, TEXT("World generation failed: %s"), *Error);
}
