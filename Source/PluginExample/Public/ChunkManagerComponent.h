#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameDataTypes.h"
#include "LLMRuntime/Public/LLMTypes.h"
#include "ChunkManagerComponent.generated.h"

class AGameChunkActor;
class ANPCCharacter;
class ULLMChatCompletionAsyncAction;
class UDialogueWidget;
class ULLMLoggerSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkLoaded, AGameChunkActor*, Chunk);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueStarted, ANPCCharacter*, NPC, const FString&, InitialText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWorldGenerationComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldGenerationFailed, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueEnded, ANPCCharacter*, NPC);

UCLASS()
class PLUGINEXAMPLE_API UChunkManagerComponent : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Generate initial world from prompt */
    UFUNCTION(BlueprintCallable)
    void GenerateWorldFromPrompt(const FString& WorldPrompt);

    /** Load an existing chunk by ID */
    UFUNCTION(BlueprintCallable)
    void LoadChunk(const FString& ChunkId);

    /** Get current chunk */
    UFUNCTION(BlueprintPure)
    AGameChunkActor* GetCurrentChunk() const { return CurrentChunk; }

    /** Get world config */
    UFUNCTION(BlueprintPure)
    FWorldConfig GetWorldConfig() const { return WorldConfig; }

    /** Get all loaded chunks */
    UFUNCTION(BlueprintPure)
    TArray<AGameChunkActor*> GetLoadedChunks() const;

    /** Get chunk name by ID */
    UFUNCTION(BlueprintCallable)
    FString GetChunkNameById(const FString& ChunkId) const;

    /** Update all existing chunks' portals when a new chunk is loaded */
    UFUNCTION(BlueprintCallable)
    void UpdateAllChunkPortals();

    /** Execute dialogue actions from NPC response */
    UFUNCTION(BlueprintCallable)
    void ExecuteDialogueActions(const TArray<FDialogueAction>& Actions);

    /** Start dialogue with NPC */
    UFUNCTION(BlueprintCallable)
    void StartDialogue(ANPCCharacter* NPC);

    /** End current dialogue */
    UFUNCTION(BlueprintCallable)
    void EndDialogue();

    /** Submit player text input and get NPC response */
    UFUNCTION(BlueprintCallable)
    void SubmitPlayerTextInput(const FString& InputText);

    /** Submit player choice selection and get NPC response */
    UFUNCTION(BlueprintCallable)
    void SubmitPlayerChoice(int32 ChoiceIndex);

    /** Called when player tries to exit current chunk */
    UFUNCTION(BlueprintCallable)
    void TryTransitionToNextChunk(AGameChunkActor* FromChunk, const FString& ExitId);

    /** Clear dialogue history for current NPC */
    UFUNCTION(BlueprintCallable)
    void ClearDialogueHistory();

    /** Get current dialogue message history */
    UFUNCTION(BlueprintPure)
    TArray<FLLMChatMessage> GetDialogueHistory() const { return DialogueMessages; }

    /** Events */
    UPROPERTY(BlueprintAssignable)
    FOnWorldGenerationComplete OnWorldGenerationComplete;

    UPROPERTY(BlueprintAssignable)
    FOnWorldGenerationFailed OnWorldGenerationFailed;

    UPROPERTY(BlueprintAssignable)
    FOnChunkLoaded OnChunkLoaded;

    UPROPERTY(BlueprintAssignable)
    FOnDialogueStarted OnDialogueStarted;

    UPROPERTY(BlueprintAssignable)
    FOnDialogueEnded OnDialogueEnded;

    /** Dialogue widget class to use */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<UDialogueWidget> DialogueWidgetClass;

    /** Chunk actor class to spawn */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<AGameChunkActor> ChunkActorClass;

    /** NPC character class to spawn */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<ANPCCharacter> NPCCharacterClass;

    /** Default world location for spawning chunks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldOrigin = FVector::ZeroVector;

    /** Spacing between chunks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChunkSpacing = 2000.0f;

    /** Maximum number of dialogue messages to keep in history (for token optimization) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxDialogueHistorySize = 20;

    /** Temperature for dialogue generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DialogueTemperature = 0.8f;

    /** Max tokens for dialogue response */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DialogueMaxTokens = 512;

    /** Thinking mode for LLM (default: Disabled for speed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM|Performance")
    ELLMThinkingMode LLMThinkingMode = ELLMThinkingMode::Disabled;

    /** Extra JSON body to pass to LLM API (provider-specific params) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM|Advanced")
    FString LLMExtraBodyJson;

    /** Enable local cache for AI responses (saves tokens on repeated runs) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM|Cache")
    bool bEnableCache = true;

    /** Use cached responses instead of real API calls (for testing/replay) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM|Cache")
    bool bUseCacheReplay = false;

protected:
    void OnInitialChunkGenerated(AGameChunkActor* Chunk);
    void GenerateNextChunk(const FString& FromChunkId, const FString& ExitId);
    void SpawnChunkAndNPCs(const FChunkData& ChunkData, const FVector& Location);

    void GenerateNPCDialogue();

private:
    UFUNCTION()
    void HandleWorldGenerationResponse(const FLLMChatResponse& Response);

    UFUNCTION()
    void HandleWorldGenStreamDelta(const FString& DeltaContent);

    void TryParseAndSpawnInitialChunk(const FString& CurrentContent);

    UFUNCTION()
    void HandleChunkGenerationResponse(const FLLMChatResponse& Response);

    UFUNCTION()
    void HandleDialogueResponse(const FLLMChatResponse& Response);

    void ParseInitialWorldResponse(const FString& JsonResponse);
    void ParseChunkGenerationResponse(const FString& JsonResponse, const FString& FromChunkId, const FString& ExitId);

    FString BuildWorldGenerationSystemPrompt() const;
    FString BuildChunkGenerationSystemPrompt() const;
    FString BuildDialogueSystemPrompt() const;

    /** Build context info about current game state */
    FString BuildDialogueContextInfo() const;

    /** Add a message to dialogue history */
    void AddDialogueMessage(const FString& Role, const FString& Content);

    /** Send dialogue request to LLM with full history */
    void SendDialogueRequest(bool bGenerateGreeting = false);

    UPROPERTY()
    AGameChunkActor* CurrentChunk;

    UPROPERTY()
    TMap<FString, AGameChunkActor*> LoadedChunks;

    UPROPERTY()
    FWorldConfig WorldConfig;

    UPROPERTY()
    ANPCCharacter* CurrentDialogueNPC;

    UPROPERTY()
    UDialogueWidget* ActiveDialogueWidget;

    /** Full dialogue message history for LLM context */
    UPROPERTY()
    TArray<FLLMChatMessage> DialogueMessages;

    /** Per-NPC dialogue history map, keyed by NPCId (not UPROPERTY - TArray cannot be TMap value) */
    TMap<FString, TArray<FLLMChatMessage>> NPCDialogueHistoryMap;

    /** Current NPC data for context */
    UPROPERTY()
    FNPCData CurrentNPCData;

    /** Chunk ID counter for generated chunks */
    int32 ChunkIdCounter = 0;

    /** NPC ID counter for generated NPCs */
    int32 NPCIdCounter = 0;

    /** Flag to track if waiting for LLM response */
    bool bIsWaitingForResponse = false;

    /** Flag to prevent concurrent chunk generation requests */
    bool bIsGeneratingChunk = false;

    /** First-time dialogue requests only need choices for the already displayed initial line. */
    bool bPendingInitialChoicesOnlyResponse = false;

    /** Accumulated content from streaming world generation */
    FString WorldGenStreamContent;

    /** Whether initial chunk has been spawned from stream */
    bool bInitialChunkSpawned = false;

    /** Store last NPC choices for selection handling */
    TArray<FString> LastNPChoices;

    /** Actions associated with each choice (by index) */
    TMap<int32, FDialogueAction> PendingChoiceActions;

    /** Temporary storage for chunk generation callback */
    FString PendingFromChunkId;
    FString PendingExitId;

    /** LLM Logger subsystem */
    UPROPERTY()
    TObjectPtr<class ULLMLoggerSubsystem> LLMLogger;

    /** LLM Cache subsystem for response caching */
    UPROPERTY()
    TObjectPtr<class ULLMCacheSubsystem> CacheSubsystem;

    /** Store pending request messages for cache saving */
    TArray<FLLMChatMessage> PendingRequestMessages;

    /** Store pending request type for cache saving */
    FString PendingRequestType;

    /** Store pending request context for cache saving */
    FString PendingRequestContext;

    /** Request timing */
    FDateTime WorldGenStartTime;
    FDateTime ChunkGenStartTime;
    FDateTime DialogueStartTime;
};
