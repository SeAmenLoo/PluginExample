#include "ChunkManagerComponent.h"
#include "GameChunkActor.h"
#include "NPCCharacter.h"
#include "DialogueWidget.h"
#include "CharacterImageSubsystem.h"
#include "LLMLoggerSubsystem.h"
#include "LLMRuntime/Public/LLMCacheSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blueprint/UserWidget.h"
#include "JsonObjectConverter.h"
#include "LLMRuntime/Public/LLMChatCompletionAsyncAction.h"

namespace
{
    float SnapDirectionToCardinal(float Direction)
    {
        while (Direction < 0.0f) Direction += 360.0f;
        while (Direction >= 360.0f) Direction -= 360.0f;

        const float CardinalDirections[4] = { 0.0f, 90.0f, 180.0f, 270.0f };
        float BestDir = 0.0f;
        float BestDiff = 360.0f;

        for (float CardDir : CardinalDirections)
        {
            float Diff = FMath::Abs(Direction - CardDir);
            if (Diff > 180.0f) Diff = 360.0f - Diff;
            if (Diff < BestDiff)
            {
                BestDiff = Diff;
                BestDir = CardDir;
            }
        }
        return BestDir;
    }
}

void UChunkManagerComponent::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ChunkIdCounter = 0;
    NPCIdCounter = 0;
    CurrentChunk = nullptr;
    CurrentDialogueNPC = nullptr;
    ActiveDialogueWidget = nullptr;
    bIsWaitingForResponse = false;

    LLMLogger = GetGameInstance()->GetSubsystem<ULLMLoggerSubsystem>();
    CacheSubsystem = GetGameInstance()->GetSubsystem<ULLMCacheSubsystem>();

    if (UCharacterImageSubsystem* ImageSubsystem = GetGameInstance()->GetSubsystem<UCharacterImageSubsystem>())
    {
        ImageSubsystem->OnCharacterImageReady.AddDynamic(this, &UChunkManagerComponent::HandleCharacterImageReady);
    }

    UE_LOG(LogTemp, Log, TEXT("[ChunkManager] Initialize - CacheSubsystem: %p, bUseCacheReplay: %d"), 
        CacheSubsystem.Get(), bUseCacheReplay);
    if (CacheSubsystem)
    {
        UE_LOG(LogTemp, Log, TEXT("[ChunkManager] Initialize - Subsystem bReplayMode: %d, EntryCount: %d"), 
            CacheSubsystem->IsReplayModeEnabled(), CacheSubsystem->GetCacheEntryCount());
    }
}

void UChunkManagerComponent::Deinitialize()
{
    if (UCharacterImageSubsystem* ImageSubsystem = GetGameInstance()->GetSubsystem<UCharacterImageSubsystem>())
    {
        ImageSubsystem->OnCharacterImageReady.RemoveDynamic(this, &UChunkManagerComponent::HandleCharacterImageReady);
    }

    if (ActiveDialogueWidget)
    {
        ActiveDialogueWidget->RemoveFromParent();
        ActiveDialogueWidget = nullptr;
    }

    Super::Deinitialize();
}

bool UChunkManagerComponent::IsReplayModeActive() const
{
    // Component's own setting takes priority
    if (bUseCacheReplay)
    {
        return true;
    }
    // Fall back to subsystem's setting
    if (CacheSubsystem && CacheSubsystem->IsReplayModeEnabled())
    {
        return true;
    }
    return false;
}

void UChunkManagerComponent::GenerateWorldFromPrompt(const FString& WorldPrompt)
{
    FString ActualPrompt = WorldPrompt;

    // In replay mode, auto-fill WorldPrompt from cache if user didn't provide one
    if (IsReplayModeActive() && WorldPrompt.IsEmpty() && CacheSubsystem)
    {
        FLLMCacheEntry WorldGenEntry;
        if (CacheSubsystem->GetLastWorldGenCacheEntry(WorldGenEntry))
        {
            // Extract WorldPrompt from cached request messages
            for (const FLLMChatMessage& Msg : WorldGenEntry.RequestMessages)
            {
                if (Msg.Role == TEXT("user"))
                {
                    ActualPrompt = Msg.Content;
                    UE_LOG(LogTemp, Log, TEXT("[Cache] Replay mode: using cached WorldPrompt"));
                    break;
                }
            }
        }
    }

    WorldConfig.OriginalPrompt = ActualPrompt;

    FString SystemPrompt = BuildWorldGenerationSystemPrompt();

    TArray<FLLMChatMessage> Messages;
    Messages.Add(FLLMChatMessage{TEXT("system"), SystemPrompt});
    Messages.Add(FLLMChatMessage{TEXT("user"), ActualPrompt});

    UE_LOG(LogTemp, Log, TEXT("[Cache] bEnableCache: %d, CacheSubsystem: %p, IsReplayMode: %d"), 
        bEnableCache, CacheSubsystem.Get(), IsReplayModeActive());

    if (bEnableCache && CacheSubsystem)
    {
        const FString RequestHash = CacheSubsystem->GenerateRequestHash(Messages);
        UE_LOG(LogTemp, Log, TEXT("[Cache] Request hash: %s"), *RequestHash);
    }

    // Check cache first - WorldGen uses type-based matching (latest WorldGen entry)
    if (bEnableCache && CacheSubsystem)
    {
        FLLMCacheEntry CachedEntry;
        if (CacheSubsystem->GetLastWorldGenCacheEntry(CachedEntry))
        {
            UE_LOG(LogTemp, Log, TEXT("[Cache] World generation cache hit (type match)! Replay mode: %d"), IsReplayModeActive());

            if (IsReplayModeActive())
            {
                FLLMChatResponse CachedResponse;
                CachedResponse.bSuccess = CachedEntry.bWasSuccessful;
                CachedResponse.Content = CachedEntry.ResponseContent;
                CachedResponse.Json = CachedEntry.ResponseJson;
                CachedResponse.Error = CachedEntry.bWasSuccessful ? TEXT("") : TEXT("Cached error response");

                WorldGenStartTime = FDateTime::UtcNow();
                WorldGenStreamContent.Empty();
                bInitialChunkSpawned = false;

                if (!CachedEntry.ResponseContent.IsEmpty())
                {
                    HandleWorldGenStreamDelta(CachedEntry.ResponseContent);
                }
                HandleWorldGenerationResponse(CachedResponse);
                return;
            }
            else if (CachedEntry.bWasSuccessful)
            {
                UE_LOG(LogTemp, Log, TEXT("[Cache] Using cached response (will still call API for comparison)"));
            }
        }
        else if (IsReplayModeActive())
        {
            UE_LOG(LogTemp, Error, TEXT("[Cache] Replay mode enabled but world generation cache miss! Aborting request."));
            OnWorldGenerationFailed.Broadcast(TEXT("Replay mode: no cached response found for world generation"));
            return;
        }
    }

    FLLMChatRequest Request;
    Request.Messages = Messages;
    Request.Temperature = 0.7f;
    Request.MaxTokens = 4096;
    Request.bForceJsonObject = true;
    Request.TimeoutSeconds = 120.0f;
    Request.bStream = true;
    Request.ThinkingMode = LLMThinkingMode;
    Request.ExtraBodyJson = LLMExtraBodyJson;

    // Store messages for potential cache save
    PendingRequestMessages = Messages;
    PendingRequestType = TEXT("WorldGen");
    PendingRequestContext = WorldPrompt;

    if (LLMLogger)
    {
        LLMLogger->LogRequest(ELLMRequestType::WorldGeneration, Request, WorldConfig.WorldName);
    }
    WorldGenStartTime = FDateTime::UtcNow();

    WorldGenStreamContent.Empty();
    bInitialChunkSpawned = false;

    ULLMChatCompletionAsyncAction* Action = ULLMChatCompletionAsyncAction::ChatCompletion(this, Request);
    Action->OnDelta.AddDynamic(this, &UChunkManagerComponent::HandleWorldGenStreamDelta);
    Action->Completed.AddDynamic(this, &UChunkManagerComponent::HandleWorldGenerationResponse);
    Action->Activate();
}

void UChunkManagerComponent::HandleWorldGenStreamDelta(const FString& DeltaContent)
{
    WorldGenStreamContent += DeltaContent;

    if (!bInitialChunkSpawned)
    {
        TryParseAndSpawnInitialChunk(WorldGenStreamContent);
    }
}

