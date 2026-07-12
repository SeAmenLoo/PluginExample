#include "DialogueWidget.h"
#include "UIStylingHelper.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/NamedSlot.h"
#include "Engine/Texture2D.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"

void UDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);

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

    if (HistoryScrollBox)
    {
        HistoryScrollBox->SetAlwaysShowScrollbar(true);
        HistoryScrollBox->SetAlwaysShowScrollbarTrack(true);
        HistoryScrollBox->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
        HistoryScrollBox->SetAnimateWheelScrolling(true);
        HistoryScrollBox->SetAllowOverscroll(true);
        HistoryScrollBox->SetWheelScrollMultiplier(1.5f);
        HistoryScrollBox->SetVisibility(ESlateVisibility::Visible);
    }

    // Auto-create background if enabled and not set in Blueprint
    if (bAutoCreateBackground && !MainBorder)
    {
        CreateRuntimeBackground();
    }

    // Apply initial styling using UIStylingHelper
    ApplyCustomStyling(StyleConfig);

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

    // Newer history entries are inserted at the top, so keep the latest content in view.
    if (bNeedScrollToEnd && HistoryScrollBox)
    {
        HistoryScrollBox->ScrollToStart();
        bNeedScrollToEnd = false;
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

    int32 InsertIndex = 0;
    if (DialogueText && HistoryContainer->HasChild(DialogueText))
    {
        InsertIndex = 1;
    }

    HistoryContainer->InsertChildAt(InsertIndex, MessageEntry);

    // Mark that we need to scroll to the newest entry next frame.
    bNeedScrollToEnd = true;

    OnMessageAdded();
}

void UDialogueWidget::ShowNPCResponse(const FString& NPCName, const FString& ResponseText, const TArray<FString>& Choices)
{
    CurrentSpeakerName = NPCName;
    ActiveChoices = Choices;

    SetNPCName(NPCName);

    // Stop any ongoing typewriter effect and flush the current message to history
    if (bIsTypewriterActive)
    {
        bIsTypewriterActive = false;
        if (DialogueText)
        {
            DialogueText->SetText(FText::FromString(FullTypewriterText));
        }
        bHasPendingLastMessage = true;
        PendingLastSpeakerName = CurrentSpeakerName;
        PendingLastMessage = FullTypewriterText;
    }

    // Push previous pending NPC message into history before starting new one
    if (bHasPendingLastMessage)
    {
        AddMessageToHistory(PendingLastSpeakerName, PendingLastMessage, false);
        bHasPendingLastMessage = false;
    }

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

    // Show choices after typewriter completes
    // Always show choices container if we have choices or always show leave option
    if (Choices.Num() > 0 || bAlwaysShowLeaveOption)
    {
        CurrentChoices = Choices;
    }
}

void UDialogueWidget::ShowInputField()
{
    if (InputContainer)
    {
        InputContainer->SetVisibility(ESlateVisibility::Visible);
    }
    if (InputTextBox)
    {
        InputTextBox->SetVisibility(ESlateVisibility::Visible);
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

        // Keep the last message in DialogueText, store as pending for next message
        bHasPendingLastMessage = true;
        PendingLastSpeakerName = CurrentSpeakerName;
        PendingLastMessage = FullTypewriterText;

        if (ActiveChoices.Num() > 0 || bAlwaysShowLeaveOption)
        {
            ShowChoices(ActiveChoices);
        }

        OnTypewriterComplete();
    }
}

