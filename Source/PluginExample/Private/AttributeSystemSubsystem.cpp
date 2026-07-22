#include "AttributeSystemSubsystem.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/DateTime.h"

void UAttributeSystemSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    DiceRandomStream.Initialize(FDateTime::Now().GetTicks());
}

// ==================== Attribute Management ====================

void UAttributeSystemSubsystem::InitializeAttributes(const TArray<FPlayerAttribute>& Attributes)
{
    AttributeMap.Empty();
    for (const FPlayerAttribute& Attr : Attributes)
    {
        // Clamp initial value to min/max range
        FPlayerAttribute Clamped = Attr;
        Clamped.CurrentValue = FMath::Clamp(Clamped.CurrentValue, Clamped.MinValue, Clamped.MaxValue);
        AttributeMap.Add(Attr.AttributeId, Clamped);
    }
}

TArray<FPlayerAttribute> UAttributeSystemSubsystem::GetAllAttributes() const
{
    TArray<FPlayerAttribute> Result;
    AttributeMap.GenerateValueArray(Result);
    return Result;
}

bool UAttributeSystemSubsystem::TryGetAttribute(const FString& AttributeId, FPlayerAttribute& OutAttribute) const
{
    if (const FPlayerAttribute* Found = AttributeMap.Find(AttributeId))
    {
        OutAttribute = *Found;
        return true;
    }
    return false;
}

int32 UAttributeSystemSubsystem::ModifyAttribute(const FString& AttributeId, int32 Delta)
{
    FPlayerAttribute* Attr = AttributeMap.Find(AttributeId);
    if (!Attr)
    {
        return 0;
    }

    int32 OldValue = Attr->CurrentValue;
    int32 NewValue = FMath::Clamp(OldValue + Delta, Attr->MinValue, Attr->MaxValue);
    Attr->CurrentValue = NewValue;

    int32 ActualDelta = NewValue - OldValue;

    if (ActualDelta != 0)
    {
        OnAttributeChanged.Broadcast(AttributeId, OldValue, NewValue);
    }

    return ActualDelta;
}

void UAttributeSystemSubsystem::SetAttributeValue(const FString& AttributeId, int32 NewValue)
{
    FPlayerAttribute* Attr = AttributeMap.Find(AttributeId);
    if (!Attr)
    {
        return;
    }

    int32 OldValue = Attr->CurrentValue;
    int32 ClampedValue = FMath::Clamp(NewValue, Attr->MinValue, Attr->MaxValue);
    Attr->CurrentValue = ClampedValue;

    if (ClampedValue != OldValue)
    {
        OnAttributeChanged.Broadcast(AttributeId, OldValue, ClampedValue);
    }
}

int32 UAttributeSystemSubsystem::GetAttributeModifier(const FString& AttributeId) const
{
    if (const FPlayerAttribute* Attr = AttributeMap.Find(AttributeId))
    {
        // D&D 5e style: (value - 10) / 2, rounded down
        return FMath::FloorToInt((Attr->CurrentValue - 10) / 2.0f);
    }
    return 0;
}

// ==================== Dice System ====================

int32 UAttributeSystemSubsystem::GetDiceMaxValue(EDiceType DiceType) const
{
    switch (DiceType)
    {
        case EDiceType::D4:   return 4;
        case EDiceType::D6:   return 6;
        case EDiceType::D8:   return 8;
        case EDiceType::D10:  return 10;
        case EDiceType::D12:  return 12;
        case EDiceType::D20:  return 20;
        case EDiceType::D100: return 100;
        default:              return 20;
    }
}

FDiceRollResult UAttributeSystemSubsystem::RollDice(
    EDiceType DiceType, int32 Count, int32 AttributeModifier,
    EDiceAdvantage Advantage, int32 DifficultyClass)
{
    FDiceRollResult Result;
    Result.DiceType = DiceType;
    Result.DiceCount = Count;
    Result.AttributeModifier = AttributeModifier;
    Result.DifficultyClass = DifficultyClass;

    const int32 MaxValue = GetDiceMaxValue(DiceType);

    // Advantage/Disadvantage: roll twice as many dice
    const int32 RollTimes = (Advantage != EDiceAdvantage::Normal) ? Count * 2 : Count;

    TArray<int32> AllRolls;
    for (int32 i = 0; i < RollTimes; ++i)
    {
        AllRolls.Add(DiceRandomStream.RandRange(1, MaxValue));
    }

    if (Advantage == EDiceAdvantage::Normal)
    {
        Result.RawRolls = AllRolls;
        for (int32 Roll : AllRolls)
        {
            Result.DiceTotal += Roll;
        }
    }
    else
    {
        // Advantage/Disadvantage: keep higher/lower for each pair
        for (int32 i = 0; i < Count; ++i)
        {
            const int32 Roll1 = AllRolls[i * 2];
            const int32 Roll2 = AllRolls[i * 2 + 1];

            int32 Kept, Discarded;
            if (Advantage == EDiceAdvantage::Advantage)
            {
                Kept = FMath::Max(Roll1, Roll2);
                Discarded = FMath::Min(Roll1, Roll2);
            }
            else
            {
                Kept = FMath::Min(Roll1, Roll2);
                Discarded = FMath::Max(Roll1, Roll2);
            }
            Result.RawRolls.Add(Kept);
            Result.DiscardedRolls.Add(Discarded);
            Result.DiceTotal += Kept;
        }
    }

    Result.FinalValue = Result.DiceTotal + AttributeModifier;

    // Critical check (D20 only)
    if (DiceType == EDiceType::D20 && Count == 1 && Result.RawRolls.Num() > 0)
    {
        const int32 NaturalRoll = Result.RawRolls[0];
        if (NaturalRoll == MaxValue)
        {
            Result.bCriticalSuccess = true;
            Result.bSuccess = true;
            return Result;
        }
        if (NaturalRoll == 1)
        {
            Result.bCriticalFailure = true;
            Result.bSuccess = false;
            return Result;
        }
    }

    Result.bSuccess = (Result.FinalValue >= DifficultyClass);
    return Result;
}

