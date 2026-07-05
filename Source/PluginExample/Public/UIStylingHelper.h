#pragma once

#include "CoreMinimal.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Components/BackgroundBlur.h"
#include "Styling/SlateColor.h"
#include "UIStylingHelper.generated.h"

/**
 * UI Styling Configuration
 */
USTRUCT(BlueprintType)
struct FUIStyleConfig
{
    GENERATED_BODY()

    // Background color with transparency
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BackgroundColor = FLinearColor(0.1f, 0.1f, 0.15f, 0.85f);

    // Border color
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BorderColor = FLinearColor(0.3f, 0.4f, 0.6f, 1.0f);

    // Border thickness
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BorderThickness = 2.0f;

    // Corner radius for rounded borders
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CornerRadius = 8.0f;

    // Padding inside containers
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector2D Padding = FVector2D(10.0f, 10.0f);

    // Button normal color
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ButtonNormalColor = FLinearColor(0.2f, 0.25f, 0.35f, 1.0f);

    // Button hovered color
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ButtonHoveredColor = FLinearColor(0.3f, 0.4f, 0.55f, 1.0f);

    // Button pressed color
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ButtonPressedColor = FLinearColor(0.15f, 0.2f, 0.3f, 1.0f);

    // Text color for normal text
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor TextColor = FLinearColor(0.9f, 0.9f, 0.95f, 1.0f);

    // Text color for titles
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor TitleColor = FLinearColor(1.0f, 0.85f, 0.5f, 1.0f);

    // Font size for normal text
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NormalFontSize = 14;

    // Font size for titles
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TitleFontSize = 18;

    // Font size for large titles
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LargeTitleFontSize = 24;

    // Background blur strength (0 = no blur)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BlurStrength = 0.0f;
};

/**
 * Helper class for styling UMG widgets in C++
 * Provides functionality for adding backgrounds, styling buttons, and creating dynamic lists
 */
UCLASS()
class PLUGINEXAMPLE_API UUIStylingHelper : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Create a styled background image for a widget
     * @param ParentWidget - The widget to add the background to
     * @param StyleConfig - Style configuration
     * @param BackgroundTexture - Optional texture for the background (nullptr for solid color)
     * @return Created background image widget
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling")
    static UImage* CreateStyledBackground(UWidget* ParentWidget, const FUIStyleConfig& StyleConfig, UTexture2D* BackgroundTexture = nullptr);

    /**
     * Create a styled border around content
     * @param ParentWidget - The widget to add the border to
     * @param StyleConfig - Style configuration
     * @return Created border widget
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling")
    static UBorder* CreateStyledBorder(UWidget* ParentWidget, const FUIStyleConfig& StyleConfig);

    /**
     * Style a button with hover effects
     * @param Button - The button to style
     * @param StyleConfig - Style configuration
     * @param ButtonText - Optional text to set on the button
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling")
    static void StyleButton(UButton* Button, const FUIStyleConfig& StyleConfig, const FText& ButtonText = FText::GetEmpty());

    /**
     * Style a text block
     * @param TextBlock - The text block to style
     * @param StyleConfig - Style configuration
     * @param bIsTitle - Whether this is a title text (uses larger font and different color)
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling")
    static void StyleTextBlock(UTextBlock* TextBlock, const FUIStyleConfig& StyleConfig, bool bIsTitle = false);

    /**
     * Create a styled list item widget
     * @param ParentContainer - The container to add the item to
     * @param ItemText - Text content for the item
     * @param StyleConfig - Style configuration
     * @param ItemIndex - Index of the item (for alternating colors)
     * @return Created list item widget
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling")
    static UBorder* CreateStyledListWidget(UVerticalBox* ParentContainer, const FString& ItemText, const FUIStyleConfig& StyleConfig, int32 ItemIndex = 0);

    /**
     * Create a dynamic scrollable list
     * @param ParentWidget - The parent widget
     * @param Items - Array of item texts to display
     * @param StyleConfig - Style configuration
     * @param MaxVisibleItems - Maximum items to show before scrolling
     * @return Created scrollbox with list items
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling")
    static UScrollBox* CreateDynamicScrollableList(UWidget* ParentWidget, const TArray<FString>& Items, const FUIStyleConfig& StyleConfig, int32 MaxVisibleItems = 10);

    /**
     * Apply background blur effect to a widget
     * @param ParentWidget - The widget to apply blur behind
     * @param BlurStrength - Strength of the blur effect
     * @return Created background blur widget
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling")
    static UBackgroundBlur* ApplyBackgroundBlur(UWidget* ParentWidget, float BlurStrength);

    /**
     * Create a styled container with background, border, and padding
     * @param ParentWidget - The parent widget
     * @param StyleConfig - Style configuration
     * @return Created container widget (VerticalBox with Border)
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling")
    static UVerticalBox* CreateStyledContainer(UWidget* ParentWidget, const FUIStyleConfig& StyleConfig);

    /**
     * Create a styled button with text for a list
     * @param ParentContainer - The container to add the button to
     * @param ButtonText - Text for the button
     * @param StyleConfig - Style configuration
     * @param Handler - UObject that owns the callback function
     * @param OnClickHandlerName - Name of the UFUNCTION to call when button is clicked
     * @return Created button widget
     */
    template<typename T>
    static UButton* CreateStyledListButton(UVerticalBox* ParentContainer, const FString& ButtonText, const FUIStyleConfig& StyleConfig, T* Handler, const FName& OnClickHandlerName)
    {
        if (!ParentContainer || !Handler)
        {
            return nullptr;
        }

        UButton* Button = NewObject<UButton>(ParentContainer);
        StyleButton(Button, StyleConfig, FText::FromString(ButtonText));

        // Bind click event using FScriptDelegate (works with template parameters)
        FScriptDelegate Delegate;
        Delegate.BindUFunction(Handler, OnClickHandlerName);
        Button->OnClicked.Add(Delegate);

        ParentContainer->AddChild(Button);

        return Button;
    }

private:
    /**
     * Create default style configuration
     */
    static FUIStyleConfig GetDefaultStyleConfig();
};