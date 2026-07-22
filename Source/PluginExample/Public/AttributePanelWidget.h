#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AttributeTypes.h"
#include "AttributePanelWidget.generated.h"

class UAttributeSystemSubsystem;
class UAttributeRowWidget;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAttributePanelClosed);

/**
 * Attribute panel widget - displays all LLM-designed attributes in a flat list.
 * Each row is a separate widget class (AttributeRowWidget) so the visual layout
 * can be customized in Blueprint while data binding stays in C++.
 */
UCLASS()
class PLUGINEXAMPLE_API UAttributePanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ==================== Public API ====================

    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    void OpenPanel();

    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    void ClosePanel();

    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    void TogglePanel();

    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    bool IsPanelOpen() const { return bIsPanelOpen; }

    UFUNCTION(BlueprintCallable, Category = "RPG|Attribute")
    void RefreshData();

    // ==================== Data Access ====================

    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    TArray<FAttributeRowData> GetAllAttributeRows() const { return AttributeRows; }

    UFUNCTION(BlueprintPure, Category = "RPG|Attribute")
    static FString FormatModifier(int32 Modifier);

    // ==================== Events ====================

    UPROPERTY(BlueprintAssignable, Category = "RPG|Attribute")
    FOnAttributePanelClosed OnPanelClosed;

    // ==================== Config ====================

    /** Row widget class to instantiate for each attribute */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute")
    TSubclassOf<UAttributeRowWidget> RowWidgetClass;

    /** Spacing between rows */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Attribute|Layout")
    float RowSpacing = 4.0f;

    // ==================== BindWidget ====================

    /** Root container for all attribute rows (place in Blueprint) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UVerticalBox> RootContainer;

protected:
    UFUNCTION()
    void HandleAttributeChanged(const FString& AttributeId, int32 OldValue, int32 NewValue);

private:
    FAttributeRowData BuildRowData(const FPlayerAttribute& Attr) const;
    void RebuildRows();

    UPROPERTY()
    TArray<FAttributeRowData> AttributeRows;

    UPROPERTY()
    TArray<TObjectPtr<UAttributeRowWidget>> RowWidgets;

    UPROPERTY()
    bool bIsPanelOpen = false;

    UPROPERTY()
    TSet<FString> RecentlyChangedAttributeIds;
};
