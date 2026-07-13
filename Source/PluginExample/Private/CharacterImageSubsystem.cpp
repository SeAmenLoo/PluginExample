#include "CharacterImageSubsystem.h"
#include "CharacterImageGenerationLibrary.h"
#include "NPCCharacter.h"
#include "PlayerCharacter.h"
#include "DialogueWidget.h"
#include "GameDataTypes.h"
#include "ChunkManagerComponent.h"
#include "AICharacterImageSetAsyncAction.h"
#include "LLMRuntime/Public/LLMCacheSubsystem.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"

namespace
{
    UTexture2D* LoadTextureFromFile(const FString& FilePath)
    {
        if (!FPaths::FileExists(FilePath))
        {
            return nullptr;
        }
        return FImageUtils::ImportFileAsTexture2D(FilePath);
    }

    FString GetStyleReferenceImagePath(const FString& FileName)
    {
        FString SavedResDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Res"));
        FString SavedPath = FPaths::Combine(SavedResDir, FileName);
        if (FPaths::FileExists(SavedPath))
        {
            return SavedPath;
        }

        FString ProjectResDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Res"));
        FString ProjectPath = FPaths::Combine(ProjectResDir, FileName);
        if (FPaths::FileExists(ProjectPath))
        {
            return ProjectPath;
        }

        return FString();
    }
}

void UCharacterImageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("[CharacterImageSubsystem] Initialized"));
}

