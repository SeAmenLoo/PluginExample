#include "DialogueWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/NamedSlot.h"
#include "Widgets/Input/SButton.h"

void UDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Bind button events
    if (SendButton)
    {
        SendButton->OnClicked.AddDynamic(this, &UDialogueWidget::OnSendClicked);
    }

    if (ContinueButton)
    {
        ContinueButton->OnClicked.AddDynamic(this, &UDialogueWidget::OnContinueClicked);
    }

    if (InputTextBox)
    {
        InputTextBox->OnTextCommitted.AddDynamic(this, &UDialogueWidget::OnInputCommitted);
    }

    // Style NPC name
    if (NPCNameText)
    {
        FSlateFontInfo NameFont = NPCNameText->GetFont();
        NameFont.Size = 24;
        NPCNameText->SetFont(NameFont);
        NPCNameText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.5f, 1.0f));
    }

    // Style dialogue text
    if (DialogueText)
    {
        FSlateFontInfo DialogueFont = DialogueText->GetFont();
        DialogueFont.Size = 18;
        DialogueText->SetFont(DialogueFont);
        DialogueText->SetColorAndOpacity(FLinearColor(0.95f, 0.95f, 1.0f, 1.0f));
        DialogueText->SetAutoWrapText(true);
    }

    // Style thinking indicator
    if (ThinkingText)
    {
        FSlateFontInfo ThinkingFont = ThinkingText->GetFont();
        ThinkingFont.Size = 16;
        ThinkingText->SetFont(ThinkingFont);
        ThinkingText->SetColorAndOpacity(FLinearColor(0.5f, 0.7f, 1.0f, 1.0f));
    }

    // Hide input initially
    HideInputField();
}

void UDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bIsTypewriterActive)
    {
        UpdateTypewriter(InDeltaTime);
    }

    if (bThinkingVisible && ThinkingText)
    {
        ThinkingDotTimer += InDeltaTime;
        if (ThinkingDotTimer >= ThinkingDotInterval)
        {
            ThinkingDotTimer = 0.0f;
            ThinkingDotCount = (ThinkingDotCount + 1) % (ThinkingMaxDots + 1);

            FString DotsStr;
            for (int32 i = 0; i < ThinkingDotCount; ++i)
            {
                DotsStr += TEXT(".");
            }
            ThinkingText->SetText(FText::FromString(TEXT("思考中") + DotsStr));
        }
    }
}

void UDialogueWidget::SetNPCName(const FString& Name)
{
    if (NPCNameText)
    {
        NPCNameText->SetText(FText::FromString(Name));
    }
}

void UDialogueWidget::AddMessageToHistory(const FString& SpeakerName, const FString& Message, bool bIsPlayer)
{
    if (!HistoryContainer)
    {
        return;
    }

    // Create message entry widget
    UVerticalBox* MessageEntry = NewObject<UVerticalBox>(HistoryContainer);

    // Create text block for speaker
    UTextBlock* SpeakerText = NewObject<UTextBlock>(MessageEntry);
    SpeakerText->SetText(FText::FromString(bIsPlayer ? TEXT("You") : SpeakerName));
    SpeakerText->SetColorAndOpacity(bIsPlayer ? FLinearColor(0.3f, 0.8f, 1.0f) : FLinearColor(1.0f, 0.8f, 0.3f));
    FSlateFontInfo SpeakerFont = SpeakerText->GetFont();
    SpeakerFont.Size = 16;
    SpeakerText->SetFont(SpeakerFont);
    SpeakerText->SetAutoWrapText(true);
    MessageEntry->AddChild(SpeakerText);

    // Create text block for message
    UTextBlock* MessageText = NewObject<UTextBlock>(MessageEntry);
    MessageText->SetText(FText::FromString(Message));
    MessageText->SetColorAndOpacity(FLinearColor::White);
    FSlateFontInfo MessageFont = MessageText->GetFont();
    MessageFont.Size = 14;
    MessageText->SetFont(MessageFont);
    MessageText->SetAutoWrapText(true);
    MessageEntry->AddChild(MessageText);

    HistoryContainer->AddChild(MessageEntry);

    // Scroll to bottom
    if (HistoryScrollBox)
    {
        HistoryScrollBox->ScrollToEnd();
    }

    OnMessageAdded();
}

