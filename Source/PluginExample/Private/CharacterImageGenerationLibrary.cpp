#include "CharacterImageGenerationLibrary.h"
#include "RPGTypes.h"
#include "AIImageTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FAICharacterSpec UCharacterImageGenerationLibrary::ConvertToCharacterSpec(const FRPGNPCProfileConfig& NPCProfile, const FString& Worldview)
{
    FAICharacterSpec Spec;

    Spec.Name = NPCProfile.NPCName;
    Spec.Worldview = Worldview;
    Spec.Gender = NPCProfile.Gender;
    Spec.Age = NPCProfile.Age;
    Spec.Race = NPCProfile.Race;
    Spec.Job = UEnum::GetValueAsString(NPCProfile.NPCRole);
    Spec.Personality = NPCProfile.NPCDescription;
    Spec.BodyType = NPCProfile.BodyType;
    Spec.Hair = NPCProfile.Hair;
    Spec.Face = NPCProfile.Face;
    Spec.Outfit = NPCProfile.Outfit;
    Spec.Palette = NPCProfile.Palette;
    Spec.SignatureItems = NPCProfile.SignatureItems;

    return Spec;
}

FAICharacterGenerationRequest UCharacterImageGenerationLibrary::BuildCharacterImageGenerationRequest(
    const FRPGNPCProfileConfig& NPCProfile,
    const FString& Worldview,
    const FString& NegativePrompt,
    const FString& SaveDirectory
)
{
    FAICharacterGenerationRequest Request;

    Request.Character = ConvertToCharacterSpec(NPCProfile, Worldview);
    Request.NegativePrompt = NegativePrompt;

    if (!SaveDirectory.IsEmpty())
    {
        Request.SaveDirectory = SaveDirectory;
    }
    else
    {
        Request.SaveDirectory = AIImagePaths::GetCharacterCacheDirectory(NPCProfile.NPCName);
    }

    Request.FilePrefix = NPCProfile.NPCName;

    return Request;
}

bool UCharacterImageGenerationLibrary::HasCachedCharacterImages(const FString& CharacterName)
{
    FString CacheDir = AIImagePaths::GetCharacterCacheDirectory(CharacterName);
    return FPaths::DirectoryExists(CacheDir);
}

FString UCharacterImageGenerationLibrary::GetCharacterImageCacheDirectory(const FString& CharacterName)
{
    return AIImagePaths::GetCharacterCacheDirectory(CharacterName);
}

FString UCharacterImageGenerationLibrary::BuildCharacterImagePrompt(const FAICharacterSpec& Character)
{
    FString Prompt;

    Prompt += Character.DetailStyle;
    Prompt += TEXT(", RPG角色设计");

    if (!Character.Worldview.IsEmpty())
    {
        Prompt += TEXT(", ") + Character.Worldview;
    }

    if (!Character.Race.IsEmpty())
    {
        Prompt += TEXT(", ") + Character.Race;
    }

    if (!Character.Gender.IsEmpty())
    {
        Prompt += TEXT(", ") + Character.Gender;
    }

    if (!Character.Age.IsEmpty())
    {
        Prompt += TEXT(", ") + Character.Age;
    }

    if (!Character.Job.IsEmpty())
    {
        Prompt += TEXT(", ") + Character.Job;
    }

    if (!Character.BodyType.IsEmpty())
    {
        Prompt += TEXT(", ") + Character.BodyType;
    }

    if (!Character.Hair.IsEmpty())
    {
        Prompt += TEXT(", ") + Character.Hair;
    }

    if (!Character.Face.IsEmpty())
    {
        Prompt += TEXT(", ") + Character.Face;
    }

    if (!Character.Outfit.IsEmpty())
    {
        Prompt += TEXT(", ") + Character.Outfit;
    }

    if (!Character.Palette.IsEmpty())
    {
        Prompt += TEXT(", 配色:") + Character.Palette;
    }

    if (!Character.SignatureItems.IsEmpty())
    {
        Prompt += TEXT(", 持有:") + Character.SignatureItems;
    }

    if (!Character.Personality.IsEmpty())
    {
        Prompt += TEXT(", ") + Character.Personality;
    }

    return Prompt;
}

