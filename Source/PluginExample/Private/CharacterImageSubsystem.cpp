#include "CharacterImageSubsystem.h"
#include "CharacterImageGenerationLibrary.h"
#include "NPCCharacter.h"
#include "PlayerCharacter.h"
#include "DialogueWidget.h"
#include "GameDataTypes.h"
#include "ChunkManagerComponent.h"
#include "AICharacterImageSetAsyncAction.h"
#include "Engine/Texture2D.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

void UCharacterImageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("[CharacterImageSubsystem] Initialized"));
}

void UCharacterImageSubsystem::Deinitialize()
{
    PendingActions.Empty();
    CachedPixelFlipbooks.Empty();
    CachedPortraitTextures.Empty();
    CachedExpressionTextures.Empty();
    GeneratingCharacterIds.Empty();
    TrackedNPCs.Empty();
    TrackedPlayer = nullptr;
    Super::Deinitialize();
}

bool UCharacterImageSubsystem::HasCachedImageSet(const FString& CharacterId) const
{
    if (CharacterId.IsEmpty())
    {
        return false;
    }

    return CachedPixelFlipbooks.Contains(CharacterId);
}

void UCharacterImageSubsystem::RequestCharacterImageForNPC(ANPCCharacter* NPC, UDialogueWidget* DialogueWidget)
{
    if (!NPC)
    {
        return;
    }

    const FString CharacterId = NPC->GetNPCId();
    if (CharacterId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterImageSubsystem] NPC has empty NPCId, skipping"));
        return;
    }

    RequestCharacterImageInternal(CharacterId, NPC, nullptr, DialogueWidget);
}

void UCharacterImageSubsystem::RequestPlayerCharacterImage(APlayerCharacter* PlayerCharacter)
{
    if (!PlayerCharacter)
    {
        return;
    }

    const FString CharacterId = PlayerCharacter->GetPlayerImageId();
    if (CharacterId.IsEmpty())
    {
        return;
    }

    RequestCharacterImageInternal(CharacterId, nullptr, PlayerCharacter, nullptr);
}

void UCharacterImageSubsystem::RequestCharacterImageInternal(
    const FString& CharacterId,
    ANPCCharacter* NPC,
    APlayerCharacter* PlayerCharacter,
    UDialogueWidget* DialogueWidget)
{
    // 1. If already cached, apply immediately
    if (HasCachedImageSet(CharacterId))
    {
        UE_LOG(LogTemp, Log, TEXT("[CharacterImageSubsystem] Cache hit for character %s"), *CharacterId);

        if (NPC)
        {
            ApplyToNPCFlipbooks(NPC, FAICharacterImageSet{});
        }
        if (PlayerCharacter)
        {
            ApplyToPlayerFlipbooks(PlayerCharacter, FAICharacterImageSet{});
        }
        if (DialogueWidget && NPC)
        {
            ApplyCachedImageToDialogue(CharacterId, DialogueWidget);
        }
        return;
    }

    // 2. If generation is in progress, do nothing (callback will fire when ready)
    if (GeneratingCharacterIds.Contains(CharacterId))
    {
        UE_LOG(LogTemp, Log, TEXT("[CharacterImageSubsystem] Character %s image generation already in progress"), *CharacterId);
        return;
    }

    // 3. If disk cache exists, try to load from disk first
    FAICharacterImageSet DiskCached;
    if (TryLoadFlipbookTextures(CharacterId, DiskCached))
    {
        if (NPC)
        {
            ApplyToNPCFlipbooks(NPC, DiskCached);
        }
        if (PlayerCharacter)
        {
            ApplyToPlayerFlipbooks(PlayerCharacter, DiskCached);
        }
        if (DialogueWidget)
        {
            ApplyToDialogueWidget(DialogueWidget, DiskCached);
        }
        return;
    }

    // 4. Otherwise, fire async image generation
    GeneratingCharacterIds.Add(CharacterId);
    if (NPC)
    {
        TrackedNPCs.Add(CharacterId, NPC);
    }
    if (PlayerCharacter)
    {
        TrackedPlayer = PlayerCharacter;
    }

    FAICharacterGenerationRequest Request;
    if (NPC)
    {
        Request = BuildImageRequestForNPC(NPC);
    }
    else if (PlayerCharacter)
    {
        Request = BuildImageRequestForPlayer(PlayerCharacter);
    }

    UAICharacterImageSetAsyncAction* Action = UAICharacterImageSetAsyncAction::GenerateCharacterImageSet(this, Request);
    if (Action)
    {
        Action->Completed.AddDynamic(this, &UCharacterImageSubsystem::HandleImageSetResponse);
        PendingActions.Add(CharacterId, Action);
        Action->Activate();
    }
    else
    {
        GeneratingCharacterIds.Remove(CharacterId);
        UE_LOG(LogTemp, Error, TEXT("[CharacterImageSubsystem] Failed to create image generation action for character %s"), *CharacterId);
        OnCharacterImageFailed.Broadcast(CharacterId, TEXT("Failed to create image generation action"));
    }
}

