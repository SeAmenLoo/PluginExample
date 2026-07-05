#pragma once

#include "CoreMinimal.h"
#include "UIStylingHelper.h"
#include "UIStylingExamples.generated.h"

/**
 * Example class demonstrating UI styling integration with AI systems
 * Shows how to use UIStylingHelper for beautifying dialogue widgets and lists
 */
UCLASS(BlueprintType)
class PLUGINEXAMPLE_API UUIStylingExamples : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Example: Apply Medieval Fantasy theme to dialogue widget
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling Examples")
    static FUIStyleConfig CreateMedievalFantasyStyle();

    /**
     * Example: Apply Sci-Fi theme to dialogue widget
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling Examples")
    static FUIStyleConfig CreateSciFiStyle();

    /**
     * Example: Apply Horror theme to dialogue widget
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling Examples")
    static FUIStyleConfig CreateHorrorStyle();

    /**
     * Example: Create a dynamic quest list from AI-generated content
     * @param ParentWidget - Widget to add the list to
     * @param QuestTitles - Array of quest titles from AI
     * @param ThemeStyle - Style configuration
     * @return Created scrollbox with quest items
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling Examples")
    static UScrollBox* CreateAIQuestList(UWidget* ParentWidget, const TArray<FString>& QuestTitles, const FUIStyleConfig& ThemeStyle);

    /**
     * Example: Create dialogue history with alternating colors
     * @param ParentContainer - Container to add history items
     * @param DialogueHistory - Array of dialogue messages
     * @param ThemeStyle - Style configuration
     */
    UFUNCTION(BlueprintCallable, Category = "UI Styling Examples")
    static void CreateDialogueHistoryList(UVerticalBox* ParentContainer, const TArray<FString>& DialogueHistory, const FUIStyleConfig& ThemeStyle);
};