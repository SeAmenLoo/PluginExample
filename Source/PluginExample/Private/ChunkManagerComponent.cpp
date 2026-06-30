#include "ChunkManagerComponent.h"
#include "GameChunkActor.h"
#include "NPCCharacter.h"
#include "DialogueWidget.h"
#include "LLMLoggerSubsystem.h"
#include "LLMRuntime/Public/LLMCacheSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blueprint/UserWidget.h"
#include "JsonObjectConverter.h"
#include "LLMRuntime/Public/LLMChatCompletionAsyncAction.h"

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
}

void UChunkManagerComponent::Deinitialize()
{
    if (ActiveDialogueWidget)
    {
        ActiveDialogueWidget->RemoveFromParent();
        ActiveDialogueWidget = nullptr;
    }

    Super::Deinitialize();
}

void UChunkManagerComponent::GenerateWorldFromPrompt(const FString& WorldPrompt)
{
    FString ActualPrompt = WorldPrompt;

    // In replay mode, auto-fill WorldPrompt from cache if user didn't provide one
    if (bUseCacheReplay && WorldPrompt.IsEmpty() && CacheSubsystem)
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
    Messages.Add(FLLMChatMessage{TEXT("user"), WorldPrompt});

    // Check cache first
    if (bEnableCache && CacheSubsystem)
    {
        FLLMCacheEntry CachedEntry;
        if (CacheSubsystem->GetCachedResponse(Messages, CachedEntry))
        {
            UE_LOG(LogTemp, Log, TEXT("[Cache] World generation cache hit! Replay mode: %d"), bUseCacheReplay);

            if (bUseCacheReplay)
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
        else if (bUseCacheReplay)
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
                Exit.ExitDirection = ExitObj->GetNumberField(TEXT("direction"));

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

                if (ExitObj->HasField(TEXT("target_chunk_id")))
                {
                    Exit.TargetChunkId = ExitObj->GetStringField(TEXT("target_chunk_id"));
                }

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
                        Exit.ExitDirection = ExitObj->GetNumberField(TEXT("direction"));

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

                        if (ExitObj->HasField(TEXT("target_chunk_id")))
                        {
                            Exit.TargetChunkId = ExitObj->GetStringField(TEXT("target_chunk_id"));
                        }

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

                FRotator NPCRotation = FRotator(0, NPCFacingYaw, 0);

                ANPCCharacter* NPC = GetWorld()->SpawnActor<ANPCCharacter>(NPCCharacterClass, NPCLocation, NPCRotation, SpawnParams);
                if (NPC)
                {
                    NPC->SetNPCData(NPCData);
                    NewChunk->SpawnedNPCs.Add(NPC);
                }
            }
        }

        LoadedChunks.Add(ChunkData.ChunkId, NewChunk);
        CurrentChunk = NewChunk;

        // Update portals in all chunks after new chunk is loaded
        UpdateAllChunkPortals();

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
            Player->SetActorLocation(TargetChunk->GetPlayerStartTransform().GetLocation());
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

    // Check cache first
    FString CacheContext = FString::Printf(TEXT("from=%s exit=%s"), *FromChunkId, *ExitId);
    if (bEnableCache && CacheSubsystem)
    {
        FLLMCacheEntry CachedEntry;
        if (CacheSubsystem->GetCachedResponse(Messages, CachedEntry))
        {
            UE_LOG(LogTemp, Log, TEXT("[Cache] Chunk generation cache hit! Replay mode: %d"), bUseCacheReplay);

            if (bUseCacheReplay)
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
        else if (bUseCacheReplay)
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
                    Exit.ExitDirection = ExitObj->GetNumberField(TEXT("direction"));

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

void UChunkManagerComponent::StartDialogue(ANPCCharacter* NPC)
{
    if (!NPC || !DialogueWidgetClass)
    {
        return;
    }

    CurrentDialogueNPC = NPC;
    CurrentNPCData = NPC->GetNPCData();
    DialogueMessages.Empty();
    LastNPChoices.Empty();
    bIsWaitingForResponse = false;

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

    // Disable player input to game
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->DisableInput(PC);
    }

    OnDialogueStarted.Broadcast(NPC, CurrentNPCData.InitialDialogue);

    // Request next dialogue turn from LLM
    SendDialogueRequest();
}

void UChunkManagerComponent::EndDialogue()
{
    CurrentDialogueNPC = nullptr;
    CurrentNPCData = FNPCData();
    DialogueMessages.Empty();
    LastNPChoices.Empty();
    bIsWaitingForResponse = false;

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
        // Add choice as user message
        FString ChoiceText = FString::Printf(TEXT("[CHOICE:%d] %s"), ChoiceIndex + 1, *LastNPChoices[ChoiceIndex]);
        AddDialogueMessage(TEXT("user"), ChoiceText);

        // Request LLM response
        SendDialogueRequest();
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

void UChunkManagerComponent::SendDialogueRequest()
{
    if (!CurrentDialogueNPC || bIsWaitingForResponse)
    {
        return;
    }

    bIsWaitingForResponse = true;

    if (ActiveDialogueWidget)
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

    // Check cache first
    FString DialogueCacheContext = FString::Printf(TEXT("npc=%s"), *CurrentNPCData.Name);
    if (bEnableCache && CacheSubsystem)
    {
        FLLMCacheEntry CachedEntry;
        if (CacheSubsystem->GetCachedResponse(Messages, CachedEntry))
        {
            UE_LOG(LogTemp, Log, TEXT("[Cache] Dialogue cache hit! Replay mode: %d"), bUseCacheReplay);

            if (bUseCacheReplay)
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
        else if (bUseCacheReplay)
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

    if (ActiveDialogueWidget)
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

            // Add NPC response to history
            AddDialogueMessage(TEXT("assistant"), ResponseText);
            LastNPChoices = Choices;

            // Show response in widget
            if (ActiveDialogueWidget)
            {
                ActiveDialogueWidget->ShowNPCResponse(CurrentNPCData.Name, ResponseText, Choices);
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
        TEXT("=== END GAME STATE ==="),
        *WorldConfig.WorldName,
        *WorldConfig.Era,
        WorldConfig.StoryTheme.IsEmpty() ? TEXT("None specified") : *WorldConfig.StoryTheme,
        CurrentChunk ? *CurrentChunk->GetChunkData().Name : TEXT("Unknown"),
        *CurrentNPCData.Name,
        *CurrentNPCData.NPCId,
        *CurrentNPCData.Personality,
        *CurrentNPCData.Faction,
        *CurrentNPCData.DialogueStyle
    );

    return ContextInfo;
}

FString UChunkManagerComponent::BuildWorldGenerationSystemPrompt() const
{
    FString Result = TEXT("你是一个RPG游戏世界观设计师。根据玩家的描述生成初始世界配置。\n\n");
    Result += TEXT("【重要】输出语言必须与用户输入语言一致（用户用中文，你就用中文；用户用英文，你就用英文）。\n\n");
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
    Result += TEXT("    \"story_theme\": \"Main theme\"\n");
    Result += TEXT("}\n\n");
    Result += TEXT("Exit directions (degrees): 0=North, 90=East, 180=South, 270=West. Use only these 4 directions. NPC positions and portal positions are auto-placed by the game engine.\n");
    Result += TEXT("Requirements: 1-2 NPCs and 2-4 exits in initial_chunk. Output valid JSON only.");
    return Result;
}

FString UChunkManagerComponent::BuildChunkGenerationSystemPrompt() const
{
    FString Result = TEXT("你是一个RPG游戏世界观设计师。根据上下文生成下一个场景分块。\n\n");
    Result += TEXT("【重要】输出语言必须与用户输入语言一致。\n\n");
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
    Result += TEXT("1-2 NPCs, 2-4 exits. Consider story progression. Valid JSON only.");
    return Result;
}

FString UChunkManagerComponent::BuildDialogueSystemPrompt() const
{
    FString Result = TEXT("你是一个RPG游戏中的NPC角色。请代入角色，以角色的身份与玩家对话。\n\n");
    Result += TEXT("【重要】回复语言必须与玩家的输入语言一致。玩家用中文，你就用中文回复；玩家用英文，你就用英文回复。\n\n");
    Result += TEXT("必须使用JSON格式回复，格式如下：\n");
    Result += TEXT("{\"response_text\":\"角色回复内容，1-3句话，符合角色性格\",\"choices\":[\"选项1\",\"选项2\",\"选项3\"]}\n\n");
    Result += TEXT("规则：\n");
    Result += TEXT("- response_text：1-3句沉浸式的角色台词\n");
    Result += TEXT("- choices：2-4个玩家对话选项。如果没有选择项，使用空数组[]\n");
    Result += TEXT("- 始终保持角色身份，不要打破第四面墙\n");
    Result += TEXT("- 匹配NPC的性格、阵营和对话风格\n");
    Result += TEXT("- 保持对话自然，不要过于冗长");
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
