#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameDataTypes.h"
#include "AttributeSystemSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAttributeChanged,
    const FString&, AttributeId, int32, OldValue, int32, NewValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeCheckCompleted,
    const FAttributeCheckResult&, Result);

UCLASS()
class PLUGINEXAMPLE_API UAttributeSystemSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // ==================== Attribute Management ====================

    /** Initialize player attributes from LLM-generated attribute list */
    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    void InitializeAttributes(const TArray<FPlayerAttribute>& Attributes);

    /** Get all attributes */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    TArray<FPlayerAttribute> GetAllAttributes() const;

    /** Get attribute by ID */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    bool TryGetAttribute(const FString& AttributeId, FPlayerAttribute& OutAttribute) const;

    /** Modify attribute value (auto-clamped to Min/Max range), returns the actual delta applied */
    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    int32 ModifyAttribute(const FString& AttributeId, int32 Delta);

    /** Directly set attribute value (auto-clamped) */
    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    void SetAttributeValue(const FString& AttributeId, int32 NewValue);

    /** Calculate attribute modifier (D&D style: (value - 10) / 2, rounded down) */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    int32 GetAttributeModifier(const FString& AttributeId) const;

    // ==================== Attribute Check ====================

    /** Execute a complete attribute check (including dice roll) */
    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    FAttributeCheckResult ExecuteAttributeCheck(const FAttributeCheckRequest& Request);

    // ==================== Dice System ====================

    /** Execute dice roll */
    UFUNCTION(BlueprintCallable, Category = "RPG|Dice")
    FDiceRollResult RollDice(EDiceType DiceType, int32 Count, int32 AttributeModifier,
        EDiceAdvantage Advantage, int32 DifficultyClass);

    // ==================== Context & Serialization ====================

    /** Generate compact attribute state text for LLM context injection */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    FString BuildAttributeContextString() const;

    /** Export attributes to JSON */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    FString ExportToJson() const;

    /** Import attributes from JSON */
    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    bool ImportFromJson(const FString& JsonString);

    /** Set random seed from world ID (for save/load reproducibility) */
    UFUNCTION(BlueprintCallable, Category = "RPG|Dice")
    void SetSeedFromWorldId(const FString& WorldId);

    // ==================== Events ====================

    /** Attribute value changed event */
    UPROPERTY(BlueprintAssignable)
    FOnAttributeChanged OnAttributeChanged;

    /** Attribute check completed event */
    UPROPERTY(BlueprintAssignable)
    FOnAttributeCheckCompleted OnAttributeCheckCompleted;

    // ==================== Helpers ====================

    /** Parse dice type string (e.g. "d20") to enum */
    UFUNCTION(BlueprintPure, Category = "RPG|Dice")
    static EDiceType ParseDiceType(const FString& DiceTypeStr);

    /** Parse advantage string to enum */
    UFUNCTION(BlueprintPure, Category = "RPG|Dice")
    static EDiceAdvantage ParseAdvantage(const FString& AdvantageStr);

    /** Parse logic string to enum */
    UFUNCTION(BlueprintPure, Category = "RPG|Check")
    static ECheckLogic ParseCheckLogic(const FString& LogicStr);

    /** Parse attribute type string to enum */
    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    static EAttributeType ParseAttributeType(const FString& TypeStr);

private:
    /** Attribute storage (indexed by AttributeId) */
    UPROPERTY()
    TMap<FString, FPlayerAttribute> AttributeMap;

    /** Dice random stream (seedable) */
    FRandomStream DiceRandomStream;

    /** Get dice max value */
    int32 GetDiceMaxValue(EDiceType DiceType) const;
};