void UChunkManagerComponent::TryParseAndSpawnInitialChunk(const FString& CurrentContent)
{
    FString JsonCandidate = CurrentContent;
    JsonCandidate.TrimStartAndEndInline();

    int32 InitialChunkStart = JsonCandidate.Find(TEXT("\"initial_chunk\""));
    if (InitialChunkStart == INDEX_NONE)
    {
        return;
    }

    int32 BraceCount = 0;
    int32 ChunkEnd = INDEX_NONE;
    bool bInString = false;
    bool bEscaped = false;

    for (int32 i = InitialChunkStart; i < JsonCandidate.Len(); ++i)
    {
        TCHAR c = JsonCandidate[i];
        if (bEscaped)
        {
            bEscaped = false;
            continue;
        }
        if (c == TEXT('\\'))
        {
            bEscaped = true;
            continue;
        }
        if (c == TEXT('"'))
        {
            bInString = !bInString;
            continue;
        }
        if (!bInString)
        {
            if (c == TEXT('{'))
            {
                ++BraceCount;
            }
            else if (c == TEXT('}'))
            {
                --BraceCount;
                if (BraceCount == 0 && ChunkEnd == INDEX_NONE)
                {
                    ChunkEnd = i;
                    break;
                }
            }
        }
    }

    if (ChunkEnd == INDEX_NONE || BraceCount > 0)
    {
        return;
    }

    int32 ObjStart = JsonCandidate.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, InitialChunkStart);
    if (ObjStart == INDEX_NONE)
    {
        return;
    }

    FString InitialChunkJson = JsonCandidate.Mid(ObjStart, ChunkEnd - ObjStart + 1);

    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InitialChunkJson);
    TSharedPtr<FJsonObject> ChunkObj;
    if (!FJsonSerializer::Deserialize(Reader, ChunkObj) || !ChunkObj.IsValid())
    {
        return;
    }

    FChunkData InitialChunk;
    InitialChunk.ChunkId = ChunkObj->GetStringField(TEXT("chunk_id"));
    InitialChunk.Name = ChunkObj->GetStringField(TEXT("name"));
    InitialChunk.Description = ChunkObj->GetStringField(TEXT("description"));
    InitialChunk.ChunkType = ChunkObj->GetStringField(TEXT("type"));
    InitialChunk.ChunkSize = 1000.0f;
    InitialChunk.bIsDiscovered = true;

    if (ChunkObj->HasField(TEXT("tags")))
    {
        const TArray<TSharedPtr<FJsonValue>>* Tags;
        if (ChunkObj->TryGetArrayField(TEXT("tags"), Tags))
        {
            for (const auto& Tag : *Tags)
            {
                InitialChunk.Tags.Add(Tag->AsString());
            }
        }
    }

    if (ChunkObj->HasField(TEXT("npcs")))
    {
        const TArray<TSharedPtr<FJsonValue>>* NPCs;
        if (ChunkObj->TryGetArrayField(TEXT("npcs"), NPCs))
        {
            for (const auto& NPCValue : *NPCs)
            {
                const TSharedPtr<FJsonObject>& NPCObj = NPCValue->AsObject();
                if (!NPCObj.IsValid()) continue;

                FNPCData NPC;
                NPC.NPCId = FString::Printf(TEXT("npc_%d"), NPCIdCounter++);
                NPC.Name = NPCObj->GetStringField(TEXT("name"));
                NPC.Description = NPCObj->GetStringField(TEXT("description"));
                NPC.Personality = NPCObj->GetStringField(TEXT("personality"));
                NPC.DialogueStyle = NPCObj->GetStringField(TEXT("dialogue_style"));
                NPC.Faction = NPCObj->GetStringField(TEXT("faction"));
                NPC.InitialDialogue = NPCObj->GetStringField(TEXT("initial_dialogue"));

                if (NPCObj->HasField(TEXT("relative_location")))
                {
                    const TArray<TSharedPtr<FJsonValue>>& Loc = NPCObj->GetArrayField(TEXT("relative_location"));
                    if (Loc.Num() >= 3)
                    {
                        NPC.RelativeLocation = FVector(
                            Loc[0]->AsNumber(),
                            Loc[1]->AsNumber(),
                            Loc[2]->AsNumber()
                        );
                    }
                }

                InitialChunk.NPCs.Add(NPC);
            }
        }
    }

    if (ChunkObj->HasField(TEXT("exit_points")))
    {
        const TArray<TSharedPtr<FJsonValue>>* Exits;
        if (ChunkObj->TryGetArrayField(TEXT("exit_points"), Exits))
        {
            for (const auto& ExitValue : *Exits)
            {
                const TSharedPtr<FJsonObject>& ExitObj = ExitValue->AsObject();
                if (!ExitObj.IsValid()) continue;

                FExitPoint Exit;
                Exit.ExitId = ExitObj->GetStringField(TEXT("exit_id"));
                Exit.ExitDirection = SnapDirectionToCardinal(ExitObj->GetNumberField(TEXT("direction")));

                if (ExitObj->HasField(TEXT("relative_location")))
                {
                    const TArray<TSharedPtr<FJsonValue>>& Loc = ExitObj->GetArrayField(TEXT("relative_location"));
                    if (Loc.Num() >= 3)
                    {
                        Exit.RelativeLocation = FVector(
                            Loc[0]->AsNumber(),
                            Loc[1]->AsNumber(),
                            Loc[2]->AsNumber()
                        );
                    }
                }

                // Intentionally ignore target_chunk_id from LLM for initial chunk:
                // these are placeholder IDs (e.g. "chunk_002") that don't correspond
                // to actual loaded chunks. Exits start undiscovered (empty TargetChunkId)
                // and get real targets only when unlock_exit triggers chunk generation.

                InitialChunk.ExitPoints.Add(Exit);
            }
        }
    }

    bInitialChunkSpawned = true;

    UE_LOG(LogTemp, Log, TEXT("Initial chunk parsed from stream, spawning early: %s (%ld chars received so far)"),
        *InitialChunk.Name, CurrentContent.Len());

    SpawnChunkAndNPCs(InitialChunk, WorldOrigin);
}

void UChunkManagerComponent::HandleWorldGenerationResponse(const FLLMChatResponse& Response)
{
    float Duration = (FDateTime::UtcNow() - WorldGenStartTime).GetTotalSeconds();
    if (LLMLogger)
    {
        LLMLogger->LogResponse(ELLMRequestType::WorldGeneration, Response, Duration, WorldConfig.WorldName);
    }

    // Save to cache
    if (bEnableCache && CacheSubsystem && !PendingRequestMessages.IsEmpty())
    {
        CacheSubsystem->SaveToCache(PendingRequestMessages, Response, PendingRequestType, PendingRequestContext);
        PendingRequestMessages.Empty();
    }

    if (Response.bSuccess && !Response.Json.IsEmpty())
    {
        ParseInitialWorldResponse(Response.Json);
    }
    else
    {
        OnWorldGenerationFailed.Broadcast(Response.Error.IsEmpty() ? TEXT("Unknown error") : Response.Error);
    }
}

void UChunkManagerComponent::ParseInitialWorldResponse(const FString& JsonResponse)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResponse);
    TSharedPtr<FJsonObject> JsonObject;

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        // 1. First, parse world config fields
        WorldConfig.WorldId = JsonObject->GetStringField(TEXT("world_id"));
        WorldConfig.WorldName = JsonObject->GetStringField(TEXT("world_name"));
        WorldConfig.Description = JsonObject->GetStringField(TEXT("description"));
        WorldConfig.Era = JsonObject->GetStringField(TEXT("era"));
        WorldConfig.Geography = JsonObject->GetStringField(TEXT("geography"));

        if (JsonObject->HasField(TEXT("factions")))
        {
            const TArray<TSharedPtr<FJsonValue>>* Factions;
            if (JsonObject->TryGetArrayField(TEXT("factions"), Factions))
            {
                for (const auto& Faction : *Factions)
                {
                    WorldConfig.Factions.Add(Faction->AsString());
                }
            }
        }

        if (JsonObject->HasField(TEXT("rules")))
        {
            const TArray<TSharedPtr<FJsonValue>>* Rules;
            if (JsonObject->TryGetArrayField(TEXT("rules"), Rules))
            {
                for (const auto& Rule : *Rules)
                {
                    WorldConfig.Rules.Add(Rule->AsString());
                }
            }
        }

        if (JsonObject->HasField(TEXT("story_theme")))
        {
            WorldConfig.StoryTheme = JsonObject->GetStringField(TEXT("story_theme"));
        }

        if (JsonObject->HasField(TEXT("player_appearance")))
        {
            WorldConfig.PlayerAppearance = JsonObject->GetStringField(TEXT("player_appearance"));
        }

        // 2. Then parse initial_chunk and spawn it (skip if already spawned by streaming)
        if (JsonObject->HasField(TEXT("initial_chunk")))
        {
            const TSharedPtr<FJsonObject>& ChunkObj = JsonObject->GetObjectField(TEXT("initial_chunk"));
            FChunkData InitialChunk;

            InitialChunk.ChunkId = ChunkObj->GetStringField(TEXT("chunk_id"));
            InitialChunk.Name = ChunkObj->GetStringField(TEXT("name"));
            InitialChunk.Description = ChunkObj->GetStringField(TEXT("description"));
            InitialChunk.ChunkType = ChunkObj->GetStringField(TEXT("type"));
            InitialChunk.ChunkSize = 1000.0f;
            InitialChunk.bIsDiscovered = true;

            if (ChunkObj->HasField(TEXT("tags")))
            {
                const TArray<TSharedPtr<FJsonValue>>* Tags;
                if (ChunkObj->TryGetArrayField(TEXT("tags"), Tags))
                {
                    for (const auto& Tag : *Tags)
                    {
                        InitialChunk.Tags.Add(Tag->AsString());
                    }
                }
            }

            if (ChunkObj->HasField(TEXT("npcs")))
            {
                const TArray<TSharedPtr<FJsonValue>>* NPCs;
                if (ChunkObj->TryGetArrayField(TEXT("npcs"), NPCs))
                {
                    for (const auto& NPCValue : *NPCs)
                    {
                        const TSharedPtr<FJsonObject>& NPCObj = NPCValue->AsObject();
                        FNPCData NPC;
                        NPC.NPCId = FString::Printf(TEXT("npc_%d"), NPCIdCounter++);
                        NPC.Name = NPCObj->GetStringField(TEXT("name"));
                        NPC.Description = NPCObj->GetStringField(TEXT("description"));
                        NPC.Personality = NPCObj->GetStringField(TEXT("personality"));
                        NPC.DialogueStyle = NPCObj->GetStringField(TEXT("dialogue_style"));
                        NPC.Faction = NPCObj->GetStringField(TEXT("faction"));
                        NPC.InitialDialogue = NPCObj->GetStringField(TEXT("initial_dialogue"));

                        if (NPCObj->HasField(TEXT("relative_location")))
                        {
                            const TArray<TSharedPtr<FJsonValue>>& Loc = NPCObj->GetArrayField(TEXT("relative_location"));
                            if (Loc.Num() >= 3)
                            {
                                NPC.RelativeLocation = FVector(
                                    Loc[0]->AsNumber(),
                                    Loc[1]->AsNumber(),
                                    Loc[2]->AsNumber()
                                );
                            }
                        }

                        InitialChunk.NPCs.Add(NPC);
                    }
                }
            }

            if (ChunkObj->HasField(TEXT("exit_points")))
            {
                const TArray<TSharedPtr<FJsonValue>>* Exits;
                if (ChunkObj->TryGetArrayField(TEXT("exit_points"), Exits))
                {
                    for (const auto& ExitValue : *Exits)
                    {
                        const TSharedPtr<FJsonObject>& ExitObj = ExitValue->AsObject();
                        FExitPoint Exit;
                        Exit.ExitId = ExitObj->GetStringField(TEXT("exit_id"));
                        Exit.ExitDirection = SnapDirectionToCardinal(ExitObj->GetNumberField(TEXT("direction")));

                        if (ExitObj->HasField(TEXT("relative_location")))
                        {
                            const TArray<TSharedPtr<FJsonValue>>& Loc = ExitObj->GetArrayField(TEXT("relative_location"));
                            if (Loc.Num() >= 3)
                            {
                                Exit.RelativeLocation = FVector(
                                    Loc[0]->AsNumber(),
                                    Loc[1]->AsNumber(),
                                    Loc[2]->AsNumber()
                                );
                            }
                        }

                        // Intentionally ignore target_chunk_id from LLM for initial chunk:
                        // these are placeholder IDs (e.g. "chunk_002") that don't correspond
                        // to actual loaded chunks. Exits start undiscovered (empty TargetChunkId)
                        // and get real targets only when unlock_exit triggers chunk generation.

                        InitialChunk.ExitPoints.Add(Exit);
                    }
                }
            }

            if (LoadedChunks.Contains(InitialChunk.ChunkId))
            {
                UE_LOG(LogTemp, Log, TEXT("Initial chunk %s already spawned by stream, updating data only"), *InitialChunk.ChunkId);
                AGameChunkActor* ExistingChunk = LoadedChunks[InitialChunk.ChunkId];
                if (ExistingChunk)
                {
                    ExistingChunk->SetChunkData(InitialChunk);
                }
            }
            else
            {
                SpawnChunkAndNPCs(InitialChunk, WorldOrigin);
            }
        }

        // 3. Broadcast world generation complete after chunk is spawned
        OnWorldGenerationComplete.Broadcast();
    }
    else
    {
        OnWorldGenerationFailed.Broadcast(TEXT("Failed to parse world response JSON"));
    }
}

