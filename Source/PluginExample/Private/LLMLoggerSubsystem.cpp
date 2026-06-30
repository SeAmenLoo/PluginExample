#include "LLMLoggerSubsystem.h"
#include "Misc/DateTime.h"

DEFINE_LOG_CATEGORY(LogPluginLLM);

void ULLMLoggerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    TotalRequests = 0;
    TotalSuccessResponses = 0;
    TotalErrorResponses = 0;
    TotalUsage = FLLMUsage();
}

void ULLMLoggerSubsystem::Deinitialize()
{
    if (bLoggingEnabled && TotalRequests > 0)
    {
        UE_LOG(LogPluginLLM, Log, TEXT("=== LLM Session Summary ==="));
        UE_LOG(LogPluginLLM, Log, TEXT("Total Requests: %d"), TotalRequests);
        UE_LOG(LogPluginLLM, Log, TEXT("Success: %d | Errors: %d"),
            TotalSuccessResponses, TotalErrorResponses);
        UE_LOG(LogPluginLLM, Log, TEXT("Total Tokens - Prompt: %d, Completion: %d, Total: %d"),
            TotalUsage.PromptTokens, TotalUsage.CompletionTokens, TotalUsage.TotalTokens);
        UE_LOG(LogPluginLLM, Log, TEXT("============================"));
    }

    Super::Deinitialize();
}

void ULLMLoggerSubsystem::LogRequest(ELLMRequestType RequestType, const FLLMChatRequest& Request, const FString& Context)
{
    if (!bLoggingEnabled)
    {
        return;
    }

    TotalRequests++;
    LastRequestTime = FDateTime::UtcNow();

    FString TypeStr = RequestTypeToString(RequestType);
    FString ContextStr = Context.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" [%s]"), *Context);

    UE_LOG(LogPluginLLM, Log, TEXT(">>> LLM REQUEST [%s]%s"), *TypeStr, *ContextStr);
    UE_LOG(LogPluginLLM, Log, TEXT("  Model: %s | Temp: %.2f | MaxTokens: %d | ForceJSON: %s"),
        Request.ModelOverride.IsEmpty() ? TEXT("(default)") : *Request.ModelOverride,
        Request.Temperature,
        Request.MaxTokens,
        Request.bForceJsonObject ? TEXT("true") : TEXT("false"));
    UE_LOG(LogPluginLLM, Log, TEXT("  Messages: %d"), Request.Messages.Num());

    if (Verbosity >= 1)
    {
        UE_LOG(LogPluginLLM, Log, TEXT("  Summary:\n%s"), *FormatMessagesSummary(Request.Messages));
    }

    if (Verbosity >= 2)
    {
        UE_LOG(LogPluginLLM, VeryVerbose, TEXT("  Full Messages:\n%s"), *FormatFullMessages(Request.Messages));
    }

    if (Verbosity >= 1 && Request.Tools.Num() > 0)
    {
        UE_LOG(LogPluginLLM, Log, TEXT("  Tools: %d"), Request.Tools.Num());
        for (const FLLMToolDefinition& Tool : Request.Tools)
        {
            UE_LOG(LogPluginLLM, Log, TEXT("    - %s: %s"), *Tool.Name, *TruncateString(Tool.Description, 80));
        }
    }
}

