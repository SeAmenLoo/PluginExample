#include "WorldSetupWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"

void UWorldSetupWidget::NativeConstruct()
{
    Super::NativeConstruct();

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