void UCharacterImageSubsystem::ApplyCachedImageToDialogue(const FString& NPCId, UDialogueWidget* DialogueWidget)
{
    if (!DialogueWidget || NPCId.IsEmpty())
    {
        return;
    }

    FAICharacterImageSet ImageSet;
    if (TryLoadFlipbookTextures(NPCId, ImageSet))
    {
        ApplyToDialogueWidget(DialogueWidget, ImageSet);
    }
}

void UCharacterImageSubsystem::CancelRequestsForCharacter(const FString& CharacterId)
{
    if (TObjectPtr<UAICharacterImageSetAsyncAction>* ActionPtr = PendingActions.Find(CharacterId))
    {
        if (*ActionPtr)
        {
            (*ActionPtr)->SetReadyToDestroy();
        }
        PendingActions.Remove(CharacterId);
    }
    GeneratingCharacterIds.Remove(CharacterId);
}

FAICharacterGenerationRequest UCharacterImageSubsystem::BuildImageRequestForNPC(ANPCCharacter* NPC) const
{
    if (!NPC)
    {
        return FAICharacterGenerationRequest{};
    }

    FNPCData Data = NPC->GetNPCData();

    FAICharacterSpec Spec;
    Spec.Name = Data.Name;
    Spec.Personality = Data.Personality;
    Spec.Job = Data.Faction;

    FAICharacterGenerationRequest Request;
    Request.Character = Spec;
    Request.FilePrefix = Data.NPCId;
    Request.SaveDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CharacterImages"), Data.NPCId);

    return Request;
}

FAICharacterGenerationRequest UCharacterImageSubsystem::BuildImageRequestForPlayer(APlayerCharacter* PlayerCharacter) const
{
    if (!PlayerCharacter)
    {
        return FAICharacterGenerationRequest{};
    }

    FAICharacterSpec Spec;
    Spec.Name = TEXT("Player");
    Spec.Personality = TEXT(" courageous hero");
    Spec.Job = TEXT("Adventurer");

    // Build description from world-configured player appearance if available
    FString AppearanceDesc;
    if (UGameInstance* GameInst = GetGameInstance())
    {
        if (UChunkManagerComponent* ChunkMgr = GameInst->GetSubsystem<UChunkManagerComponent>())
        {
            FWorldConfig WorldConfig = ChunkMgr->GetWorldConfig();
            if (!WorldConfig.PlayerAppearance.IsEmpty())
            {
                AppearanceDesc = WorldConfig.PlayerAppearance;
            }
        }
    }

    if (!AppearanceDesc.IsEmpty())
    {
        Spec.Personality = AppearanceDesc + TEXT("，") + Spec.Personality;
    }

    FAICharacterGenerationRequest Request;
    Request.Character = Spec;
    Request.FilePrefix = PlayerCharacter->GetPlayerImageId();
    Request.SaveDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CharacterImages"), PlayerCharacter->GetPlayerImageId());

    return Request;
}

