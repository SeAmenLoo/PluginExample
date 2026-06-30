#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LLMRuntime/Public/LLMTypes.h"
#include "LLMLoggerSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPluginLLM, Log, All);

UENUM(BlueprintType)
enum class ELLMRequestType : uint8
{
    WorldGeneration,
    ChunkGeneration,
    NPCDialogue,
    Other
};

UCLASS()
class PLUGINEXAMPLE_API ULLMLoggerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Log a chat completion request (before sending) */
    UFUNCTION(BlueprintCallable, Category = "LLM|Logger")
    void LogRequest(ELLMRequestType RequestType, const FLLMChatRequest& Request, const FString& Context = TEXT(""));

    /** Log a chat completion response (after receiving) */
    UFUNCTION(BlueprintCallable, Category = "LLM|Logger")
    void LogResponse(ELLMRequestType RequestType, const FLLMChatResponse& Response, float DurationSeconds = 0.0f, const FString& Context = TEXT(""));

    /** Log a chat completion error */
    UFUNCTION(BlueprintCallable, Category = "LLM|Logger")
    void LogError(ELLMRequestType RequestType, const FString& ErrorMessage, const FString& Context = TEXT(""));

    /** Toggle logging on/off */
    UFUNCTION(BlueprintCallable, Category = "LLM|Logger")
    void SetLoggingEnabled(bool bEnabled) { bLoggingEnabled = bEnabled; }

    /** Check if logging is enabled */
    UFUNCTION(BlueprintPure, Category = "LLM|Logger")
    bool IsLoggingEnabled() const { return bLoggingEnabled; }

    /** Set verbosity level: 0=minimal, 1=normal, 2=full message content */
    UFUNCTION(BlueprintCallable, Category = "LLM|Logger")
    void SetVerbosity(int32 Level) { Verbosity = FMath::Clamp(Level, 0, 3); }

    UFUNCTION(BlueprintPure, Category = "LLM|Logger")
    int32 GetVerbosity() const { return Verbosity; }

    /** Get total request count since startup */
    UFUNCTION(BlueprintPure, Category = "LLM|Logger")
    int32 GetTotalRequests() const { return TotalRequests; }

    /** Get total token usage since startup */
    UFUNCTION(BlueprintPure, Category = "LLM|Logger")
    FLLMUsage GetTotalUsage() const { return TotalUsage; }

private:
    FString RequestTypeToString(ELLMRequestType Type) const;
    FString FormatMessagesSummary(const TArray<FLLMChatMessage>& Messages) const;
    FString FormatFullMessages(const TArray<FLLMChatMessage>& Messages) const;
    FString TruncateString(const FString& Str, int32 MaxLen) const;

    UPROPERTY()
    bool bLoggingEnabled = true;

    UPROPERTY()
    int32 Verbosity = 1;

    UPROPERTY()
    int32 TotalRequests = 0;

    UPROPERTY()
    int32 TotalSuccessResponses = 0;

    UPROPERTY()
    int32 TotalErrorResponses = 0;

    UPROPERTY()
    FLLMUsage TotalUsage;

    FDateTime LastRequestTime;
};
