#include "UIStylingExamples.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

FUIStyleConfig UUIStylingExamples::CreateMedievalFantasyStyle()
{
    FUIStyleConfig MedievalStyle;

    // Warm, earthy tones for medieval fantasy
    MedievalStyle.BackgroundColor = FLinearColor(0.15f, 0.1f, 0.05f, 0.9f); // Dark brown background
    MedievalStyle.BorderColor = FLinearColor(0.4f, 0.3f, 0.2f, 1.0f); // Bronze border
    MedievalStyle.TitleColor = FLinearColor(1.0f, 0.7f, 0.3f, 1.0f); // Gold title
    MedievalStyle.TextColor = FLinearColor(0.9f, 0.8f, 0.6f, 1.0f); // Cream text
    MedievalStyle.ButtonNormalColor = FLinearColor(0.25f, 0.18f, 0.12f, 1.0f); // Dark brown button
    MedievalStyle.ButtonHoveredColor = FLinearColor(0.35f, 0.25f, 0.15f, 1.0f); // Lighter brown on hover
    MedievalStyle.TitleFontSize = 24;
    MedievalStyle.NormalFontSize = 16;
    MedievalStyle.Padding = FVector2D(15.0f, 12.0f);
    MedievalStyle.BorderThickness = 3.0f;

    return MedievalStyle;
}

FUIStyleConfig UUIStylingExamples::CreateSciFiStyle()
{
    FUIStyleConfig SciFiStyle;

    // Cool, futuristic colors
    SciFiStyle.BackgroundColor = FLinearColor(0.05f, 0.1f, 0.2f, 0.9f); // Deep blue background
    SciFiStyle.BorderColor = FLinearColor(0.2f, 0.6f, 1.0f, 1.0f); // Cyan border
    SciFiStyle.TitleColor = FLinearColor(0.3f, 0.8f, 1.0f, 1.0f); // Bright cyan title
    SciFiStyle.TextColor = FLinearColor(0.8f, 0.9f, 1.0f, 1.0f); // Light blue text
    SciFiStyle.ButtonNormalColor = FLinearColor(0.1f, 0.2f, 0.35f, 1.0f); // Dark blue button
    SciFiStyle.ButtonHoveredColor = FLinearColor(0.2f, 0.4f, 0.6f, 1.0f); // Lighter blue on hover
    SciFiStyle.TitleFontSize = 22;
    SciFiStyle.NormalFontSize = 14;
    SciFiStyle.Padding = FVector2D(12.0f, 8.0f);
    SciFiStyle.BorderThickness = 2.0f;
    SciFiStyle.BlurStrength = 5.0f; // Add blur for futuristic effect

    return SciFiStyle;
}

FUIStyleConfig UUIStylingExamples::CreateHorrorStyle()
{
    FUIStyleConfig HorrorStyle;

    // Dark, unsettling colors
    HorrorStyle.BackgroundColor = FLinearColor(0.02f, 0.02f, 0.05f, 0.95f); // Very dark background
    HorrorStyle.BorderColor = FLinearColor(0.3f, 0.1f, 0.1f, 1.0f); // Dark red border
    HorrorStyle.TitleColor = FLinearColor(0.6f, 0.2f, 0.2f, 1.0f); // Blood red title
    HorrorStyle.TextColor = FLinearColor(0.7f, 0.6f, 0.6f, 1.0f); // Pale grey text
    HorrorStyle.ButtonNormalColor = FLinearColor(0.08f, 0.05f, 0.05f, 1.0f); // Dark button
    HorrorStyle.ButtonHoveredColor = FLinearColor(0.15f, 0.1f, 0.1f, 1.0f); // Slightly lighter on hover
    HorrorStyle.TitleFontSize = 20;
    HorrorStyle.NormalFontSize = 14;
    HorrorStyle.Padding = FVector2D(10.0f, 8.0f);
    HorrorStyle.BorderThickness = 1.5f;

    return HorrorStyle;
}

UScrollBox* UUIStylingExamples::CreateAIQuestList(UWidget* ParentWidget, const TArray<FString>& QuestTitles, const FUIStyleConfig& ThemeStyle)
{
    if (!ParentWidget || QuestTitles.Num() == 0)
    {
        return nullptr;
    }

    // Create styled scrollable list using UIStylingHelper
    return UUIStylingHelper::CreateDynamicScrollableList(ParentWidget, QuestTitles, ThemeStyle, 8);
}

void UUIStylingExamples::CreateDialogueHistoryList(UVerticalBox* ParentContainer, const TArray<FString>& DialogueHistory, const FUIStyleConfig& ThemeStyle)
{
    if (!ParentContainer || DialogueHistory.Num() == 0)
    {
        return;
    }

    // Clear existing history
    ParentContainer->ClearChildren();

    // Add each dialogue entry with alternating colors
    int32 EntryIndex = 0;
    for (const FString& DialogueEntry : DialogueHistory)
    {
        // Create styled list widget
        UUIStylingHelper::CreateStyledListWidget(ParentContainer, DialogueEntry, ThemeStyle, EntryIndex);
        EntryIndex++;
    }
}