void UChunkManagerComponent::SpawnChunkAndNPCs(const FChunkData& ChunkData, const FVector& Location)
{
    if (!ChunkActorClass)
    {
        UE_LOG(LogTemp, Error, TEXT("ChunkActorClass not set in ChunkManagerComponent"));
        return;
    }

    if (LoadedChunks.Contains(ChunkData.ChunkId))
    {
        AGameChunkActor* ExistingChunk = LoadedChunks[ChunkData.ChunkId];
        if (ExistingChunk)
        {
            UE_LOG(LogTemp, Log, TEXT("Chunk %s already exists, skipping spawn"), *ChunkData.ChunkId);
            CurrentChunk = ExistingChunk;
            OnChunkLoaded.Broadcast(ExistingChunk);
            return;
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AGameChunkActor* NewChunk = GetWorld()->SpawnActor<AGameChunkActor>(ChunkActorClass, Location, FRotator::ZeroRotator, SpawnParams);
    if (NewChunk)
    {
        NewChunk->SetChunkData(ChunkData);

        if (NPCCharacterClass)
        {
            int32 TotalNPCs = ChunkData.NPCs.Num();
            for (int32 NPCIndex = 0; NPCIndex < TotalNPCs; ++NPCIndex)
            {
                const FNPCData& NPCData = ChunkData.NPCs[NPCIndex];

                FVector RelativeNPCLoc = NewChunk->ComputeNPCLocation(NPCIndex, TotalNPCs);
                FVector NPCLocation = Location + RelativeNPCLoc;

                float NPCFacingYaw = 0.0f;
                if (TotalNPCs == 1)
                {
                    NPCFacingYaw = 180.0f;
                }

                FRotator NPCRotation = FRotator(0, 0, 0);//FRotator(0, NPCFacingYaw, 0);

                ANPCCharacter* NPC = GetWorld()->SpawnActor<ANPCCharacter>(NPCCharacterClass, NPCLocation, NPCRotation, SpawnParams);
                if (NPC)
                {
                    NPC->SetNPCData(NPCData);
                    NewChunk->SpawnedNPCs.Add(NPC);

                    // Trigger async character image generation for new NPCs.
                    // The CharacterImageSubsystem handles caching, dedup, and application
                    // of the generated flipbook/portrait to NPC and any active dialogue widget.
                    if (UCharacterImageSubsystem* ImageSubsystem = GetGameInstance()->GetSubsystem<UCharacterImageSubsystem>())
                    {
                        ImageSubsystem->RequestCharacterImageForNPC(NPC, ActiveDialogueWidget);
                    }
                }
            }
        }

        LoadedChunks.Add(ChunkData.ChunkId, NewChunk);
        CurrentChunk = NewChunk;

        OnChunkLoaded.Broadcast(NewChunk);
    }
}

void UChunkManagerComponent::TryTransitionToNextChunk(AGameChunkActor* FromChunk, const FString& ExitId)
{
    if (!FromChunk)
    {
        return;
    }

    FChunkData ChunkData = FromChunk->GetChunkData();
    FExitPoint* ExitPoint = ChunkData.ExitPoints.FindByPredicate([&ExitId](const FExitPoint& Exit)
    {
        return Exit.ExitId == ExitId;
    });

    if (!ExitPoint)
    {
        return;
    }

    if (ExitPoint->bIsLocked && !ExitPoint->UnlockCondition.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Exit %s is locked: %s"), *ExitId, *ExitPoint->UnlockCondition);
        return;
    }

    if (!ExitPoint->TargetChunkId.IsEmpty() && LoadedChunks.Contains(ExitPoint->TargetChunkId))
    {
        // Target chunk exists, create portal if needed and teleport player
        FromChunk->CreatePortalForExitById(ExitId);

        AGameChunkActor* TargetChunk = LoadedChunks[ExitPoint->TargetChunkId];
        CurrentChunk = TargetChunk;
        OnChunkLoaded.Broadcast(TargetChunk);

        if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
        {
            const FTransform SpawnTransform = TargetChunk->GetPlayerStartTransformFromExit(ExitPoint->ExitDirection);
            Player->SetActorLocationAndRotation(
                SpawnTransform.GetLocation(),
                SpawnTransform.Rotator(),
                false,
                nullptr,
                ETeleportType::TeleportPhysics);
        }
    }
    else
    {
        GenerateNextChunk(FromChunk->GetChunkId(), ExitId);
    }
}

void UChunkManagerComponent::GenerateNextChunk(const FString& FromChunkId, const FString& ExitId)
{
    if (bIsGeneratingChunk)
    {
        UE_LOG(LogTemp, Warning, TEXT("Chunk generation already in progress, skipping request from=%s exit=%s"), *FromChunkId, *ExitId);
        return;
    }

    bIsGeneratingChunk = true;
    FString SystemPrompt = BuildChunkGenerationSystemPrompt();

    FString UserPrompt = FString::Printf(
        TEXT("Generate the next chunk for world '%s' (era: %s, theme: %s). Player is exiting from chunk '%s' through exit '%s'."),
        *WorldConfig.WorldName, *WorldConfig.Era, *WorldConfig.StoryTheme, *FromChunkId, *ExitId);

    TArray<FLLMChatMessage> Messages;
    Messages.Add(FLLMChatMessage{TEXT("system"), SystemPrompt});
    Messages.Add(FLLMChatMessage{TEXT("user"), UserPrompt});

    // Check cache first - ChunkGen uses type-based matching (latest ChunkGen entry)
    FString CacheContext = FString::Printf(TEXT("from=%s exit=%s"), *FromChunkId, *ExitId);
    if (bEnableCache && CacheSubsystem)
    {
        FLLMCacheEntry CachedEntry;
        if (CacheSubsystem->GetLastChunkGenCacheEntry(CachedEntry))
        {
            UE_LOG(LogTemp, Log, TEXT("[Cache] Chunk generation cache hit (type match)! Replay mode: %d"), IsReplayModeActive());

            if (IsReplayModeActive())
            {
                bIsGeneratingChunk = false;
                FLLMChatResponse CachedResponse;
                CachedResponse.bSuccess = CachedEntry.bWasSuccessful;
                CachedResponse.Content = CachedEntry.ResponseContent;
                CachedResponse.Json = CachedEntry.ResponseJson;
                CachedResponse.Error = CachedEntry.bWasSuccessful ? TEXT("") : TEXT("Cached error response");

                PendingFromChunkId = FromChunkId;
                PendingExitId = ExitId;
                HandleChunkGenerationResponse(CachedResponse);
                return;
            }
        }
        else if (IsReplayModeActive())
        {
            bIsGeneratingChunk = false;
            UE_LOG(LogTemp, Error, TEXT("[Cache] Replay mode enabled but chunk generation cache miss! Aborting request."));
            return;
        }
    }

    // Store messages for potential cache save
    PendingRequestMessages = Messages;
    PendingRequestType = TEXT("ChunkGen");
    PendingRequestContext = CacheContext;

    FLLMChatRequest Request;
    Request.Messages = Messages;
    Request.Temperature = 0.7f;
    Request.MaxTokens = 2048;
    Request.bForceJsonObject = true;
    Request.TimeoutSeconds = 90.0f;
    Request.ThinkingMode = LLMThinkingMode;
    Request.ExtraBodyJson = LLMExtraBodyJson;

    if (LLMLogger)
    {
        LLMLogger->LogRequest(ELLMRequestType::ChunkGeneration, Request, CacheContext);
    }
    ChunkGenStartTime = FDateTime::UtcNow();

    ULLMChatCompletionAsyncAction* Action = ULLMChatCompletionAsyncAction::ChatCompletion(this, Request);

    // Store context for callback
    PendingFromChunkId = FromChunkId;
    PendingExitId = ExitId;

    Action->Completed.AddDynamic(this, &UChunkManagerComponent::HandleChunkGenerationResponse);
    Action->Activate();
}

void UChunkManagerComponent::HandleChunkGenerationResponse(const FLLMChatResponse& Response)
{
    bIsGeneratingChunk = false;

    float Duration = (FDateTime::UtcNow() - ChunkGenStartTime).GetTotalSeconds();
    if (LLMLogger)
    {
        FString Context = FString::Printf(TEXT("from=%s exit=%s"), *PendingFromChunkId, *PendingExitId);
        LLMLogger->LogResponse(ELLMRequestType::ChunkGeneration, Response, Duration, Context);
    }

    // Save to cache
    if (bEnableCache && CacheSubsystem && !PendingRequestMessages.IsEmpty())
    {
        CacheSubsystem->SaveToCache(PendingRequestMessages, Response, PendingRequestType, PendingRequestContext);
        PendingRequestMessages.Empty();
    }

    if (Response.bSuccess && !Response.Json.IsEmpty())
    {
        ParseChunkGenerationResponse(Response.Json, PendingFromChunkId, PendingExitId);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to generate next chunk: %s"), *Response.Error);
    }
}

void UChunkManagerComponent::ParseChunkGenerationResponse(const FString& JsonResponse, const FString& FromChunkId, const FString& ExitId)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResponse);
    TSharedPtr<FJsonObject> JsonObject;

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FChunkData NewChunk;
        NewChunk.ChunkId = JsonObject->GetStringField(TEXT("chunk_id"));
        NewChunk.Name = JsonObject->GetStringField(TEXT("name"));
        NewChunk.Description = JsonObject->GetStringField(TEXT("description"));
        NewChunk.ChunkType = JsonObject->GetStringField(TEXT("type"));
        NewChunk.ChunkSize = 1000.0f;
        NewChunk.bIsDiscovered = true;

        if (LoadedChunks.Contains(NewChunk.ChunkId))
        {
            const FString OriginalChunkId = NewChunk.ChunkId;
            do
            {
                NewChunk.ChunkId = FString::Printf(TEXT("chunk_%d"), ChunkIdCounter++);
            }
            while (LoadedChunks.Contains(NewChunk.ChunkId));

            UE_LOG(LogTemp, Warning, TEXT("Generated chunk id %s already exists, remapping exit %s target to unique chunk id %s"),
                *OriginalChunkId, *ExitId, *NewChunk.ChunkId);
        }

        if (JsonObject->HasField(TEXT("tags")))
        {
            const TArray<TSharedPtr<FJsonValue>>* Tags;
            if (JsonObject->TryGetArrayField(TEXT("tags"), Tags))
            {
                for (const auto& Tag : *Tags)
                {
                    NewChunk.Tags.Add(Tag->AsString());
                }
            }
        }

        if (JsonObject->HasField(TEXT("npcs")))
        {
            const TArray<TSharedPtr<FJsonValue>>* NPCs;
            if (JsonObject->TryGetArrayField(TEXT("npcs"), NPCs))
            {
                for (const auto& NPCValue : *NPCs)
                {
                    const TSharedPtr<FJsonObject>& NPCObj = NPCValue->AsObject();
                    FNPCData NPC;
                    NPC.NPCId = FString::Printf(TEXT("npc_%d"), NPCIdCounter++);
                    NPC.Name = NPCObj->GetStringField(TEXT("name"));
                    NPC.Description = NPCObj->GetStringField(TEXT("description"));
                    NPC.Personality = NPCObj->GetStringField(TEXT("personality"));
                    NPC.DialogueStyle = NPCObj->GetStringField(TEXT("dialogue_style"));
                    NPC.Faction = NPCObj->GetStringField(TEXT("faction"));
                    NPC.InitialDialogue = NPCObj->GetStringField(TEXT("initial_dialogue"));

                    if (NPCObj->HasField(TEXT("relative_location")))
                    {
                        const TArray<TSharedPtr<FJsonValue>>& Loc = NPCObj->GetArrayField(TEXT("relative_location"));
                        if (Loc.Num() >= 3)
                        {
                            NPC.RelativeLocation = FVector(Loc[0]->AsNumber(), Loc[1]->AsNumber(), Loc[2]->AsNumber());
                        }
                    }

                    NewChunk.NPCs.Add(NPC);
                }
            }
        }

        if (JsonObject->HasField(TEXT("exit_points")))
        {
            const TArray<TSharedPtr<FJsonValue>>* Exits;
            if (JsonObject->TryGetArrayField(TEXT("exit_points"), Exits))
            {
                for (const auto& ExitValue : *Exits)
                {
                    const TSharedPtr<FJsonObject>& ExitObj = ExitValue->AsObject();
                    FExitPoint Exit;
                    Exit.ExitId = ExitObj->GetStringField(TEXT("exit_id"));
                    Exit.ExitDirection = SnapDirectionToCardinal(ExitObj->GetNumberField(TEXT("direction")));

                    if (ExitObj->HasField(TEXT("relative_location")))
                    {
                        const TArray<TSharedPtr<FJsonValue>>& Loc = ExitObj->GetArrayField(TEXT("relative_location"));
                        if (Loc.Num() >= 3)
                        {
                            Exit.RelativeLocation = FVector(Loc[0]->AsNumber(), Loc[1]->AsNumber(), Loc[2]->AsNumber());
                        }
                    }

                    NewChunk.ExitPoints.Add(Exit);
                }
            }
        }

        LoadedChunks.Add(NewChunk.ChunkId, nullptr);

        FVector ChunkLocation = WorldOrigin;
        float EntryDirection = 0.0f;
        if (LoadedChunks.Contains(FromChunkId))
        {
            if (AGameChunkActor* FromChunk = LoadedChunks[FromChunkId])
            {
                FChunkData FromData = FromChunk->GetChunkData();
                FExitPoint* Exit = FromData.ExitPoints.FindByPredicate([&ExitId](const FExitPoint& E) { return E.ExitId == ExitId; });
                if (Exit)
                {
                    EntryDirection = Exit->ExitDirection;
                    FRotator Rot(0, EntryDirection, 0);
                    ChunkLocation = FromChunk->GetActorLocation() + Rot.Vector() * ChunkSpacing;
                }
            }
        }

        if (!NewChunk.ExitPoints.ContainsByPredicate([EntryDirection](const FExitPoint& Exit)
        {
            return FMath::IsNearlyEqual(Exit.ExitDirection, FMath::Fmod(EntryDirection + 180.0f, 360.0f), 1.0f);
        }))
        {
            FExitPoint ReverseExit;
            const float ReverseDirection = FMath::Fmod(EntryDirection + 180.0f, 360.0f);
            ReverseExit.ExitId = FString::Printf(TEXT("return_%s"), *FromChunkId);
            ReverseExit.ExitDirection = ReverseDirection < 0.0f ? ReverseDirection + 360.0f : ReverseDirection;
            NewChunk.ExitPoints.Add(ReverseExit);

            UE_LOG(LogTemp, Warning, TEXT("Generated chunk %s did not include a reverse exit for source chunk %s; injected fallback return exit at %.0f degrees"),
                *NewChunk.ChunkId, *FromChunkId, ReverseExit.ExitDirection);
        }

        // Update FromChunk's exit TargetChunkId before spawning new chunk
        if (AGameChunkActor* FromChunk = LoadedChunks.FindRef(FromChunkId))
        {
            FromChunk->UpdateExitTargetChunkId(ExitId, NewChunk.ChunkId);
        }

        SpawnChunkAndNPCs(NewChunk, ChunkLocation);

        // After spawning, set reverse exit target in new chunk
        if (AGameChunkActor* NewChunkActor = LoadedChunks.FindRef(NewChunk.ChunkId))
        {
            NewChunkActor->SetReverseExitTarget(FromChunkId, EntryDirection);
        }

        // Now create all portals after both forward and reverse exits are set
        UpdateAllChunkPortals();
    }
}

