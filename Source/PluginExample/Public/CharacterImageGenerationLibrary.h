#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RPGTypes.h"
#include "AIImageTypes.h"
#include "CharacterImageGenerationLibrary.generated.h"

UCLASS()
class PLUGINEXAMPLE_API UCharacterImageGenerationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    static FAICharacterSpec ConvertToCharacterSpec(const FRPGNPCProfileConfig& NPCProfile, const FString& Worldview = TEXT(""));

    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    static FAICharacterGenerationRequest BuildCharacterImageGenerationRequest(
        const FRPGNPCProfileConfig& NPCProfile,
        const FString& Worldview = TEXT(""),
        const FString& NegativePrompt = TEXT(""),
        const FString& SaveDirectory = TEXT("")
    );

    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    static bool HasCachedCharacterImages(const FString& CharacterName);

    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    static FString GetCharacterImageCacheDirectory(const FString& CharacterName);

    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    static FString BuildCharacterImagePrompt(const FAICharacterSpec& Character);

    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    static bool SaveNPCProfileCache(const FRPGNPCProfileConfig& NPCProfile);

    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    static bool LoadNPCProfileCache(const FString& NPCId, FRPGNPCProfileConfig& OutNPCProfile);

    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    static bool HasCachedNPCProfile(const FString& NPCId);
};