void ULLMLoggerSubsystem::LogResponse(ELLMRequestType RequestType, const FLLMChatResponse& Response, float DurationSeconds, const FString& Context)
{
    if (!bLoggingEnabled)
    {
        return;
    }

    FString TypeStr = RequestTypeToString(RequestType);
    FString ContextStr = Context.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" [%s]"), *Context);

    if (Response.bSuccess)
    {
        TotalSuccessResponses++;
        TotalUsage.PromptTokens += Response.Usage.PromptTokens;
        TotalUsage.CompletionTokens += Response.Usage.CompletionTokens;
        TotalUsage.TotalTokens += Response.Usage.TotalTokens;

        UE_LOG(LogPluginLLM, Log, TEXT("<<< LLM RESPONSE [%s]%s - OK (%.2fs)"), *TypeStr, *ContextStr, DurationSeconds);
        UE_LOG(LogPluginLLM, Log, TEXT("  HTTP: %d | Tokens: %d (%d+%d)"),
            Response.HttpStatus,
            Response.Usage.TotalTokens,
            Response.Usage.PromptTokens,
            Response.Usage.CompletionTokens);

        if (Verbosity >= 1)
        {
            FString DisplayContent = Response.Content.IsEmpty() ? Response.Json : Response.Content;
            UE_LOG(LogPluginLLM, Log, TEXT("  Content (%d chars):\n%s"),
                DisplayContent.Len(),
                *TruncateString(DisplayContent, 500));
        }

        if (Verbosity >= 2)
        {
            UE_LOG(LogPluginLLM, VeryVerbose, TEXT("  Full Content:\n%s"), *Response.Content);
            if (!Response.Json.IsEmpty())
            {
                UE_LOG(LogPluginLLM, VeryVerbose, TEXT("  Full JSON:\n%s"), *Response.Json);
            }
        }

        if (Response.ToolCalls.Num() > 0)
        {
            UE_LOG(LogPluginLLM, Log, TEXT("  Tool Calls: %d"), Response.ToolCalls.Num());
            for (const FLLMToolCall& Call : Response.ToolCalls)
            {
                UE_LOG(LogPluginLLM, Log, TEXT("    - %s (id: %s)"), *Call.Name, *Call.Id);
                if (Verbosity >= 2)
                {
                    UE_LOG(LogPluginLLM, VeryVerbose, TEXT("      args: %s"), *TruncateString(Call.ArgumentsJson, 200));
                }
            }
        }
    }
    else
    {
        TotalErrorResponses++;

        UE_LOG(LogPluginLLM, Error, TEXT("<<< LLM RESPONSE [%s]%s - FAILED (%.2fs)"), *TypeStr, *ContextStr, DurationSeconds);
        UE_LOG(LogPluginLLM, Error, TEXT("  HTTP: %d | Error: %s"), Response.HttpStatus, *Response.Error);
    }
}

void ULLMLoggerSubsystem::LogError(ELLMRequestType RequestType, const FString& ErrorMessage, const FString& Context)
{
    if (!bLoggingEnabled)
    {
        return;
    }

    FString TypeStr = RequestTypeToString(RequestType);
    FString ContextStr = Context.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" [%s]"), *Context);

    UE_LOG(LogPluginLLM, Error, TEXT("!!! LLM ERROR [%s]%s: %s"), *TypeStr, *ContextStr, *ErrorMessage);
}

FString ULLMLoggerSubsystem::RequestTypeToString(ELLMRequestType Type) const
{
    switch (Type)
    {
    case ELLMRequestType::WorldGeneration: return TEXT("WorldGen");
    case ELLMRequestType::ChunkGeneration: return TEXT("ChunkGen");
    case ELLMRequestType::NPCDialogue:     return TEXT("Dialogue");
    case ELLMRequestType::Other:           return TEXT("Other");
    default:                                return TEXT("Unknown");
    }
}

FString ULLMLoggerSubsystem::FormatMessagesSummary(const TArray<FLLMChatMessage>& Messages) const
{
    FString Result;
    for (int32 i = 0; i < Messages.Num(); ++i)
    {
        const FLLMChatMessage& Msg = Messages[i];
        FString ContentPreview = TruncateString(Msg.Content, 100);
        ContentPreview.ReplaceInline(TEXT("\n"), TEXT("\\n"));
        Result += FString::Printf(TEXT("    [%d] %s: %s (%d chars)\n"),
            i, *Msg.Role, *ContentPreview, Msg.Content.Len());
    }
    return Result;
}

FString ULLMLoggerSubsystem::FormatFullMessages(const TArray<FLLMChatMessage>& Messages) const
{
    FString Result;
    for (int32 i = 0; i < Messages.Num(); ++i)
    {
        const FLLMChatMessage& Msg = Messages[i];
        Result += FString::Printf(TEXT("--- Message %d (%s) ---\n%s\n\n"), i, *Msg.Role, *Msg.Content);
    }
    return Result;
}

FString ULLMLoggerSubsystem::TruncateString(const FString& Str, int32 MaxLen) const
{
    if (Str.Len() <= MaxLen)
    {
        return Str;
    }
    return Str.Left(MaxLen) + TEXT("...");
}