TArray<AGameChunkActor*> UChunkManagerComponent::GetLoadedChunks() const
{
    TArray<AGameChunkActor*> Chunks;
    LoadedChunks.GenerateValueArray(Chunks);
    return Chunks;
}

FString UChunkManagerComponent::GetChunkNameById(const FString& ChunkId) const
{
    if (const AGameChunkActor*const* ChunkPtr = LoadedChunks.Find(ChunkId))
    {
        if (const AGameChunkActor* Chunk = *ChunkPtr)
        {
            return Chunk->GetChunkData().Name;
        }
    }
    return FString();
}

void UChunkManagerComponent::UpdateAllChunkPortals()
{
    for (const TPair<FString, AGameChunkActor*>& Pair : LoadedChunks)
    {
        if (AGameChunkActor* Chunk = Pair.Value)
        {
            Chunk->RebuildExitPortals();
        }
    }
}

void UChunkManagerComponent::HandleCharacterImageReady(const FString& CharacterId)
{
    if (!ActiveDialogueWidget || !CurrentDialogueNPC)
    {
        return;
    }

    UCharacterImageSubsystem* ImageSubsystem = GetGameInstance()->GetSubsystem<UCharacterImageSubsystem>();
    if (!ImageSubsystem)
    {
        return;
    }

    bool bIsNPC = CharacterId == CurrentNPCData.NPCId;
    bool bIsPlayer = CharacterId == TEXT("Player");

    if (bIsNPC)
    {
        UTexture2D* NPCTex = ImageSubsystem->GetExpressionPortrait(CharacterId, TEXT("neutral"));
        if (NPCTex)
        {
            ActiveDialogueWidget->SetNPCPortrait(NPCTex);
            UE_LOG(LogTemp, Log, TEXT("[ChunkManager] Updated NPC portrait for %s"), *CharacterId);
        }
    }

    if (bIsPlayer)
    {
        UTexture2D* PlayerTex = ImageSubsystem->GetExpressionPortrait(CharacterId, TEXT("neutral"));
        if (PlayerTex)
        {
            ActiveDialogueWidget->SetPlayerPortrait(PlayerTex);
            UE_LOG(LogTemp, Log, TEXT("[ChunkManager] Updated Player portrait"));
        }
    }
}

