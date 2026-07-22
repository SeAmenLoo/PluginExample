#pragma once

#include "CoreMinimal.h"
#include "GameDataTypes.h"
#include "AttributeTypes.generated.h"

/**
 * Data for a single attribute row displayed in the panel.
 * Bundles the raw attribute with pre-computed display values.
 */
USTRUCT(BlueprintType)
struct FAttributeRowData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "RPG|Attribute")
    FString AttributeId;

    UPROPERTY(BlueprintReadOnly, Category = "RPG|Attribute")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "RPG|Attribute")
    EAttributeType AttributeType = EAttributeType::Physical;

    UPROPERTY(BlueprintReadOnly, Category = "RPG|Attribute")
    int32 CurrentValue = 10;

    UPROPERTY(BlueprintReadOnly, Category = "RPG|Attribute")
    int32 MinValue = 1;

    UPROPERTY(BlueprintReadOnly, Category = "RPG|Attribute")
    int32 MaxValue = 20;

    UPROPERTY(BlueprintReadOnly, Category = "RPG|Attribute")
    int32 Modifier = 0;

    UPROPERTY(BlueprintReadOnly, Category = "RPG|Attribute")
    float FillRatio = 0.5f;

    UPROPERTY(BlueprintReadOnly, Category = "RPG|Attribute")
    bool bRecentlyChanged = false;
};