// ==================== Attribute Check ====================

FAttributeCheckResult UAttributeSystemSubsystem::ExecuteAttributeCheck(
    const FAttributeCheckRequest& Request)
{
    FAttributeCheckResult Result;
    Result.Request = Request;

    bool bOverallSuccess = false;

    if (Request.AttributeIds.Num() == 0)
    {
        // No attributes involved, pure dice roll
        FDiceRollResult DiceResult = RollDice(Request.DiceType, Request.DiceCount,
            0, Request.Advantage, Request.DifficultyClass);
        bOverallSuccess = DiceResult.bSuccess;
        Result.DiceResult = DiceResult;
    }
    else if (Request.Logic == ECheckLogic::Intersection)
    {
        // AND logic: all attributes must succeed
        bOverallSuccess = true;
        for (const FString& AttrId : Request.AttributeIds)
        {
            int32 Modifier = GetAttributeModifier(AttrId);
            FDiceRollResult DiceResult = RollDice(Request.DiceType, Request.DiceCount,
                Modifier, Request.Advantage, Request.DifficultyClass);

            // Record the first dice result for display
            if (Result.DiceResult.FinalValue == 0)
            {
                Result.DiceResult = DiceResult;
            }

            if (!DiceResult.bSuccess)
            {
                bOverallSuccess = false;
                break;  // Short-circuit
            }
        }
    }
    else
    {
        // OR logic: any attribute succeeds
        bOverallSuccess = false;
        for (const FString& AttrId : Request.AttributeIds)
        {
            int32 Modifier = GetAttributeModifier(AttrId);
            FDiceRollResult DiceResult = RollDice(Request.DiceType, Request.DiceCount,
                Modifier, Request.Advantage, Request.DifficultyClass);

            if (Result.DiceResult.FinalValue == 0)
            {
                Result.DiceResult = DiceResult;
            }

            if (DiceResult.bSuccess)
            {
                bOverallSuccess = true;
                Result.DiceResult = DiceResult;  // Record the successful dice result
                break;  // Short-circuit
            }
        }
    }

    // Fill result based on success/failure
    if (bOverallSuccess)
    {
        Result.DisplayedText = Request.SuccessText;
        Result.ExecutedActions = Request.SuccessActions;
        Result.ExecutedModifications = Request.SuccessModifications;
    }
    else
    {
        Result.DisplayedText = Request.FailureText;
        Result.ExecutedActions = Request.FailureActions;
        Result.ExecutedModifications = Request.FailureModifications;
    }

    Result.DiceResult.bSuccess = bOverallSuccess;

    // Execute attribute modifications
    for (const FAttributeModification& Mod : Result.ExecutedModifications)
    {
        ModifyAttribute(Mod.AttributeId, Mod.Delta);
    }

    // Broadcast event
    OnAttributeCheckCompleted.Broadcast(Result);

    return Result;
}

// ==================== Context & Serialization ====================

FString UAttributeSystemSubsystem::BuildAttributeContextString() const
{
    FString ContextStr;
    for (const auto& Pair : AttributeMap)
    {
        const FPlayerAttribute& Attr = Pair.Value;
        int32 Modifier = GetAttributeModifier(Attr.AttributeId);
        FString ModStr = (Modifier >= 0) ?
            FString::Printf(TEXT("+%d"), Modifier) :
            FString::Printf(TEXT("%d"), Modifier);

        ContextStr += FString::Printf(TEXT("%s(%s): %d [%s] | "),
            *Attr.DisplayName, *Attr.AttributeId, Attr.CurrentValue, *ModStr);
    }

    // Remove trailing " | "
    if (ContextStr.Len() > 3)
    {
        ContextStr.LeftChopInline(3);
    }

    return ContextStr;
}