void UChunkManagerComponent::StartDialogue(ANPCCharacter* NPC)
{
    if (!NPC || !DialogueWidgetClass)
    {
        return;
    }

    CurrentDialogueNPC = NPC;
    CurrentNPCData = NPC->GetNPCData();
    LastNPChoices.Empty();
    bIsWaitingForResponse = false;
    bPendingInitialChoicesOnlyResponse = false;

    // Create dialogue widget
    if (!ActiveDialogueWidget)
    {
        ActiveDialogueWidget = CreateWidget<UDialogueWidget>(UGameplayStatics::GetPlayerController(this, 0), DialogueWidgetClass);
        if (ActiveDialogueWidget)
        {
            ActiveDialogueWidget->AddToViewport();

            // Bind dialogue widget events
            ActiveDialogueWidget->OnMessageSubmittedDelegate.AddDynamic(this, &UChunkManagerComponent::SubmitPlayerTextInput);
            ActiveDialogueWidget->OnChoiceSelectedDelegate.AddDynamic(this, &UChunkManagerComponent::SubmitPlayerChoice);
            ActiveDialogueWidget->OnDialogueClosedDelegate.AddDynamic(this, &UChunkManagerComponent::EndDialogue);

            if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
            {
                FInputModeGameAndUI InputMode;
                InputMode.SetWidgetToFocus(ActiveDialogueWidget->TakeWidget());
                InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                InputMode.SetHideCursorDuringCapture(false);
                PC->SetInputMode(InputMode);
                PC->bShowMouseCursor = true;
            }
        }
    }

    // Clear previous dialogue history in widget when switching NPCs
    if (ActiveDialogueWidget)
    {
        ActiveDialogueWidget->ClearHistory();
    }

    // Push cached character image to the dialogue widget for the current NPC.
    // If image is not yet ready, the subsystem callback (OnCharacterImageReady) will
    // update the dialogue widget as soon as generation completes.
    if (UCharacterImageSubsystem* ImageSubsystem = GetGameInstance()->GetSubsystem<UCharacterImageSubsystem>())
    {
        FString NPCId = NPC->GetNPCId();
        if (ImageSubsystem->HasCachedImageSet(NPCId))
        {
            ImageSubsystem->ApplyCachedImageToDialogue(NPCId, ActiveDialogueWidget);
        }

        // Also set player portrait if cached
        if (ActiveDialogueWidget)
        {
            UTexture2D* PlayerPortrait = ImageSubsystem->GetExpressionPortrait(TEXT("Player"), TEXT("neutral"));
            if (!PlayerPortrait)
            {
                PlayerPortrait = ImageSubsystem->GetExpressionPortrait(TEXT("Player"), TEXT("happy"));
            }
            if (PlayerPortrait)
            {
                ActiveDialogueWidget->SetPlayerPortrait(PlayerPortrait);
            }
        }
    }

    // Check if we have prior dialogue history with this NPC
    FString NPCId = CurrentNPCData.NPCId;
    bool bHasPriorHistory = NPCDialogueHistoryMap.Contains(NPCId) && NPCDialogueHistoryMap[NPCId].Num() > 0;

    if (bHasPriorHistory)
    {
        // Restore prior dialogue history
        DialogueMessages = NPCDialogueHistoryMap[NPCId];

        // Show thinking indicator while generating a context-aware greeting
        if (ActiveDialogueWidget)
        {
            ActiveDialogueWidget->ShowThinkingIndicator();
        }

        // Request a new context-aware greeting from LLM
        SendDialogueRequest(true);
    }
    else
    {
        // First time talking - use initial dialogue
        DialogueMessages.Empty();

        // Add initial NPC greeting to history
        if (!CurrentNPCData.InitialDialogue.IsEmpty())
        {
            AddDialogueMessage(TEXT("assistant"), CurrentNPCData.InitialDialogue);
        }

        // Show NPC initial dialogue
        if (ActiveDialogueWidget)
        {
            ActiveDialogueWidget->ShowNPCResponse(CurrentNPCData.Name, CurrentNPCData.InitialDialogue, TArray<FString>());
        }

        OnDialogueStarted.Broadcast(NPC, CurrentNPCData.InitialDialogue);

        // Request LLM to generate dialogue choices following the initial greeting
        bPendingInitialChoicesOnlyResponse = true;
        SendDialogueRequest(true);
    }

    // Disable player input to game
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->DisableInput(PC);
    }
}

void UChunkManagerComponent::EndDialogue()
{
    // Save dialogue history for this NPC before clearing
    if (CurrentDialogueNPC && DialogueMessages.Num() > 0)
    {
        FString NPCId = CurrentNPCData.NPCId;
        if (!NPCId.IsEmpty())
        {
            NPCDialogueHistoryMap.Add(NPCId, DialogueMessages);
            UE_LOG(LogTemp, Log, TEXT("Saved dialogue history for NPC %s (%d messages)"), *NPCId, DialogueMessages.Num());
        }
    }

    CurrentDialogueNPC = nullptr;
    CurrentNPCData = FNPCData();
    DialogueMessages.Empty();
    LastNPChoices.Empty();
    bIsWaitingForResponse = false;
    bPendingInitialChoicesOnlyResponse = false;

    if (ActiveDialogueWidget)
    {
        ActiveDialogueWidget->RemoveFromParent();
        ActiveDialogueWidget = nullptr;
    }

    // Re-enable player input
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->EnableInput(PC);

        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = false;
    }

    OnDialogueEnded.Broadcast(CurrentDialogueNPC);
}

void UChunkManagerComponent::SubmitPlayerTextInput(const FString& InputText)
{
    if (!CurrentDialogueNPC || bIsWaitingForResponse)
    {
        return;
    }

    // Add player input to history
    AddDialogueMessage(TEXT("user"), InputText);

    // Request LLM response
    SendDialogueRequest();
}

void UChunkManagerComponent::SubmitPlayerChoice(int32 ChoiceIndex)
{
    if (!CurrentDialogueNPC || bIsWaitingForResponse)
    {
        return;
    }

    if (ChoiceIndex >= 0 && ChoiceIndex < LastNPChoices.Num())
    {
        FString ChoiceText = FString::Printf(TEXT("[CHOICE:%d] %s"), ChoiceIndex + 1, *LastNPChoices[ChoiceIndex]);
        AddDialogueMessage(TEXT("user"), ChoiceText);

        // Check if this choice has an associated action
        if (FDialogueAction* Action = PendingChoiceActions.Find(ChoiceIndex))
        {
            if (Action->ActionType == EDialogueActionType::EndDialogue)
            {
                UE_LOG(LogTemp, Log, TEXT("Choice %d has end_dialogue action, closing dialogue"), ChoiceIndex);
                EndDialogue();
                return;
            }
            else if (Action->ActionType == EDialogueActionType::UnlockExit)
            {
                UE_LOG(LogTemp, Log, TEXT("Choice %d has unlock_exit action"), ChoiceIndex);
                TArray<FDialogueAction> Actions;
                Actions.Add(*Action);
                ExecuteDialogueActions(Actions);
                // Continue dialogue after unlocking (NPC will respond)
            }
        }

        SendDialogueRequest();
    }
}

