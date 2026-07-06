#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AIImageTypes.h"
#include "CharacterImageSubsystem.generated.h"

class ANPCCharacter;
class APlayerCharacter;
class UDialogueWidget;
class UPaperFlipbook;
class UTexture2D;
class UAICharacterImageSetAsyncAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterImageReady, const FString&, CharacterId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCharacterImageFailed, const FString&, CharacterId, const FString&, Error);

/**
 * 角色图管理子系统
 * 负责协调 LLM 生成的 NPC 配置和 AIImageGenerator 之间的交叉操作：
 * - 在检测到新 NPC 时自动请求角色图集
 * - 缓存已生成的图集，避免重复请求
 * - 将图集应用到场景中 NPC 的 PaperFlipbook 组件和 DialogueWidget 的 Image 组件
 */
UCLASS()
class PLUGINEXAMPLE_API UCharacterImageSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 请求为指定 NPC 生成角色图集（如果已缓存则直接应用，不会重新请求 LLM/Image API） */
    UFUNCTION(BlueprintCallable, Category = "RPG|CharacterImage")
    void RequestCharacterImageForNPC(ANPCCharacter* NPC, UDialogueWidget* DialogueWidget = nullptr);

    /** 请求玩家角色图集（在世界创建时触发，不影响主游戏流程） */
    UFUNCTION(BlueprintCallable, Category = "RPG|CharacterImage")
    void RequestPlayerCharacterImage(APlayerCharacter* PlayerCharacter);

    /** 当对话开始时调用，根据 NPCId 推送已缓存的图像到当前 DialogueWidget */
    UFUNCTION(BlueprintCallable, Category = "RPG|CharacterImage")
    void ApplyCachedImageToDialogue(const FString& NPCId, UDialogueWidget* DialogueWidget);

    /** 检查指定角色 ID 是否已存在本地缓存的图集 */
    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    bool HasCachedImageSet(const FString& CharacterId) const;

    /** 获取指定角色和表情的缓存肖像图（表情如 happy/angry/sad，不区分大小写） */
    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    UTexture2D* GetExpressionPortrait(const FString& CharacterId, const FString& Expression) const;

    /** 取消指定角色 ID 的所有进行中请求 */
    UFUNCTION(BlueprintCallable, Category = "RPG|CharacterImage")
    void CancelRequestsForCharacter(const FString& CharacterId);

    /** Delegate fired when character image set is ready and applied */
    UPROPERTY(BlueprintAssignable)
    FOnCharacterImageReady OnCharacterImageReady;

    /** Delegate fired when generation fails */
    UPROPERTY(BlueprintAssignable)
    FOnCharacterImageFailed OnCharacterImageFailed;

private:
    /** Internal dispatcher for both NPC and player image requests */
    void RequestCharacterImageInternal(
        const FString& CharacterId,
        ANPCCharacter* NPC,
        APlayerCharacter* PlayerCharacter,
        UDialogueWidget* DialogueWidget);

    /** Build the AI character generation request from NPCData */
    FAICharacterGenerationRequest BuildImageRequestForNPC(ANPCCharacter* NPC) const;

    /** Build the AI character generation request for the player character */
    FAICharacterGenerationRequest BuildImageRequestForPlayer(APlayerCharacter* PlayerCharacter) const;

    /** Async callback: image generation completed. */
    UFUNCTION()
    void HandleImageSetResponse(const FAICharacterGenerationResponse& Response);

    /** Internal handler keyed by character id. */
    void OnImageReadyForCharacter(const FString& CharacterId, const FAICharacterImageSet& ImageSet);

    /** Apply image set to NPC and dialogue widget (Flipbook + UI Image) */
    void ApplyImageSetToActors(const FString& CharacterId, const FAICharacterImageSet& ImageSet);

    /** Apply generated textures to NPC's PaperFlipbook components */
    void ApplyToNPCFlipbooks(ANPCCharacter* NPC, const FAICharacterImageSet& ImageSet);

    /** Apply generated textures to Player's PaperFlipbook components */
    void ApplyToPlayerFlipbooks(APlayerCharacter* PlayerCharacter, const FAICharacterImageSet& ImageSet);

    /** Apply generated textures to DialogueWidget's UI Images */
    void ApplyToDialogueWidget(UDialogueWidget* DialogueWidget, const FAICharacterImageSet& ImageSet);

    /** Try to load flipbook textures from cached disk location */
    bool TryLoadFlipbookTextures(const FString& CharacterId, FAICharacterImageSet& OutImageSet);

    /** Tracking pending async actions per character */
    UPROPERTY()
    TMap<FString, TObjectPtr<UAICharacterImageSetAsyncAction>> PendingActions;

    /** Cached generated pixel flipbooks per character (key = CharacterId) */
    UPROPERTY()
    TMap<FString, TObjectPtr<UPaperFlipbook>> CachedPixelFlipbooks;

    /** Cached UI portrait texture per character (default/neutral expression) */
    UPROPERTY()
    TMap<FString, TObjectPtr<UTexture2D>> CachedPortraitTextures;

    /** Cached expression textures per character: key format = \"CharacterId|expression\" (expression lower-case) */
    UPROPERTY()
    TMap<FString, TObjectPtr<UTexture2D>> CachedExpressionTextures;

    /** Track which characters are currently being generated */
    UPROPERTY()
    TSet<FString> GeneratingCharacterIds;

    /** Track NPC actors by CharacterId so we can apply image to actor on completion. */
    UPROPERTY()
    TMap<FString, TObjectPtr<ANPCCharacter>> TrackedNPCs;

    /** Track Player character so we can apply image on completion. */
    UPROPERTY()
    TObjectPtr<APlayerCharacter> TrackedPlayer;
};