FString UAttributeSystemSubsystem::ExportToJson() const
{
    TSharedRef<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> AttrArray;

    for (const auto& Pair : AttributeMap)
    {
        const FPlayerAttribute& Attr = Pair.Value;
        TSharedRef<FJsonObject> AttrObj = MakeShareable(new FJsonObject());
        AttrObj->SetStringField(TEXT("id"), Attr.AttributeId);
        AttrObj->SetStringField(TEXT("display_name"), Attr.DisplayName);
        AttrObj->SetNumberField(TEXT("value"), Attr.CurrentValue);
        AttrObj->SetNumberField(TEXT("min"), Attr.MinValue);
        AttrObj->SetNumberField(TEXT("max"), Attr.MaxValue);
        AttrObj->SetStringField(TEXT("description"), Attr.Description);

        // Attribute type to string
        FString TypeStr;
        switch (Attr.AttributeType)
        {
            case EAttributeType::Physical: TypeStr = TEXT("physical"); break;
            case EAttributeType::Mental:   TypeStr = TEXT("mental"); break;
            case EAttributeType::Social:   TypeStr = TEXT("social"); break;
            default:                       TypeStr = TEXT("special"); break;
        }
        AttrObj->SetStringField(TEXT("type"), TypeStr);

        AttrArray.Add(MakeShareable(new FJsonValueObject(AttrObj)));
    }

    RootObject->SetArrayField(TEXT("player_attributes"), AttrArray);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject, Writer);
    return OutputString;
}

bool UAttributeSystemSubsystem::ImportFromJson(const FString& JsonString)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    TSharedPtr<FJsonObject> JsonObject;
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* AttrArray;
    if (!JsonObject->TryGetArrayField(TEXT("player_attributes"), AttrArray))
    {
        return false;
    }

    TArray<FPlayerAttribute> ParsedAttributes;
    for (const auto& AttrVal : *AttrArray)
    {
        const TSharedPtr<FJsonObject>& AttrObj = AttrVal->AsObject();
        if (!AttrObj.IsValid()) continue;

        FPlayerAttribute Attr;
        Attr.AttributeId = AttrObj->GetStringField(TEXT("id"));
        Attr.DisplayName = AttrObj->GetStringField(TEXT("display_name"));
        Attr.CurrentValue = AttrObj->GetNumberField(TEXT("value"));
        Attr.MinValue = AttrObj->HasField(TEXT("min")) ?
            AttrObj->GetNumberField(TEXT("min")) : 1;
        Attr.MaxValue = AttrObj->HasField(TEXT("max")) ?
            AttrObj->GetNumberField(TEXT("max")) : 20;
        Attr.Description = AttrObj->GetStringField(TEXT("description"));

        Attr.AttributeType = ParseAttributeType(
            AttrObj->GetStringField(TEXT("type")));

        ParsedAttributes.Add(Attr);
    }

    InitializeAttributes(ParsedAttributes);
    return true;
}

void UAttributeSystemSubsystem::SetSeedFromWorldId(const FString& WorldId)
{
    uint32 Seed = FCrc::StrCrc32(*WorldId);
    DiceRandomStream.Initialize(Seed);
}

// ==================== Helpers ====================

EDiceType UAttributeSystemSubsystem::ParseDiceType(const FString& DiceTypeStr)
{
    if (DiceTypeStr == TEXT("d4"))   return EDiceType::D4;
    if (DiceTypeStr == TEXT("d6"))   return EDiceType::D6;
    if (DiceTypeStr == TEXT("d8"))   return EDiceType::D8;
    if (DiceTypeStr == TEXT("d10"))  return EDiceType::D10;
    if (DiceTypeStr == TEXT("d12"))  return EDiceType::D12;
    if (DiceTypeStr == TEXT("d100")) return EDiceType::D100;
    return EDiceType::D20;  // Default
}

EDiceAdvantage UAttributeSystemSubsystem::ParseAdvantage(const FString& AdvantageStr)
{
    if (AdvantageStr == TEXT("advantage"))    return EDiceAdvantage::Advantage;
    if (AdvantageStr == TEXT("disadvantage")) return EDiceAdvantage::Disadvantage;
    return EDiceAdvantage::Normal;
}

ECheckLogic UAttributeSystemSubsystem::ParseCheckLogic(const FString& LogicStr)
{
    if (LogicStr == TEXT("union")) return ECheckLogic::Union;
    return ECheckLogic::Intersection;
}

EAttributeType UAttributeSystemSubsystem::ParseAttributeType(const FString& TypeStr)
{
    if (TypeStr == TEXT("physical")) return EAttributeType::Physical;
    if (TypeStr == TEXT("mental"))   return EAttributeType::Mental;
    if (TypeStr == TEXT("social"))   return EAttributeType::Social;
    return EAttributeType::Special;
}