void UChunkManagerComponent::ExecuteDialogueActions(const TArray<FDialogueAction>& Actions)
{
    bool bShouldEndDialogue = false;

    for (const FDialogueAction& Action : Actions)
    {
        UE_LOG(LogTemp, Log, TEXT("Executing dialogue action: %d"), (int32)Action.ActionType);

        switch (Action.ActionType)
        {
        case EDialogueActionType::EndDialogue:
            bShouldEndDialogue = true;
            break;

        case EDialogueActionType::UnlockExit:
            if (CurrentChunk && !Action.ExitId.IsEmpty())
            {
                const FChunkData& ChunkData = CurrentChunk->GetChunkData();
                const FExitPoint* ExitPoint = ChunkData.ExitPoints.FindByPredicate(
                    [&Action](const FExitPoint& E) { return E.ExitId == Action.ExitId; });

                if (ExitPoint)
                {
                    FString TargetId = !Action.TargetChunkId.IsEmpty()
                        ? Action.TargetChunkId
                        : ExitPoint->TargetChunkId;

                    if (!TargetId.IsEmpty() && LoadedChunks.Contains(TargetId))
                    {
                        // Target chunk already loaded - this is a return path or already-explored exit.
                        // Just ensure the portal exists (CreatePortalForExitById guards against duplicates).
                        // Do NOT generate a new chunk here.
                        CurrentChunk->UpdateExitTargetChunkId(Action.ExitId, TargetId);
                        CurrentChunk->CreatePortalForExitById(Action.ExitId);
                        UE_LOG(LogTemp, Log, TEXT("Unlock exit %s: target %s already loaded (return/visited path), ensuring portal exists (no new chunk generated)"),
                            *Action.ExitId, *TargetId);
                    }
                    else
                    {
                        // Target chunk not loaded - generate it
                        UE_LOG(LogTemp, Log, TEXT("Unlock exit %s: generating new chunk from exit"), *Action.ExitId);
                        GenerateNextChunk(CurrentChunk->GetChunkId(), Action.ExitId);
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Unlock exit failed: exit %s not found in chunk %s"),
                        *Action.ExitId, *CurrentChunk->GetChunkId());
                }
            }
            break;

        case EDialogueActionType::GiveItem:
            UE_LOG(LogTemp, Log, TEXT("Give item: %s"), *Action.ItemId);
            break;

        case EDialogueActionType::StartQuest:
            UE_LOG(LogTemp, Log, TEXT("Start quest: %s"), *Action.QuestId);
            break;

        case EDialogueActionType::ChangeNPCFaction:
            UE_LOG(LogTemp, Log, TEXT("Change NPC %s faction to %s"), *Action.NPCId, *Action.NewFaction);
            break;

        case EDialogueActionType::Custom:
            UE_LOG(LogTemp, Log, TEXT("Custom action: %s"), *Action.CustomData);
            break;

        default:
            break;
        }
    }

    if (bShouldEndDialogue)
    {
        EndDialogue();
    }
}

void UChunkManagerComponent::ClearDialogueHistory()
{
    DialogueMessages.Empty();
}

void UChunkManagerComponent::AddDialogueMessage(const FString& Role, const FString& Content)
{
    FLLMChatMessage Message;
    Message.Role = Role;
    Message.Content = Content;
    DialogueMessages.Add(Message);

    // Trim history if too long
    if (DialogueMessages.Num() > MaxDialogueHistorySize)
    {
        // Keep system prompt and recent messages
        TArray<FLLMChatMessage> TrimmedMessages;

        // Always keep system prompt
        if (DialogueMessages[0].Role == TEXT("system"))
        {
            TrimmedMessages.Add(DialogueMessages[0]);
        }

        // Keep last N messages
        int32 MessagesToKeep = MaxDialogueHistorySize - 1;
        for (int32 i = FMath::Max(1, DialogueMessages.Num() - MessagesToKeep); i < DialogueMessages.Num(); ++i)
        {
            TrimmedMessages.Add(DialogueMessages[i]);
        }

        DialogueMessages = TrimmedMessages;
    }
}

void UChunkManagerComponent::SendDialogueRequest(bool bGenerateGreeting)
{
    if (!CurrentDialogueNPC || bIsWaitingForResponse)
    {
        return;
    }

    bIsWaitingForResponse = true;

    if (ActiveDialogueWidget && !bPendingInitialChoicesOnlyResponse)
    {
        ActiveDialogueWidget->ShowThinkingIndicator();
    }

    TArray<FLLMChatMessage> Messages;

    // Add system prompt
    FString SystemPrompt = BuildDialogueSystemPrompt();
    Messages.Add(FLLMChatMessage{TEXT("system"), SystemPrompt});

    // Add dialogue context
    FString ContextInfo = BuildDialogueContextInfo();
    Messages.Add(FLLMChatMessage{TEXT("system"), ContextInfo});

    // Add dialogue history
    Messages.Append(DialogueMessages);

    if (bGenerateGreeting)
    {
        if (DialogueMessages.Num() <= 1)
        {
            Messages.Add(FLLMChatMessage{TEXT("system"),
                TEXT("对话刚开始，请接续你刚才的问候，提供3-5个玩家可以选择的对话选项。使用JSON格式回复，response可以简短承接，重点在choices中提供选项。")});
        }
        else
        {
            Messages.Add(FLLMChatMessage{TEXT("system"),
                TEXT("玩家再次与你对话，请基于之前的对话历史生成一句新的自然开场白（不要重复之前的问候）。使用JSON格式回复。")});
        }
    }

    // Check cache first - Dialogue uses NPC-based matching (latest Dialogue entry for this NPC)
    FString DialogueCacheContext = FString::Printf(TEXT("npc=%s"), *CurrentNPCData.Name);
    if (bEnableCache && CacheSubsystem)
    {
        FLLMCacheEntry CachedEntry;
        if (CacheSubsystem->GetLastDialogueCacheEntryForNPC(CurrentNPCData.Name, CachedEntry))
        {
            UE_LOG(LogTemp, Log, TEXT("[Cache] Dialogue cache hit (NPC match: %s)! Replay mode: %d"), *CurrentNPCData.Name, IsReplayModeActive());

            if (IsReplayModeActive())
            {
                bIsWaitingForResponse = false;
                if (ActiveDialogueWidget)
                {
                    ActiveDialogueWidget->HideThinkingIndicator();
                }

                FLLMChatResponse CachedResponse;
                CachedResponse.bSuccess = CachedEntry.bWasSuccessful;
                CachedResponse.Content = CachedEntry.ResponseContent;
                CachedResponse.Json = CachedEntry.ResponseJson;
                CachedResponse.Error = CachedEntry.bWasSuccessful ? TEXT("") : TEXT("Cached error response");
                HandleDialogueResponse(CachedResponse);
                return;
            }
        }
        else if (IsReplayModeActive())
        {
            bIsWaitingForResponse = false;
            UE_LOG(LogTemp, Error, TEXT("[Cache] Replay mode enabled but dialogue cache miss! Aborting request."));
            if (ActiveDialogueWidget)
            {
                ActiveDialogueWidget->HideThinkingIndicator();
                ActiveDialogueWidget->ShowNPCResponse(CurrentNPCData.Name,
                    TEXT("(回放模式：未找到缓存的对话记录)"),
                    TArray<FString>());
            }
            return;
        }
    }

    // Store messages for potential cache save
    PendingRequestMessages = Messages;
    PendingRequestType = TEXT("Dialogue");
    PendingRequestContext = DialogueCacheContext;

    FLLMChatRequest Request;
    Request.Messages = Messages;
    Request.Temperature = DialogueTemperature;
    Request.MaxTokens = DialogueMaxTokens;
    Request.bForceJsonObject = true;
    Request.TimeoutSeconds = 60.0f;
    Request.ThinkingMode = LLMThinkingMode;
    Request.ExtraBodyJson = LLMExtraBodyJson;

    if (LLMLogger)
    {
        LLMLogger->LogRequest(ELLMRequestType::NPCDialogue, Request, DialogueCacheContext);
    }
    DialogueStartTime = FDateTime::UtcNow();

    ULLMChatCompletionAsyncAction* Action = ULLMChatCompletionAsyncAction::ChatCompletion(this, Request);
    Action->Completed.AddDynamic(this, &UChunkManagerComponent::HandleDialogueResponse);
    Action->Activate();
}

