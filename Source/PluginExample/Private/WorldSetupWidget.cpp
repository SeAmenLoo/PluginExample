#include "WorldSetupWidget.h"
#include "UIStylingHelper.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Input/Events.h"

void UWorldSetupWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);

    if (StartGameButton)
    {
        StartGameButton->OnClicked.AddDynamic(this, &UWorldSetupWidget::OnStartGameClicked);
    }

    if (GameStyleSelector)
    {
        GameStyleSelector->AddOption(TEXT("Medieval Fantasy"));
        GameStyleSelector->AddOption(TEXT("Sci-Fi Future"));
        GameStyleSelector->AddOption(TEXT("Modern Horror"));
        GameStyleSelector->AddOption(TEXT("Post-Apocalyptic"));
        GameStyleSelector->AddOption(TEXT("Custom"));
        GameStyleSelector->SetSelectedOption(TEXT("Medieval Fantasy"));
    }

    // Apply custom styling
    ApplyCustomStyling(StyleConfig);
}

FReply UWorldSetupWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Enter && InKeyEvent.IsShiftDown())
    {
        OnStartGameClicked();
        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UWorldSetupWidget::OnStartGameClicked()
{
    FString Prompt = TEXT("Create a ");

    if (GameStyleSelector)
    {
        FString Style = GameStyleSelector->GetSelectedOption();
        Prompt += Style + TEXT(" style ");
    }
    else
    {
        Prompt += TEXT("medieval fantasy ");
    }

    if (WorldPromptTextBox && !WorldPromptTextBox->GetText().IsEmpty())
    {
        Prompt += TEXT("world with the following details: ") + WorldPromptTextBox->GetText().ToString();
    }
    else
    {
        Prompt += TEXT("world with a rich lore, diverse factions, and interesting characters.");
    }

    if (PlayerCharacterNameBox && !PlayerCharacterNameBox->GetText().IsEmpty())
    {
        Prompt += FString::Printf(TEXT(" The player's character name is: %s."), *PlayerCharacterNameBox->GetText().ToString());
    }

    OnWorldSetupComplete.Broadcast(Prompt);

    RemoveFromParent();
}

void UWorldSetupWidget::OnStyleSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
}

void UWorldSetupWidget::ApplyCustomStyling(const FUIStyleConfig& InStyleConfig)
{
    StyleConfig = InStyleConfig;

    // Style title text if available
    if (TitleText)
    {
        UUIStylingHelper::StyleTextBlock(TitleText, StyleConfig, true);
        FSlateFontInfo TitleFont = TitleText->GetFont();
        TitleFont.Size = StyleConfig.LargeTitleFontSize;
        TitleText->SetFont(TitleFont);
    }

    // Style start button
    if (StartGameButton)
    {
        UUIStylingHelper::StyleButton(StartGameButton, StyleConfig, FText::FromString(TEXT("确定")));
    }

    // Style editable text boxes
    if (WorldPromptTextBox)
    {
        WorldPromptTextBox->SetForegroundColor(StyleConfig.TextColor);
    }

    if (PlayerCharacterNameBox)
    {
        PlayerCharacterNameBox->SetForegroundColor(StyleConfig.TextColor);
    }

    // Apply background styling if available
    if (BackgroundBorder)
    {
        BackgroundBorder->SetBrushColor(StyleConfig.BackgroundColor);
        BackgroundBorder->SetPadding(FMargin(StyleConfig.Padding.X * 1.5f, StyleConfig.Padding.Y * 1.5f));
    }
}