void UCharacterImageSubsystem::HandleImageSetResponse(const FAICharacterGenerationResponse& Response)
{
    FString MatchedCharacterId;

    for (const TPair<FString, TObjectPtr<UAICharacterImageSetAsyncAction>>& Pair : PendingActions)
    {
        if (Pair.Value)
        {
            MatchedCharacterId = Pair.Key;
            break;
        }
    }

    if (MatchedCharacterId.IsEmpty())
    {
        return;
    }

    PendingActions.Remove(MatchedCharacterId);
    GeneratingCharacterIds.Remove(MatchedCharacterId);

    if (!Response.bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("[CharacterImageSubsystem] Image generation failed for character %s: %s"),
            *MatchedCharacterId, *Response.Error);
        OnCharacterImageFailed.Broadcast(MatchedCharacterId, Response.Error);
        return;
    }

    OnImageReadyForCharacter(MatchedCharacterId, Response.Images);
}

void UCharacterImageSubsystem::OnImageReadyForCharacter(const FString& CharacterId, const FAICharacterImageSet& ImageSet)
{
    if (TObjectPtr<ANPCCharacter>* NPCPtr = TrackedNPCs.Find(CharacterId))
    {
        if (*NPCPtr)
        {
            ApplyToNPCFlipbooks(*NPCPtr, ImageSet);
        }
    }

    if (TrackedPlayer && CharacterId == TrackedPlayer->GetPlayerImageId())
    {
        ApplyToPlayerFlipbooks(TrackedPlayer, ImageSet);
    }

    if (TObjectPtr<ANPCCharacter>* NPCPtr = TrackedNPCs.Find(CharacterId))
    {
        if (*NPCPtr)
        {
            FNPCData Data = (*NPCPtr)->GetNPCData();
            FRPGNPCProfileConfig Profile;
            Profile.NPCId = Data.NPCId;
            Profile.NPCName = Data.Name;
            Profile.NPCDescription = Data.Description;
            UCharacterImageGenerationLibrary::SaveNPCProfileCache(Profile);
        }
    }

    // Cache expression textures for dialogue portrait switching
    auto CacheExpression = [this, &CharacterId](const FAICharacterImageArtifact& Artifact, const FString& ExpressionName)
    {
        if (!Artifact.Image.LocalFilePath.IsEmpty() && FPaths::FileExists(Artifact.Image.LocalFilePath))
        {
            UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Artifact.Image.LocalFilePath);
            if (Texture)
            {
                FString Key = CharacterId + TEXT("|") + ExpressionName.ToLower();
                CachedExpressionTextures.Add(Key, Texture);
            }
        }
    };
    CacheExpression(ImageSet.DetailAnchor, TEXT("neutral"));
    CacheExpression(ImageSet.Happy, TEXT("happy"));
    CacheExpression(ImageSet.Angry, TEXT("angry"));
    CacheExpression(ImageSet.Sad, TEXT("sad"));

    OnCharacterImageReady.Broadcast(CharacterId);
}

void UCharacterImageSubsystem::ApplyImageSetToActors(const FString& CharacterId, const FAICharacterImageSet& ImageSet)
{
    UE_LOG(LogTemp, Log, TEXT("[CharacterImageSubsystem] Applied image set to actors for character %s"), *CharacterId);
}

void UCharacterImageSubsystem::ApplyToNPCFlipbooks(ANPCCharacter* NPC, const FAICharacterImageSet& ImageSet)
{
    if (!NPC)
    {
        return;
    }

    const FString CharacterId = NPC->GetNPCId();

    if (TObjectPtr<UPaperFlipbook>* PixelPtr = CachedPixelFlipbooks.Find(CharacterId))
    {
        if (*PixelPtr)
        {
            NPC->SetPixelFlipbook(*PixelPtr);
        }
    }
}

void UCharacterImageSubsystem::ApplyToPlayerFlipbooks(APlayerCharacter* PlayerCharacter, const FAICharacterImageSet& ImageSet)
{
    if (!PlayerCharacter)
    {
        return;
    }

    const FString CharacterId = PlayerCharacter->GetPlayerImageId();

    if (TObjectPtr<UPaperFlipbook>* PixelPtr = CachedPixelFlipbooks.Find(CharacterId))
    {
        if (*PixelPtr)
        {
            PlayerCharacter->SetPixelFlipbook(*PixelPtr);
        }
    }
}