void UChunkManagerComponent::HandleDialogueResponse(const FLLMChatResponse& Response)
{
    bIsWaitingForResponse = false;
    const bool bInitialChoicesOnlyResponse = bPendingInitialChoicesOnlyResponse;
    bPendingInitialChoicesOnlyResponse = false;

    if (ActiveDialogueWidget && !bInitialChoicesOnlyResponse)
    {
        ActiveDialogueWidget->HideThinkingIndicator();
    }

    float Duration = (FDateTime::UtcNow() - DialogueStartTime).GetTotalSeconds();
    if (LLMLogger)
    {
        FString Context = FString::Printf(TEXT("npc=%s"), *CurrentNPCData.Name);
        LLMLogger->LogResponse(ELLMRequestType::NPCDialogue, Response, Duration, Context);
    }

    // Save to cache
    if (bEnableCache && CacheSubsystem && !PendingRequestMessages.IsEmpty())
    {
        CacheSubsystem->SaveToCache(PendingRequestMessages, Response, PendingRequestType, PendingRequestContext);
        PendingRequestMessages.Empty();
    }

    if (Response.bSuccess && !Response.Json.IsEmpty())
    {
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response.Json);
        TSharedPtr<FJsonObject> JsonObject;

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            FString ResponseText = JsonObject->GetStringField(TEXT("response_text"));

            TArray<FString> Choices;
            if (JsonObject->HasField(TEXT("choices")))
            {
                const TArray<TSharedPtr<FJsonValue>>* ChoiceArray;
                if (JsonObject->TryGetArrayField(TEXT("choices"), ChoiceArray))
                {
                    for (const auto& Choice : *ChoiceArray)
                    {
                        Choices.Add(Choice->AsString());
                    }
                }
            }

            // Parse expression fields and switch dialogue portraits
            FString NPCExpression = TEXT("neutral");
            FString PlayerExpression = TEXT("neutral");
            if (JsonObject->HasField(TEXT("npc_expression")))
            {
                NPCExpression = JsonObject->GetStringField(TEXT("npc_expression"));
            }
            if (JsonObject->HasField(TEXT("player_expression")))
            {
                PlayerExpression = JsonObject->GetStringField(TEXT("player_expression"));
            }

            if (ActiveDialogueWidget)
            {
                if (UCharacterImageSubsystem* ImageSubsystem = GetGameInstance()->GetSubsystem<UCharacterImageSubsystem>())
                {
                    // Switch NPC portrait by expression
                    if (!CurrentNPCData.NPCId.IsEmpty())
                    {
                        UTexture2D* NPCExprTex = ImageSubsystem->GetExpressionPortrait(CurrentNPCData.NPCId, NPCExpression);
                        if (!NPCExprTex)
                        {
                            // Fallback to default portrait (DetailAnchor) if expression not cached
                            NPCExprTex = ImageSubsystem->GetExpressionPortrait(CurrentNPCData.NPCId, TEXT("neutral"));
                        }
                        if (NPCExprTex)
                        {
                            ActiveDialogueWidget->SetNPCPortrait(NPCExprTex);
                        }
                    }

                    // Switch player portrait by expression
                    UTexture2D* PlayerExprTex = ImageSubsystem->GetExpressionPortrait(TEXT("Player"), PlayerExpression);
                    if (!PlayerExprTex)
                    {
                        PlayerExprTex = ImageSubsystem->GetExpressionPortrait(TEXT("Player"), TEXT("neutral"));
                    }
                    if (PlayerExprTex)
                    {
                        ActiveDialogueWidget->SetPlayerPortrait(PlayerExprTex);
                    }
                }
            }

            // Parse dialogue actions
            TArray<FDialogueAction> Actions;
            if (JsonObject->HasField(TEXT("actions")))
            {
                const TArray<TSharedPtr<FJsonValue>>* ActionArray;
                if (JsonObject->TryGetArrayField(TEXT("actions"), ActionArray))
                {
                    for (const auto& ActionVal : *ActionArray)
                    {
                        const TSharedPtr<FJsonObject>& ActionObj = ActionVal->AsObject();
                        if (!ActionObj.IsValid()) continue;

                        FDialogueAction Action;
                        FString ActionTypeStr = ActionObj->GetStringField(TEXT("type"));

                        if (ActionTypeStr == TEXT("end_dialogue"))
                        {
                            Action.ActionType = EDialogueActionType::EndDialogue;
                        }
                        else if (ActionTypeStr == TEXT("unlock_exit"))
                        {
                            Action.ActionType = EDialogueActionType::UnlockExit;
                            if (ActionObj->HasField(TEXT("exit_id")))
                            {
                                Action.ExitId = ActionObj->GetStringField(TEXT("exit_id"));
                            }
                        }
                        else if (ActionTypeStr == TEXT("give_item"))
                        {
                            Action.ActionType = EDialogueActionType::GiveItem;
                            if (ActionObj->HasField(TEXT("item_id")))
                            {
                                Action.ItemId = ActionObj->GetStringField(TEXT("item_id"));
                            }
                        }
                        else if (ActionTypeStr == TEXT("start_quest"))
                        {
                            Action.ActionType = EDialogueActionType::StartQuest;
                            if (ActionObj->HasField(TEXT("quest_id")))
                            {
                                Action.QuestId = ActionObj->GetStringField(TEXT("quest_id"));
                            }
                        }
                        else if (ActionTypeStr == TEXT("change_faction"))
                        {
                            Action.ActionType = EDialogueActionType::ChangeNPCFaction;
                            if (ActionObj->HasField(TEXT("npc_id")))
                            {
                                Action.NPCId = ActionObj->GetStringField(TEXT("npc_id"));
                            }
                            if (ActionObj->HasField(TEXT("new_faction")))
                            {
                                Action.NewFaction = ActionObj->GetStringField(TEXT("new_faction"));
                            }
                        }
                        else
                        {
                            Action.ActionType = EDialogueActionType::Custom;
                            Action.CustomData = ActionTypeStr;
                        }

                        Actions.Add(Action);
                    }
                }
            }

            // Parse choice actions (which choices trigger end_dialogue)
            TMap<int32, FDialogueAction> ChoiceActionMap;
            if (JsonObject->HasField(TEXT("choice_actions")))
            {
                const TSharedPtr<FJsonObject>* ChoiceActionsObj;
                if (JsonObject->TryGetObjectField(TEXT("choice_actions"), ChoiceActionsObj))
                {
                    for (auto It = (*ChoiceActionsObj)->Values.CreateIterator(); It; ++It)
                    {
                        int32 ChoiceIndex = FCString::Atoi(*It.Key());
                        const TSharedPtr<FJsonValue>& ActionVal = It.Value();
                        if (ActionVal->Type == EJson::Object)
                        {
                            const TSharedPtr<FJsonObject>& ActionObj = ActionVal->AsObject();
                            FString ActionTypeStr = ActionObj->GetStringField(TEXT("type"));
                            FDialogueAction ChoiceAction;
                            if (ActionTypeStr == TEXT("end_dialogue"))
                            {
                                ChoiceAction.ActionType = EDialogueActionType::EndDialogue;
                            }
                            else if (ActionTypeStr == TEXT("unlock_exit"))
                            {
                                ChoiceAction.ActionType = EDialogueActionType::UnlockExit;
                                if (ActionObj->HasField(TEXT("exit_id")))
                                {
                                    ChoiceAction.ExitId = ActionObj->GetStringField(TEXT("exit_id"));
                                }
                            }
                            ChoiceActionMap.Add(ChoiceIndex, ChoiceAction);
                        }
                        else if (ActionVal->Type == EJson::String)
                        {
                            FString ActionStr = ActionVal->AsString();
                            if (ActionStr == TEXT("end_dialogue"))
                            {
                                FDialogueAction ChoiceAction;
                                ChoiceAction.ActionType = EDialogueActionType::EndDialogue;
                                ChoiceActionMap.Add(ChoiceIndex, ChoiceAction);
                            }
                        }
                    }
                }
            }

            LastNPChoices = Choices;

            // Store pending choice actions
            PendingChoiceActions = ChoiceActionMap;

            if (bInitialChoicesOnlyResponse)
            {
                if (ActiveDialogueWidget)
                {
                    ActiveDialogueWidget->ShowChoices(Choices);
                }

                if (Actions.Num() > 0)
                {
                    ExecuteDialogueActions(Actions);
                }
                return;
            }

            // Add NPC response to history
            AddDialogueMessage(TEXT("assistant"), ResponseText);

            // Show response in widget
            if (ActiveDialogueWidget)
            {
                ActiveDialogueWidget->ShowNPCResponse(CurrentNPCData.Name, ResponseText, Choices);
            }

            // Execute immediate actions (not choice-dependent)
            if (Actions.Num() > 0)
            {
                ExecuteDialogueActions(Actions);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to generate NPC dialogue: %s"), *Response.Error);

        // Show error in widget
        if (ActiveDialogueWidget)
        {
            ActiveDialogueWidget->ShowNPCResponse(CurrentNPCData.Name,
                TEXT("I'm sorry, I seem to have lost my train of thought. Could you repeat that?"),
                TArray<FString>());
        }
    }
}

FString UChunkManagerComponent::BuildDialogueContextInfo() const
{
    FString ExitsInfo;
    if (CurrentChunk)
    {
        const FChunkData& ChunkData = CurrentChunk->GetChunkData();
        if (ChunkData.ExitPoints.Num() > 0)
        {
            ExitsInfo += TEXT("\nExits in current chunk:\n");
            for (const FExitPoint& Exit : ChunkData.ExitPoints)
            {
                if (Exit.TargetChunkId.IsEmpty())
                {
                    // No target assigned - can be unlocked to generate a new area
                    ExitsInfo += FString::Printf(TEXT("  - %s (direction: %.0f) - unexplored, can use unlock_exit to generate new area\n"),
                        *Exit.ExitId, Exit.ExitDirection);
                }
                else
                {
                    // Target assigned - portal already available, leads to known area
                    FString TargetName = GetChunkNameById(Exit.TargetChunkId);
                    ExitsInfo += FString::Printf(TEXT("  - %s (direction: %.0f) - available, leads to: %s (do NOT use unlock_exit on this exit)\n"),
                        *Exit.ExitId, Exit.ExitDirection,
                        !TargetName.IsEmpty() ? *TargetName : TEXT("known area"));
                }
            }
        }
    }

    FString ContextInfo = FString::Printf(
        TEXT("=== GAME STATE ===\n")
        TEXT("World: %s\n")
        TEXT("Era: %s\n")
        TEXT("Story Theme: %s\n")
        TEXT("Current Location: %s\n")
        TEXT("Current NPC: %s (ID: %s)\n")
        TEXT("NPC Personality: %s\n")
        TEXT("NPC Faction: %s\n")
        TEXT("NPC Dialogue Style: %s\n")
        TEXT("%s")
        TEXT("=== END GAME STATE ==="),
        *WorldConfig.WorldName,
        *WorldConfig.Era,
        WorldConfig.StoryTheme.IsEmpty() ? TEXT("None specified") : *WorldConfig.StoryTheme,
        CurrentChunk ? *CurrentChunk->GetChunkData().Name : TEXT("Unknown"),
        *CurrentNPCData.Name,
        *CurrentNPCData.NPCId,
        *CurrentNPCData.Personality,
        *CurrentNPCData.Faction,
        *CurrentNPCData.DialogueStyle,
        *ExitsInfo
    );

    return ContextInfo;
}