bool UCharacterImageGenerationLibrary::SaveNPCProfileCache(const FRPGNPCProfileConfig& NPCProfile)
{
    if (NPCProfile.NPCId.IsEmpty())
    {
        return false;
    }

    FString CacheDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NPCProfiles"));
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*CacheDir))
    {
        PlatformFile.CreateDirectoryTree(*CacheDir);
    }

    FString CacheFilePath = FPaths::Combine(CacheDir, NPCProfile.NPCId + TEXT(".json"));

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetStringField(TEXT("npc_id"), NPCProfile.NPCId);
    JsonObject->SetStringField(TEXT("npc_name"), NPCProfile.NPCName);
    JsonObject->SetStringField(TEXT("npc_role"), UEnum::GetValueAsString(NPCProfile.NPCRole));
    JsonObject->SetStringField(TEXT("npc_description"), NPCProfile.NPCDescription);

    if (!NPCProfile.Gender.IsEmpty())
    {
        JsonObject->SetStringField(TEXT("gender"), NPCProfile.Gender);
    }
    if (!NPCProfile.Age.IsEmpty())
    {
        JsonObject->SetStringField(TEXT("age"), NPCProfile.Age);
    }
    if (!NPCProfile.Race.IsEmpty())
    {
        JsonObject->SetStringField(TEXT("race"), NPCProfile.Race);
    }
    if (!NPCProfile.BodyType.IsEmpty())
    {
        JsonObject->SetStringField(TEXT("body_type"), NPCProfile.BodyType);
    }
    if (!NPCProfile.Hair.IsEmpty())
    {
        JsonObject->SetStringField(TEXT("hair"), NPCProfile.Hair);
    }
    if (!NPCProfile.Face.IsEmpty())
    {
        JsonObject->SetStringField(TEXT("face"), NPCProfile.Face);
    }
    if (!NPCProfile.Outfit.IsEmpty())
    {
        JsonObject->SetStringField(TEXT("outfit"), NPCProfile.Outfit);
    }
    if (!NPCProfile.Palette.IsEmpty())
    {
        JsonObject->SetStringField(TEXT("palette"), NPCProfile.Palette);
    }
    if (!NPCProfile.SignatureItems.IsEmpty())
    {
        JsonObject->SetStringField(TEXT("signature_items"), NPCProfile.SignatureItems);
    }

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        return FFileHelper::SaveStringToFile(JsonString, *CacheFilePath);
    }

    return false;
}

bool UCharacterImageGenerationLibrary::LoadNPCProfileCache(const FString& NPCId, FRPGNPCProfileConfig& OutNPCProfile)
{
    if (NPCId.IsEmpty())
    {
        return false;
    }

    FString CacheDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NPCProfiles"));
    FString CacheFilePath = FPaths::Combine(CacheDir, NPCId + TEXT(".json"));

    if (!FPaths::FileExists(CacheFilePath))
    {
        return false;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *CacheFilePath))
    {
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        OutNPCProfile.NPCId = JsonObject->GetStringField(TEXT("npc_id"));
        OutNPCProfile.NPCName = JsonObject->GetStringField(TEXT("npc_name"));
        OutNPCProfile.NPCDescription = JsonObject->GetStringField(TEXT("npc_description"));

        FString RoleStr = JsonObject->GetStringField(TEXT("npc_role"));
        if (!RoleStr.IsEmpty())
        {
            const UEnum* EnumPtr = StaticEnum<ERPGNPCRole>();
            if (EnumPtr)
            {
                int64 EnumValue = EnumPtr->GetValueByNameString(RoleStr);
                if (EnumValue != INDEX_NONE)
                {
                    OutNPCProfile.NPCRole = (ERPGNPCRole)EnumValue;
                }
            }
        }

        OutNPCProfile.Gender = JsonObject->GetStringField(TEXT("gender"));
        OutNPCProfile.Age = JsonObject->GetStringField(TEXT("age"));
        OutNPCProfile.Race = JsonObject->GetStringField(TEXT("race"));
        OutNPCProfile.BodyType = JsonObject->GetStringField(TEXT("body_type"));
        OutNPCProfile.Hair = JsonObject->GetStringField(TEXT("hair"));
        OutNPCProfile.Face = JsonObject->GetStringField(TEXT("face"));
        OutNPCProfile.Outfit = JsonObject->GetStringField(TEXT("outfit"));
        OutNPCProfile.Palette = JsonObject->GetStringField(TEXT("palette"));
        OutNPCProfile.SignatureItems = JsonObject->GetStringField(TEXT("signature_items"));

        return true;
    }

    return false;
}

bool UCharacterImageGenerationLibrary::HasCachedNPCProfile(const FString& NPCId)
{
    if (NPCId.IsEmpty())
    {
        return false;
    }

    FString CacheDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NPCProfiles"));
    FString CacheFilePath = FPaths::Combine(CacheDir, NPCId + TEXT(".json"));

    return FPaths::FileExists(CacheFilePath);
}