void UCharacterImageSubsystem::Deinitialize()
{
    PendingActions.Empty();
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

    return CachedPortraitTextures.Contains(CharacterId + TEXT("|pixel"));
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
    // 1. If already cached in memory, apply immediately
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

    // 3. If disk cache exists, load from disk first
    FAICharacterImageSet DiskCached;
    if (TryLoadFlipbookTextures(CharacterId, DiskCached))
    {
        UE_LOG(LogTemp, Log, TEXT("[CharacterImageSubsystem] Disk cache hit for character %s"), *CharacterId);

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
    UE_LOG(LogTemp, Log, TEXT("[CharacterImageSubsystem] Starting image generation for character %s"), *CharacterId);
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
        Action->Progressed.AddDynamic(this, &UCharacterImageSubsystem::HandleImageSetProgress);
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
    // Combine visual description and personality for richer prompt context
    if (!Data.Description.IsEmpty() && !Data.Personality.IsEmpty())
    {
        Spec.Personality = Data.Description + TEXT("。") + Data.Personality;
    }
    else if (!Data.Description.IsEmpty())
    {
        Spec.Personality = Data.Description;
    }
    else
    {
        Spec.Personality = Data.Personality;
    }
    Spec.Job = Data.Faction;

    FAICharacterGenerationRequest Request;
    Request.Character = Spec;
    Request.FilePrefix = Data.NPCId;
    Request.SaveDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CharacterImages"), Data.NPCId);

    // Set style reference images for consistent art style
    FString DetailRefPath = GetStyleReferenceImagePath(TEXT("Detail.png"));
    if (!DetailRefPath.IsEmpty())
    {
        FAIImageReference DetailRef;
        DetailRef.LocalFilePath = DetailRefPath;
        Request.AdditionalReferences.Add(DetailRef);
    }

    FString PixelRefPath = GetStyleReferenceImagePath(TEXT("Pixel.png"));
    if (!PixelRefPath.IsEmpty())
    {
        FAIImageReference PixelRef;
        PixelRef.LocalFilePath = PixelRefPath;
        Request.PixelStyleReference = PixelRef;
    }
    else
    {
        PixelRefPath = GetStyleReferenceImagePath(TEXT("D_Pixel.png"));
        if (!PixelRefPath.IsEmpty())
        {
            FAIImageReference PixelRef;
            PixelRef.LocalFilePath = PixelRefPath;
            Request.PixelStyleReference = PixelRef;
        }
    }

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

    // Set style reference images for consistent art style
    FString DetailRefPath = GetStyleReferenceImagePath(TEXT("Detail.png"));
    if (!DetailRefPath.IsEmpty())
    {
        FAIImageReference DetailRef;
        DetailRef.LocalFilePath = DetailRefPath;
        Request.AdditionalReferences.Add(DetailRef);
    }

    FString PixelRefPath = GetStyleReferenceImagePath(TEXT("Pixel.png"));
    if (!PixelRefPath.IsEmpty())
    {
        FAIImageReference PixelRef;
        PixelRef.LocalFilePath = PixelRefPath;
        Request.PixelStyleReference = PixelRef;
    }
    else
    {
        PixelRefPath = GetStyleReferenceImagePath(TEXT("D_Pixel.png"));
        if (!PixelRefPath.IsEmpty())
        {
            FAIImageReference PixelRef;
            PixelRef.LocalFilePath = PixelRefPath;
            Request.PixelStyleReference = PixelRef;
        }
    }

    return Request;
}

void UCharacterImageSubsystem::HandleImageSetProgress(const FAICharacterGenerationResponse& Response)
{
    if (Response.CharacterId.IsEmpty())
    {
        return;
    }

    const bool bHasDetailReady = Response.CompletedSteps.Contains(TEXT("DetailAnchor"));
    const bool bHasPixelReady = Response.CompletedSteps.Contains(TEXT("PixelAnchor"));

    if (!bHasDetailReady && !bHasPixelReady)
    {
        return;
    }

    OnImageReadyForCharacter(Response.CharacterId, Response.Images);
}

void UCharacterImageSubsystem::HandleImageSetResponse(const FAICharacterGenerationResponse& Response)
{
    FString MatchedCharacterId = Response.CharacterId;

    if (MatchedCharacterId.IsEmpty())
    {
        for (const TPair<FString, TObjectPtr<UAICharacterImageSetAsyncAction>>& Pair : PendingActions)
        {
            if (Pair.Value)
            {
                MatchedCharacterId = Pair.Key;
                UE_LOG(LogTemp, Warning, TEXT("[CharacterImageSubsystem] Response missing CharacterId, falling back to first pending action: %s"), *MatchedCharacterId);
                break;
            }
        }
    }

    if (MatchedCharacterId.IsEmpty())
    {
        return;
    }

    PendingActions.Remove(MatchedCharacterId);
    GeneratingCharacterIds.Remove(MatchedCharacterId);

    if (Response.FailedSteps.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CharacterImageSubsystem] Partial image generation for character %s. Failed steps: %s"),
            *MatchedCharacterId, *FString::Join(Response.FailedSteps, TEXT(", ")));
    }

    if (!Response.bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("[CharacterImageSubsystem] Image generation failed for character %s: %s"),
            *MatchedCharacterId, *Response.Error);
        OnCharacterImageFailed.Broadcast(MatchedCharacterId, Response.Error);
    }

    OnImageReadyForCharacter(MatchedCharacterId, Response.Images);
}

void UCharacterImageSubsystem::OnImageReadyForCharacter(const FString& CharacterId, const FAICharacterImageSet& ImageSet)
{
    // Cache pixel texture for character display
    if (!ImageSet.PixelAnchor.Image.LocalFilePath.IsEmpty() && FPaths::FileExists(ImageSet.PixelAnchor.Image.LocalFilePath))
    {
        if (UTexture2D* PixelTexture = LoadTextureFromFile(ImageSet.PixelAnchor.Image.LocalFilePath))
        {
            CachedPortraitTextures.Add(CharacterId + TEXT("|pixel"), PixelTexture);
        }
    }

    // Cache walk frame textures
    if (!ImageSet.WalkLeft.Image.LocalFilePath.IsEmpty() && FPaths::FileExists(ImageSet.WalkLeft.Image.LocalFilePath))
    {
        if (UTexture2D* WalkTex = LoadTextureFromFile(ImageSet.WalkLeft.Image.LocalFilePath))
        {
            CachedPortraitTextures.Add(CharacterId + TEXT("|walk1"), WalkTex);
        }
    }
    if (!ImageSet.WalkRight.Image.LocalFilePath.IsEmpty() && FPaths::FileExists(ImageSet.WalkRight.Image.LocalFilePath))
    {
        if (UTexture2D* WalkTex = LoadTextureFromFile(ImageSet.WalkRight.Image.LocalFilePath))
        {
            CachedPortraitTextures.Add(CharacterId + TEXT("|walk2"), WalkTex);
        }
    }

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
            if (UTexture2D* Texture = LoadTextureFromFile(Artifact.Image.LocalFilePath))
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
    UTexture2D* PixelTexture = GetCachedPortrait(CharacterId + TEXT("|pixel"));
    if (PixelTexture)
    {
        NPC->SetSpriteFromTexture(PixelTexture, true);
    }
}