void UDialogueWidget::ShowThinkingIndicator()
{
    // Stop any ongoing typewriter effect and flush to history
    if (bIsTypewriterActive)
    {
        bIsTypewriterActive = false;
        if (DialogueText)
        {
            DialogueText->SetText(FText::FromString(FullTypewriterText));
        }
        bHasPendingLastMessage = true;
        PendingLastSpeakerName = CurrentSpeakerName;
        PendingLastMessage = FullTypewriterText;
    }

    FlushPendingNPCMessageToHistory();

    bThinkingVisible = true;
    ThinkingDotTimer = 0.0f;
    ThinkingDotCount = 0;

    if (ThinkingText)
    {
        ThinkingText->SetVisibility(ESlateVisibility::Visible);
        ThinkingText->SetText(FText::FromString(TEXT("思考中")));
    }

    // Clear current dialogue text
    if (DialogueText)
    {
        DialogueText->SetText(FText::GetEmpty());
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
    ChoicesContainer->SetVisibility(ESlateVisibility::Visible);

    ActiveChoices = Choices;
    CurrentChoices = Choices;

    int32 ChoiceIndex = 0;
    for (; ChoiceIndex < Choices.Num(); ++ChoiceIndex)
    {
        UButton* ChoiceButton = NewObject<UButton>(ChoicesBox);
        UTextBlock* ChoiceText = NewObject<UTextBlock>(ChoiceButton);

        ChoiceText->SetText(FText::FromString(FString::Printf(TEXT("%d. %s"), ChoiceIndex + 1, *Choices[ChoiceIndex])));
        ChoiceText->SetColorAndOpacity(FLinearColor::White);
        ChoiceText->SetAutoWrapText(true);

        ChoiceButton->AddChild(ChoiceText);
        ChoicesBox->AddChild(ChoiceButton);

        const int32 Index = ChoiceIndex;
        TSharedRef<SWidget> WidgetRef = ChoiceButton->TakeWidget();
        TSharedRef<SButton> SlateButtonRef = StaticCastSharedRef<SButton>(WidgetRef);
        SlateButtonRef->SetOnClicked(FOnClicked::CreateWeakLambda(this, [this, Index]() -> FReply
        {
            SelectChoice(Index);
            return FReply::Handled();
        }));
    }

    if (bAlwaysShowLeaveOption)
    {
        UButton* LeaveButton = NewObject<UButton>(ChoicesBox);
        UTextBlock* LeaveText = NewObject<UTextBlock>(LeaveButton);

        LeaveText->SetText(LeaveOptionText);
        LeaveText->SetColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f));
        LeaveText->SetAutoWrapText(true);

        LeaveButton->AddChild(LeaveText);
        ChoicesBox->AddChild(LeaveButton);

        TSharedRef<SWidget> WidgetRef = LeaveButton->TakeWidget();
        TSharedRef<SButton> SlateButtonRef = StaticCastSharedRef<SButton>(WidgetRef);
        SlateButtonRef->SetOnClicked(FOnClicked::CreateWeakLambda(this, [this]() -> FReply
        {
            CloseDialogue();
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
        // Preserve chronological order in reverse history view:
        // flush the currently displayed NPC line before inserting the newer player choice.
        FlushPendingNPCMessageToHistory();

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
        // Preserve chronological order in reverse history view:
        // flush the currently displayed NPC line before inserting the newer player message.
        FlushPendingNPCMessageToHistory();

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
        bIsTypewriterActive = false;
        if (DialogueText)
        {
            DialogueText->SetText(FText::FromString(FullTypewriterText));
        }

        bHasPendingLastMessage = true;
        PendingLastSpeakerName = CurrentSpeakerName;
        PendingLastMessage = FullTypewriterText;

        if (ActiveChoices.Num() > 0 || bAlwaysShowLeaveOption)
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
}

FReply UDialogueWidget::NativeOnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Enter && InKeyEvent.IsShiftDown())
    {
        SubmitInput();
        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(MyGeometry, InKeyEvent);
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

void UDialogueWidget::FlushPendingNPCMessageToHistory()
{
    if (bHasPendingLastMessage)
    {
        AddMessageToHistory(PendingLastSpeakerName, PendingLastMessage, false);
        bHasPendingLastMessage = false;
    }
}

FReply UDialogueWidget::NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    return Super::NativeOnKeyDown(MyGeometry, InKeyEvent);
}

void UDialogueWidget::ApplyCustomStyling(const FUIStyleConfig& InStyleConfig)
{
    StyleConfig = InStyleConfig;

    // Style NPC name text
    if (NPCNameText)
    {
        UUIStylingHelper::StyleTextBlock(NPCNameText, StyleConfig, true);
        FSlateFontInfo NameFont = NPCNameText->GetFont();
        NameFont.Size = StyleConfig.LargeTitleFontSize;
        NPCNameText->SetFont(NameFont);
    }

    // Style dialogue text
    if (DialogueText)
    {
        UUIStylingHelper::StyleTextBlock(DialogueText, StyleConfig, false);
        FSlateFontInfo DialogueFont = DialogueText->GetFont();
        DialogueFont.Size = StyleConfig.TitleFontSize;
        DialogueText->SetFont(DialogueFont);
    }

    // Style thinking indicator
    if (ThinkingText)
    {
        UUIStylingHelper::StyleTextBlock(ThinkingText, StyleConfig, false);
        ThinkingText->SetColorAndOpacity(FLinearColor(0.5f, 0.7f, 1.0f, 1.0f));
    }

    // Style buttons
    if (SendButton)
    {
        UUIStylingHelper::StyleButton(SendButton, StyleConfig, FText::FromString(TEXT("确定")));
    }

    if (ContinueButton)
    {
        UUIStylingHelper::StyleButton(ContinueButton, StyleConfig, FText::FromString(TEXT("继续")));
    }

    // Adjust history scroll box height
    if (HistoryScrollBox)
    {
        if (USizeBox* SizeBox = Cast<USizeBox>(HistoryScrollBox->GetParent()))
        {
            SizeBox->SetMinDesiredHeight(200.0f);
        }
    }

    // Apply background styling if MainBorder is available
    if (MainBorder)
    {
        MainBorder->SetBrushColor(StyleConfig.BackgroundColor);
        MainBorder->SetPadding(FMargin(StyleConfig.Padding.X, StyleConfig.Padding.Y));
    }
}

void UDialogueWidget::SetBackgroundTexture(UTexture2D* BackgroundTexture)
{
    if (BackgroundImage && BackgroundTexture)
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(BackgroundTexture);
        Brush.ImageSize = FVector2D(BackgroundTexture->GetSizeX(), BackgroundTexture->GetSizeY());
        Brush.DrawAs = ESlateBrushDrawType::Image;
        BackgroundImage->SetBrush(Brush);
        BackgroundImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
}

void UDialogueWidget::SetNPCPortrait(UTexture2D* InTexture)
{
    CurrentNPCPortrait = InTexture;

    if (!NPCPortraitImage)
    {
        return;
    }

    if (InTexture)
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(InTexture);
        Brush.ImageSize = FVector2D(InTexture->GetSizeX(), InTexture->GetSizeY());
        Brush.DrawAs = ESlateBrushDrawType::Image;
        NPCPortraitImage->SetBrush(Brush);
        NPCPortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    else
    {
        NPCPortraitImage->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDialogueWidget::SetPlayerPortrait(UTexture2D* InTexture)
{
    CurrentPlayerPortrait = InTexture;

    if (!PlayerPortraitImage)
    {
        return;
    }

    if (InTexture)
    {
        if (WhiteBgTransparentMaterial)
        {
            UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(WhiteBgTransparentMaterial, this);
            if (MID)
            {
                MID->SetTextureParameterValue(TEXT("SpriteTexture"), InTexture);
                FSlateBrush Brush;
                Brush.SetResourceObject(MID);
                Brush.ImageSize = FVector2D(InTexture->GetSizeX(), InTexture->GetSizeY());
                Brush.DrawAs = ESlateBrushDrawType::Image;
                PlayerPortraitImage->SetBrush(Brush);
            }
        }
        else
        {
            FSlateBrush Brush;
            Brush.SetResourceObject(InTexture);
            Brush.ImageSize = FVector2D(InTexture->GetSizeX(), InTexture->GetSizeY());
            Brush.DrawAs = ESlateBrushDrawType::Image;
            PlayerPortraitImage->SetBrush(Brush);
        }
        PlayerPortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    else
    {
        PlayerPortraitImage->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDialogueWidget::SetNPCPortraitMaterial(UMaterialInterface* InMaterial)
{
    if (!NPCPortraitImage)
    {
        return;
    }

    if (InMaterial)
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(InMaterial);
        Brush.ImageSize = FVector2D(256.0f, 256.0f);
        Brush.DrawAs = ESlateBrushDrawType::Image;
        NPCPortraitImage->SetBrush(Brush);
        NPCPortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    else
    {
        NPCPortraitImage->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDialogueWidget::CreateRuntimeBackground()
{
    if (MainBorder)
    {
        return;
    }

    // Create a border as the main background container
    MainBorder = NewObject<UBorder>(this);
    MainBorder->Rename(TEXT("MainBorder"));
    MainBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    // Set initial background color
    MainBorder->SetBrushColor(StyleConfig.BackgroundColor);
    MainBorder->SetPadding(FMargin(StyleConfig.Padding.X, StyleConfig.Padding.Y));

    // Create border brush with corner radius
    FSlateBrush BorderBrush;
    BorderBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
    BorderBrush.TintColor = StyleConfig.BorderColor;
    FVector4 CornerRadii = FVector4(
        StyleConfig.CornerRadius,
        StyleConfig.CornerRadius,
        StyleConfig.CornerRadius,
        StyleConfig.CornerRadius
    );
    BorderBrush.OutlineSettings.CornerRadii = CornerRadii;
    BorderBrush.OutlineSettings.Width = StyleConfig.BorderThickness;
    MainBorder->SetBrush(BorderBrush);

    // If we have a root widget, try to wrap content in the border
    // Note: In UMG, we can't easily reparent widgets at runtime
    // Instead, we add the border as a background and it sits behind content
}