FString UChunkManagerComponent::BuildWorldGenerationSystemPrompt() const
{
    FString Result = TEXT("你是一个RPG游戏世界观设计师。根据玩家的描述生成初始世界配置。\n\n");
    Result += TEXT("【重要】输出语言必须与玩家输入的世界描述语言一致。判断语言时，以 \"details:\" 之后玩家实际填写的内容为准，不要以提示词中的英文格式文本（如 \"Create a...style world\"）为依据。玩家用中文描述，所有名称和对话就用中文；玩家用英文描述，就用英文。\n\n");
    Result += TEXT("【重要】必须在JSON中优先输出initial_chunk，确保流式传输时最早到达。将其放在world_id之后。\n\n");
    Result += TEXT("JSON structure:\n");
    Result += TEXT("{\n");
    Result += TEXT("    \"world_id\": \"unique_id\",\n");
    Result += TEXT("    \"initial_chunk\": {\n");
    Result += TEXT("        \"chunk_id\": \"chunk_001\",\n");
    Result += TEXT("        \"name\": \"Starting Location\",\n");
    Result += TEXT("        \"description\": \"Area description\",\n");
    Result += TEXT("        \"type\": \"Village/Town/Forest/Dungeon/etc\",\n");
    Result += TEXT("        \"tags\": [\"tag1\"],\n");
    Result += TEXT("        \"npcs\": [{\"name\":\"NPC Name\",\"description\":\"...\",\"personality\":\"...\",\"dialogue_style\":\"...\",\"faction\":\"...\",\"initial_dialogue\":\"...\"}],\n");
    Result += TEXT("        \"exit_points\": [{\"exit_id\":\"exit_north\",\"direction\":0,\"target_chunk_id\":\"\"}]\n");
    Result += TEXT("    },\n");
    Result += TEXT("    \"world_name\": \"World Name\",\n");
    Result += TEXT("    \"description\": \"Brief world description\",\n");
    Result += TEXT("    \"era\": \"Time period\",\n");
    Result += TEXT("    \"geography\": \"Main geography\",\n");
    Result += TEXT("    \"factions\": [\"Faction 1\"],\n");
    Result += TEXT("    \"rules\": [\"Rule 1\"],\n");
    Result += TEXT("    \"story_theme\": \"Main theme\",\n");
    Result += TEXT("    \"player_appearance\": \"主角外貌描述，用于生成角色图。包含性别、年龄、种族、发型、服装、显著特征等\"\n");
    Result += TEXT("}\n\n");
    Result += TEXT("Exit directions (degrees): 0=North, 90=East, 180=South, 270=West. Use only these 4 directions. NPC positions and portal positions are auto-placed by the game engine.\n");
    Result += TEXT("Requirements: 1-2 NPCs and 2-4 exits in initial_chunk. Output valid JSON only.");
    return Result;
}

FString UChunkManagerComponent::BuildChunkGenerationSystemPrompt() const
{
    FString Result = TEXT("你是一个RPG游戏世界观设计师。根据上下文生成下一个场景分块。\n\n");
    Result += TEXT("【重要】输出语言必须与玩家输入的世界描述语言一致，参考游戏状态中的世界名称和NPC名称语言。\n\n");
    Result += TEXT("JSON format:\n");
    Result += TEXT("{\n");
    Result += TEXT("    \"chunk_id\": \"chunk_XXX\",\n");
    Result += TEXT("    \"name\": \"Location Name\",\n");
    Result += TEXT("    \"description\": \"Description\",\n");
    Result += TEXT("    \"type\": \"Village/Town/Forest/Dungeon/etc\",\n");
    Result += TEXT("    \"tags\": [\"tag1\"],\n");
    Result += TEXT("    \"npcs\": [{\"name\":\"NPC Name\",\"description\":\"...\",\"personality\":\"...\",\"dialogue_style\":\"...\",\"faction\":\"...\",\"initial_dialogue\":\"...\"}],\n");
    Result += TEXT("    \"exit_points\": [{\"exit_id\":\"exit_XXX\",\"direction\":0}]\n");
    Result += TEXT("}\n\n");
    Result += TEXT("Exit directions (degrees): 0=North, 90=East, 180=South, 270=West. Use only these 4 directions. NPC positions and portal positions are auto-placed by the game engine.\n");
    Result += TEXT("【关键】新生成的chunk必须包含一个“返回来源chunk”的反向出口：如果玩家是从某个方向进入，就必须提供位于相反方向的exit，供系统创建回程传送门。\n");
    Result += TEXT("【关键】不要省略这个反向出口；它可以有任意合理的exit_id，但direction必须是进入方向的反方向。\n");
    Result += TEXT("1-2 NPCs, 2-4 exits. Consider story progression. Valid JSON only.");
    return Result;
}

FString UChunkManagerComponent::BuildDialogueSystemPrompt() const
{
    FString Result = TEXT("你是一个RPG游戏中的NPC角色。请代入角色，以角色的身份与玩家对话。\n\n");
    Result += TEXT("【重要】回复语言必须与玩家的输入语言一致。玩家用中文，你就用中文回复；玩家用英文，你就用英文回复。\n\n");
    Result += TEXT("必须使用JSON格式回复，格式如下：\n");
    Result += TEXT("{\n");
    Result += TEXT("  \"response_text\": \"角色回复内容，1-3句话，符合角色性格\",\n");
    Result += TEXT("  \"npc_expression\": \"happy\",\n");
    Result += TEXT("  \"player_expression\": \"neutral\",\n");
    Result += TEXT("  \"choices\": [\"选项1\", \"选项2\", \"选项3\"],\n");
    Result += TEXT("  \"actions\": [{\"type\": \"action_type\", ...}],\n");
    Result += TEXT("  \"choice_actions\": {\"0\": \"end_dialogue\", \"2\": {\"type\": \"unlock_exit\", \"exit_id\": \"exit_north\"}}\n");
    Result += TEXT("}\n\n");
    Result += TEXT("规则：\n");
    Result += TEXT("- response_text：1-3句沉浸式的角色台词\n");
    Result += TEXT("- npc_expression：NPC当前表情，可选值: happy（开心）, angry（愤怒）, sad（悲伤）, neutral（平静）。根据台词情绪选择\n");
    Result += TEXT("- player_expression：玩家角色当前表情，可选值同npc_expression。根据语境推断玩家可能的情绪反应\n");
    Result += TEXT("- choices：2-4个玩家对话选项。如果没有选择项，使用空数组[]\n");
    Result += TEXT("- actions：（可选）立即执行的动作列表，见下方动作说明\n");
    Result += TEXT("- choice_actions：（可选）选项对应的动作，键为选项索引（从0开始）\n");
    Result += TEXT("- 始终保持角色身份，不要打破第四面墙\n");
    Result += TEXT("- 匹配NPC的性格、阵营和对话风格\n");
    Result += TEXT("- 保持对话自然，不要过于冗长\n\n");
    Result += TEXT("可用动作类型（actions / choice_actions）：\n");
    Result += TEXT("1. end_dialogue - 结束对话，关闭对话界面\n");
    Result += TEXT("   【重要】仅用于玩家明确表示要离开/再见/结束对话的选项，如'再见'、'我走了'、'告辞'等\n");
    Result += TEXT("   【重要】询问问题、继续对话、获取信息的选项绝对不能标记为 end_dialogue\n");
    Result += TEXT("   示例：{\"type\": \"end_dialogue\"}\n");
    Result += TEXT("2. unlock_exit - 解锁一个出口，生成并创建通往新区域的传送门\n");
    Result += TEXT("   当玩家选择前往某个方向或NPC同意让玩家通过时使用\n");
    Result += TEXT("   示例：{\"type\": \"unlock_exit\", \"exit_id\": \"exit_north\"}\n");
    Result += TEXT("   （出口ID从上下文中的当前位置出口列表获取，只传exit_id即可）\n");
    Result += TEXT("3. give_item - 给予玩家物品\n");
    Result += TEXT("   示例：{\"type\": \"give_item\", \"item_id\": \"sword_001\"}\n");
    Result += TEXT("4. start_quest - 开始任务\n");
    Result += TEXT("   示例：{\"type\": \"start_quest\", \"quest_id\": \"quest_find_cat\"}\n\n");
    Result += TEXT("使用建议：\n");
    Result += TEXT("- end_dialogue 只能用于离开/再见类选项，其他选项一律不加\n");
    Result += TEXT("- 对话界面已有默认的'离开'按钮，通常不需要额外添加离开选项\n");
    Result += TEXT("- unlock_exit 会触发新区域生成和传送门创建，玩家随后可以进入传送门前往\n");
    Result += TEXT("- 大多数选项只是继续对话，不需要标记任何动作，留空即可");
    return Result;
}

void UChunkManagerComponent::LoadChunk(const FString& ChunkId)
{
    if (LoadedChunks.Contains(ChunkId))
    {
        AGameChunkActor* Chunk = LoadedChunks[ChunkId];
        if (Chunk)
        {
            CurrentChunk = Chunk;
            OnChunkLoaded.Broadcast(Chunk);

            if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
            {
                Player->SetActorLocation(Chunk->GetPlayerStartTransform().GetLocation());
            }
        }
    }
}