void UCharacterImageSubsystem::ApplyToPlayerFlipbooks(APlayerCharacter* PlayerCharacter, const FAICharacterImageSet& ImageSet)
{
    if (!PlayerCharacter)
    {
        return;
    }

    const FString CharacterId = PlayerCharacter->GetPlayerImageId();
    UTexture2D* PixelTexture = GetCachedPortrait(CharacterId + TEXT("|pixel"));
    if (PixelTexture)
    {
        PlayerCharacter->SetSpriteFromTexture(PixelTexture);
    }

    // Load walk frames from ImageSet or cache
    UTexture2D* WalkLeftTex = nullptr;
    UTexture2D* WalkRightTex = nullptr;

    if (!ImageSet.WalkLeft.Image.LocalFilePath.IsEmpty() && FPaths::FileExists(ImageSet.WalkLeft.Image.LocalFilePath))
    {
        WalkLeftTex = LoadTextureFromFile(ImageSet.WalkLeft.Image.LocalFilePath);
    }
    else
    {
        WalkLeftTex = GetCachedPortrait(CharacterId + TEXT("|walk1"));
    }

    if (!ImageSet.WalkRight.Image.LocalFilePath.IsEmpty() && FPaths::FileExists(ImageSet.WalkRight.Image.LocalFilePath))
    {
        WalkRightTex = LoadTextureFromFile(ImageSet.WalkRight.Image.LocalFilePath);
    }
    else
    {
        WalkRightTex = GetCachedPortrait(CharacterId + TEXT("|walk2"));
    }

    if (WalkLeftTex || WalkRightTex)
    {
        PlayerCharacter->SetWalkFrames(WalkLeftTex, WalkRightTex);
    }
}

UTexture2D* UCharacterImageSubsystem::GetExpressionPortrait(const FString& CharacterId, const FString& Expression) const
{
    if (CharacterId.IsEmpty() || Expression.IsEmpty())
    {
        return nullptr;
    }

    FString Key = CharacterId + TEXT("|") + Expression.ToLower();
    return GetCachedPortrait(Key);
}