UTexture2D* UCharacterImageSubsystem::GetExpressionPortrait(const FString& CharacterId, const FString& Expression) const
{
    if (CharacterId.IsEmpty() || Expression.IsEmpty())
    {
        return nullptr;
    }

    FString Key = CharacterId + TEXT("|") + Expression.ToLower();
    if (const TObjectPtr<UTexture2D>* TexPtr = CachedExpressionTextures.Find(Key))
    {
        return *TexPtr;
    }
    return nullptr;
}

void UCharacterImageSubsystem::ApplyToDialogueWidget(UDialogueWidget* DialogueWidget, const FAICharacterImageSet& ImageSet)
{
    if (!DialogueWidget)
    {
        return;
    }

    UTexture2D* PortraitTexture = nullptr;

    if (ImageSet.DetailAnchor.Image.LocalFilePath.Len() > 0 && FPaths::FileExists(ImageSet.DetailAnchor.Image.LocalFilePath))
    {
        PortraitTexture = LoadObject<UTexture2D>(nullptr, *ImageSet.DetailAnchor.Image.LocalFilePath);
    }

    if (PortraitTexture)
    {
        DialogueWidget->SetNPCPortrait(PortraitTexture);
    }
}

bool UCharacterImageSubsystem::TryLoadFlipbookTextures(const FString& CharacterId, FAICharacterImageSet& OutImageSet)
{
    if (CharacterId.IsEmpty())
    {
        return false;
    }

    FString CacheDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CharacterImages"), CharacterId);

    if (!FPaths::DirectoryExists(CacheDir))
    {
        return false;
    }

    bool bAnyLoaded = false;

    auto TryLoadImage = [&CacheDir, &bAnyLoaded](const FString& FileName, FAIImageResult& OutImage) -> bool
    {
        FString FullPath = FPaths::Combine(CacheDir, FileName);
        if (FPaths::FileExists(FullPath))
        {
            OutImage.LocalFilePath = FullPath;
            bAnyLoaded = true;
            return true;
        }
        return false;
    };

    auto TryLoadExpression = [this, &CacheDir, &CharacterId, &bAnyLoaded](const FString& FileName, const FString& ExpressionName) -> UTexture2D*
    {
        FString FullPath = FPaths::Combine(CacheDir, FileName);
        if (FPaths::FileExists(FullPath))
        {
            UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *FullPath);
            if (Texture)
            {
                FString Key = CharacterId + TEXT("|") + ExpressionName.ToLower();
                CachedExpressionTextures.Add(Key, Texture);
                bAnyLoaded = true;
            }
            return Texture;
        }
        return nullptr;
    };

    TryLoadImage(TEXT("DetailAnchor.png"), OutImageSet.DetailAnchor.Image);
    TryLoadImage(TEXT("PixelAnchor.png"), OutImageSet.PixelAnchor.Image);
    TryLoadImage(TEXT("Happy.png"), OutImageSet.Happy.Image);
    TryLoadImage(TEXT("Angry.png"), OutImageSet.Angry.Image);
    TryLoadImage(TEXT("Sad.png"), OutImageSet.Sad.Image);
    TryLoadImage(TEXT("WalkLeft.png"), OutImageSet.WalkLeft.Image);
    TryLoadImage(TEXT("WalkRight.png"), OutImageSet.WalkRight.Image);

    // Also cache expression textures for dialogue portrait switching
    TryLoadExpression(TEXT("DetailAnchor.png"), TEXT("neutral"));
    TryLoadExpression(TEXT("Happy.png"), TEXT("happy"));
    TryLoadExpression(TEXT("Angry.png"), TEXT("angry"));
    TryLoadExpression(TEXT("Sad.png"), TEXT("sad"));

    return bAnyLoaded;
}