void UDialogueWidget::ShowNPCResponse(const FString& NPCName, const FString& ResponseText, const TArray<FString>& Choices)
{
    CurrentSpeakerName = NPCName;
    ActiveChoices = Choices;

    SetNPCName(NPCName);

    // Clear current dialogue text
    if (DialogueText)
    {
        DialogueText->SetText(FText::GetEmpty());
    }

    // Clear previous choices
    if (ChoicesBox)
    {
        ChoicesBox->ClearChildren();
    }

    // Start typewriter effect
    StartTypewriterEffect(ResponseText);

    // Show choices after typewriter completes (or immediately if no choices)
    if (Choices.Num() > 0)
    {
        // Store choices for after typewriter
        CurrentChoices = Choices;
    }
}

void UDialogueWidget::ShowInputField()
{
    if (InputContainer)
    {
        InputContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    if (InputTextBox)
    {
        InputTextBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        FSlateApplication::Get().SetKeyboardFocus(InputTextBox->GetCachedWidget());
    }
    bInputVisible = true;
}

void UDialogueWidget::HideInputField()
{
    if (InputContainer)
    {
        InputContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
    bInputVisible = false;
}

void UDialogueWidget::StartTypewriterEffect(const FString& FullText)
{
    FullTypewriterText = FullText;
    TypewriterIndex = 0;
    TypewriterAccumulator = 0.0f;
    bIsTypewriterActive = true;

    if (DialogueText)
    {
        DialogueText->SetText(FText::GetEmpty());
    }
}

void UDialogueWidget::UpdateTypewriter(float DeltaTime)
{
    if (!bIsTypewriterActive || !DialogueText)
    {
        return;
    }

    TypewriterAccumulator += TypingSpeed * DeltaTime;
    int32 CharsThisFrame = FMath::FloorToInt(TypewriterAccumulator);
    TypewriterAccumulator -= CharsThisFrame;

    for (int32 i = 0; i < CharsThisFrame && TypewriterIndex < FullTypewriterText.Len(); ++i)
    {
        FString Char = FullTypewriterText.Mid(TypewriterIndex, 1);
        DialogueText->SetText(FText::FromString(DialogueText->GetText().ToString() + Char));
        TypewriterIndex++;
    }

    if (TypewriterIndex >= FullTypewriterText.Len())
    {
        bIsTypewriterActive = false;
        DialogueText->SetText(FText::FromString(FullTypewriterText));

        AddMessageToHistory(CurrentSpeakerName, FullTypewriterText, false);

        if (ActiveChoices.Num() > 0)
        {
            ShowChoices(ActiveChoices);
        }

        OnTypewriterComplete();
    }
}

void UDialogueWidget::ShowThinkingIndicator()
{
    bThinkingVisible = true;
    ThinkingDotTimer = 0.0f;
    ThinkingDotCount = 0;

    if (ThinkingText)
    {
        ThinkingText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        ThinkingText->SetText(FText::FromString(TEXT("思考中")));
    }

    HideInputField();

    if (ChoicesContainer)
    {
        ChoicesContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDialogueWidget::HideThinkingIndicator()
{
    bThinkingVisible = false;
    ThinkingDotTimer = 0.0f;
    ThinkingDotCount = 0;

    if (ThinkingText)
    {
        ThinkingText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDialogueWidget::ShowChoices(const TArray<FString>& Choices)
{
    if (!ChoicesBox || !ChoicesContainer)
    {
        return;
    }

    ChoicesBox->ClearChildren();
    ChoicesContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    for (int32 i = 0; i < Choices.Num(); ++i)
    {
        UButton* ChoiceButton = NewObject<UButton>(ChoicesBox);
        UTextBlock* ChoiceText = NewObject<UTextBlock>(ChoiceButton);

        ChoiceText->SetText(FText::FromString(FString::Printf(TEXT("%d. %s"), i + 1, *Choices[i])));
        ChoiceText->SetColorAndOpacity(FLinearColor::White);
        ChoiceText->SetAutoWrapText(true);

        ChoiceButton->AddChild(ChoiceText);

        ChoicesBox->AddChild(ChoiceButton);

        const int32 Index = i;
        TSharedRef<SWidget> WidgetRef = ChoiceButton->TakeWidget();
        TSharedRef<SButton> SlateButtonRef = StaticCastSharedRef<SButton>(WidgetRef);
        SlateButtonRef->SetOnClicked(FOnClicked::CreateWeakLambda(this, [this, Index]() -> FReply
        {
            SelectChoice(Index);
            return FReply::Handled();
        }));
    }

    ShowInputField();

    if (ContinueButton)
    {
        ContinueButton->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDialogueWidget::SelectChoice(int32 ChoiceIndex)
{
    if (ChoiceIndex >= 0 && ChoiceIndex < CurrentChoices.Num())
    {
        // Add choice to history
        FString ChoiceText = FString::Printf(TEXT("[%d] %s"), ChoiceIndex + 1, *CurrentChoices[ChoiceIndex]);
        AddMessageToHistory(TEXT("You"), CurrentChoices[ChoiceIndex], true);

        // Clear choices
        if (ChoicesBox)
        {
            ChoicesBox->ClearChildren();
        }
        if (ChoicesContainer)
        {
            ChoicesContainer->SetVisibility(ESlateVisibility::Collapsed);
        }

        CurrentChoices.Empty();

        // Notify via delegate
        OnChoiceSelectedDelegate.Broadcast(ChoiceIndex);
    }
}

void UDialogueWidget::SubmitInput()
{
    if (!InputTextBox)
    {
        return;
    }

    FString InputText = InputTextBox->GetText().ToString();
    if (!InputText.IsEmpty())
    {
        // Add to history
        AddMessageToHistory(TEXT("You"), InputText, true);

        // Notify via delegate
        OnMessageSubmittedDelegate.Broadcast(InputText);

        // Clear input
        InputTextBox->SetText(FText::GetEmpty());
    }

    // Hide input after sending
    HideInputField();
}

void UDialogueWidget::CloseDialogue()
{
    // Clear history
    ClearHistory();

    // Clear typewriter state
    bIsTypewriterActive = false;
    FullTypewriterText.Empty();
    TypewriterIndex = 0;

    // Clear choices
    if (ChoicesBox)
    {
        ChoicesBox->ClearChildren();
    }

    // Notify delegate
    OnDialogueClosedDelegate.Broadcast();

    // Remove from parent
    RemoveFromParent();
}

void UDialogueWidget::ClearHistory()
{
    if (HistoryContainer)
    {
        HistoryContainer->ClearChildren();
    }
}

void UDialogueWidget::OnSendClicked()
{
    SubmitInput();
}

void UDialogueWidget::OnContinueClicked()
{
    if (bIsTypewriterActive)
    {
        // Skip typewriter - show full text immediately
        bIsTypewriterActive = false;
        if (DialogueText)
        {
            DialogueText->SetText(FText::FromString(FullTypewriterText));
        }
        AddMessageToHistory(CurrentSpeakerName, FullTypewriterText, false);

        if (ActiveChoices.Num() > 0)
        {
            ShowChoices(ActiveChoices);
        }

        OnTypewriterComplete();
    }
    else if (!bInputVisible)
    {
        // Show input field for free text
        ShowInputField();
    }
}

void UDialogueWidget::OnChoiceClicked(int32 Index)
{
    SelectChoice(Index);
}

void UDialogueWidget::OnInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter)
    {
        SubmitInput();
    }
}

void UDialogueWidget::AdvanceToNextMessage()
{
    if (PendingNPCMessages.Num() > 0)
    {
        auto Message = PendingNPCMessages[0];
        PendingNPCMessages.RemoveAt(0);
        ShowNPCResponse(Message.Key, Message.Value, TArray<FString>());
    }
}