UTexture2D* UCharacterImageSubsystem::GetCachedPortrait(const FString& Key) const
{
    if (Key.IsEmpty())
    {
        return nullptr;
    }

    if (const TObjectPtr<UTexture2D>* TexPtr = CachedPortraitTextures.Find(Key))
    {
        return *TexPtr;
    }
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
        PortraitTexture = LoadTextureFromFile(ImageSet.DetailAnchor.Image.LocalFilePath);
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

    auto TryLoadImage = [&CacheDir, &bAnyLoaded](const FString& SubDir, const FString& FilePrefix, FAIImageResult& OutImage) -> bool
    {
        FString SubDirPath = FPaths::Combine(CacheDir, SubDir);
        if (!FPaths::DirectoryExists(SubDirPath))
        {
            return false;
        }

        TArray<FString> Extensions = { TEXT("jpg"), TEXT("jpeg"), TEXT("png"), TEXT("webp") };
        for (const FString& Ext : Extensions)
        {
            FString FileName = FilePrefix + TEXT("_01.") + Ext;
            FString FullPath = FPaths::Combine(SubDirPath, FileName);
            if (FPaths::FileExists(FullPath))
            {
                OutImage.LocalFilePath = FullPath;
                bAnyLoaded = true;
                return true;
            }
        }
        return false;
    };

    // Load images from correct subdirectories with generated file names
    TryLoadImage(TEXT("detail"), CharacterId + TEXT("_detail_anchor"), OutImageSet.DetailAnchor.Image);
    TryLoadImage(TEXT("pixel"), CharacterId + TEXT("_pixel_anchor"), OutImageSet.PixelAnchor.Image);
    TryLoadImage(TEXT("detail"), CharacterId + TEXT("_happy"), OutImageSet.Happy.Image);
    TryLoadImage(TEXT("detail"), CharacterId + TEXT("_angry"), OutImageSet.Angry.Image);
    TryLoadImage(TEXT("detail"), CharacterId + TEXT("_sad"), OutImageSet.Sad.Image);
    TryLoadImage(TEXT("pixel"), CharacterId + TEXT("_walk_left"), OutImageSet.WalkLeft.Image);
    TryLoadImage(TEXT("pixel"), CharacterId + TEXT("_walk_right"), OutImageSet.WalkRight.Image);

    // Cache pixel texture for character display
    if (!OutImageSet.PixelAnchor.Image.LocalFilePath.IsEmpty())
    {
        if (UTexture2D* PixelTexture = LoadTextureFromFile(OutImageSet.PixelAnchor.Image.LocalFilePath))
        {
            CachedPortraitTextures.Add(CharacterId + TEXT("|pixel"), PixelTexture);
        }
    }

    // Cache walk frame textures
    if (!OutImageSet.WalkLeft.Image.LocalFilePath.IsEmpty())
    {
        if (UTexture2D* WalkTex = LoadTextureFromFile(OutImageSet.WalkLeft.Image.LocalFilePath))
        {
            CachedPortraitTextures.Add(CharacterId + TEXT("|walk1"), WalkTex);
        }
    }
    if (!OutImageSet.WalkRight.Image.LocalFilePath.IsEmpty())
    {
        if (UTexture2D* WalkTex = LoadTextureFromFile(OutImageSet.WalkRight.Image.LocalFilePath))
        {
            CachedPortraitTextures.Add(CharacterId + TEXT("|walk2"), WalkTex);
        }
    }

    // Cache expression textures for dialogue portrait switching
    auto TryLoadExpression = [this, &CacheDir, &CharacterId, &bAnyLoaded](const FString& SubDir, const FString& FilePrefix, const FString& ExpressionName)
    {
        FString SubDirPath = FPaths::Combine(CacheDir, SubDir);
        if (!FPaths::DirectoryExists(SubDirPath))
        {
            return;
        }

        TArray<FString> Extensions = { TEXT("jpg"), TEXT("jpeg"), TEXT("png"), TEXT("webp") };
        for (const FString& Ext : Extensions)
        {
            FString FileName = FilePrefix + TEXT("_01.") + Ext;
            FString FullPath = FPaths::Combine(SubDirPath, FileName);
            if (FPaths::FileExists(FullPath))
            {
                if (UTexture2D* Texture = LoadTextureFromFile(FullPath))
                {
                    FString Key = CharacterId + TEXT("|") + ExpressionName.ToLower();
                    CachedExpressionTextures.Add(Key, Texture);
                    bAnyLoaded = true;
                }
                break;
            }
        }
    };

    TryLoadExpression(TEXT("detail"), CharacterId + TEXT("_detail_anchor"), TEXT("neutral"));
    TryLoadExpression(TEXT("detail"), CharacterId + TEXT("_happy"), TEXT("happy"));
    TryLoadExpression(TEXT("detail"), CharacterId + TEXT("_angry"), TEXT("angry"));
    TryLoadExpression(TEXT("detail"), CharacterId + TEXT("_sad"), TEXT("sad"));

    return bAnyLoaded;
}
