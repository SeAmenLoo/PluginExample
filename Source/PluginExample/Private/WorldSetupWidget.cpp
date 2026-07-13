#include "WorldSetupWidget.h"
#include "UIStylingHelper.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Input/Events.h"
#include "Engine/GameInstance.h"
#include "LLMRuntime/Public/LLMCacheSubsystem.h"

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
        GameStyleSelector->AddOption(TEXT("中世纪奇幻"));
        GameStyleSelector->AddOption(TEXT("科幻未来"));
        GameStyleSelector->AddOption(TEXT("现代恐怖"));
        GameStyleSelector->AddOption(TEXT("后末日"));
        GameStyleSelector->AddOption(TEXT("自定义"));
        GameStyleSelector->AddOption(TEXT("使用缓存"));
        GameStyleSelector->SetSelectedOption(TEXT("中世纪奇幻"));
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
    bool bUseCacheReplay = false;

    if (GameStyleSelector)
    {
        FString Style = GameStyleSelector->GetSelectedOption();
        if (Style == TEXT("使用缓存"))
        {
            bUseCacheReplay = true;
            Prompt = TEXT("");
        }
        else
        {
            Prompt += Style + TEXT(" style ");
        }
    }
    else
    {
        Prompt += TEXT("medieval fantasy ");
    }

    if (!bUseCacheReplay)
    {
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
    }

    if (bUseCacheReplay)
    {
        UGameInstance* GI = GetGameInstance();
        if (GI)
        {
            ULLMCacheSubsystem* CacheSubsystem = GI->GetSubsystem<ULLMCacheSubsystem>();
            if (CacheSubsystem)
            {
                CacheSubsystem->SetReplayMode(true);
                UE_LOG(LogTemp, Log, TEXT("[WorldSetup] Enabled cache replay mode"));
            }
        }
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
