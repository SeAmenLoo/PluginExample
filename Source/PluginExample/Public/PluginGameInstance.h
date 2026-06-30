#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameDataTypes.h"
#include "PluginGameInstance.generated.h"

class UWorldSetupWidget;
class UChunkManagerComponent;
class UUserWidget;
class AGameChunkActor;
class ANPCCharacter;
class UDialogueWidget;

UCLASS()
class PLUGINEXAMPLE_API UPluginGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;
    virtual void Shutdown() override;

    /** Start the game with world generation from prompt */
    UFUNCTION(BlueprintCallable)
    void StartGameWithPrompt(const FString& WorldPrompt);

    /** Get the chunk manager */
    UChunkManagerComponent* GetChunkManager() const { return ChunkManager; }

    UFUNCTION(BlueprintPure)
    FWorldConfig GetWorldConfig() const;

    /** World setup widget class - set in blueprint */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Config")
    TSubclassOf<UWorldSetupWidget> WorldSetupWidgetClass;

    /** Game chunk actor class - set in blueprint */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Config")
    TSubclassOf<class AGameChunkActor> ChunkActorClass;

    /** NPC character class - set in blueprint */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Config")
    TSubclassOf<class ANPCCharacter> NPCCharacterClass;

    /** Dialogue widget class - set in blueprint */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Config")
    TSubclassOf<class UDialogueWidget> DialogueWidgetClass;

protected:
    UFUNCTION()
    void OnWorldGenerationComplete();

    UFUNCTION()
    void OnWorldGenerationFailed(const FString& Error);

    void ShowGameHUD();
    void TransitionToGame();

private:
    UPROPERTY()
    TObjectPtr<UChunkManagerComponent> ChunkManager;

    UPROPERTY()
    FWorldConfig CurrentWorldConfig;
};
