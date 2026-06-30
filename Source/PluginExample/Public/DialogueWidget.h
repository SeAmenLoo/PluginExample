#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DialogueWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UScrollBox;
class UEditableTextBox;
class USizeBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChoiceSelected, int32, ChoiceIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMessageSubmitted, const FString&, MessageText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueClosed);

UCLASS()
class PLUGINEXAMPLE_API UDialogueWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Set the NPC name to display */
    UFUNCTION(BlueprintCallable)
    void SetNPCName(const FString& Name);

    /** Add a message to the dialogue history (from player or NPC) */
    UFUNCTION(BlueprintCallable)
    void AddMessageToHistory(const FString& SpeakerName, const FString& Message, bool bIsPlayer);

    /** Show NPC response with typewriter effect */
    UFUNCTION(BlueprintCallable)
    void ShowNPCResponse(const FString& NPCName, const FString& ResponseText, const TArray<FString>& Choices);

    /** Show dialogue input field */
    UFUNCTION(BlueprintCallable)
    void ShowInputField();

    /** Hide dialogue input field */
    UFUNCTION(BlueprintCallable)
    void HideInputField();

    /** Called when typewriter effect completes */
    UFUNCTION(BlueprintImplementableEvent)
    void OnTypewriterComplete();

    /** Called when a new message is added */
    UFUNCTION(BlueprintImplementableEvent)
    void OnMessageAdded();

    /** Select a dialogue choice */
    UFUNCTION(BlueprintCallable)
    void SelectChoice(int32 ChoiceIndex);

    /** Submit text input */
    UFUNCTION(BlueprintCallable)
    void SubmitInput();

    /** Show dialogue choices */
    UFUNCTION(BlueprintCallable)
    void ShowChoices(const TArray<FString>& Choices);

    /** Show thinking indicator while waiting for AI response */
    UFUNCTION(BlueprintCallable)
    void ShowThinkingIndicator();

    /** Hide thinking indicator */
    UFUNCTION(BlueprintCallable)
    void HideThinkingIndicator();

    /** Check if thinking indicator is visible */
    UFUNCTION(BlueprintPure)
    bool IsThinkingIndicatorVisible() const { return bThinkingVisible; }

    /** Close the dialogue */
    UFUNCTION(BlueprintCallable)
    void CloseDialogue();

    /** Check if there are choices to display */
    UFUNCTION(BlueprintPure)
    bool HasChoices() const { return CurrentChoices.Num() > 0; }

    /** Clear all dialogue history */
    UFUNCTION(BlueprintCallable)
    void ClearHistory();

    /** Get typing speed for typewriter effect */
    UFUNCTION(BlueprintPure)
    float GetTypingSpeed() const { return TypingSpeed; }

    /** Set typing speed for typewriter effect */
    UFUNCTION(BlueprintCallable)
    void SetTypingSpeed(float Speed) { TypingSpeed = Speed; }

    /** Delegate when player selects a choice */
    UPROPERTY(BlueprintAssignable)
    FOnChoiceSelected OnChoiceSelectedDelegate;

    /** Delegate when player submits a text message */
    UPROPERTY(BlueprintAssignable)
    FOnMessageSubmitted OnMessageSubmittedDelegate;

    /** Delegate when dialogue is closed */
    UPROPERTY(BlueprintAssignable)
    FOnDialogueClosed OnDialogueClosedDelegate;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Typing animation tick - call this in tick or timer */
    UFUNCTION(BlueprintCallable)
    void UpdateTypewriter(float DeltaTime);

    UFUNCTION()
    void OnSendClicked();

    UFUNCTION()
    void OnContinueClicked();

    UFUNCTION()
    void OnChoiceClicked(int32 Index);

    UFUNCTION()
    void OnInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

private:
    void StartTypewriterEffect(const FString& FullText);
    void AdvanceToNextMessage();

public:
    /** NPC name text */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> NPCNameText;

    /** Dialogue history scroll box */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UScrollBox> HistoryScrollBox;

    /** Container for dialogue history items */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UVerticalBox> HistoryContainer;

    /** Current NPC dialogue text (for typewriter effect) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> DialogueText;

    /** Choices vertical box */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UVerticalBox> ChoicesBox;

    /** Continue button */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UButton> ContinueButton;

    /** Player input text box */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UEditableTextBox> InputTextBox;

    /** Send button */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UButton> SendButton;

    /** Input container */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<USizeBox> InputContainer;

    /** Choices container */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<USizeBox> ChoicesContainer;

    /** Thinking indicator text (optional - if present in widget blueprint) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ThinkingText;

    /** Typing speed (characters per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TypingSpeed = 30.0f;

private:
    /** Current choices array */
    UPROPERTY()
    TArray<FString> CurrentChoices;

    /** Typewriter effect state */
    UPROPERTY()
    FString FullTypewriterText;

    UPROPERTY()
    int32 TypewriterIndex = 0;

    UPROPERTY()
    bool bIsTypewriterActive = false;

    float TypewriterAccumulator = 0.0f;

    /** Message queue for auto-advancing */
    TArray<TPair<FString, FString>> PendingNPCMessages;

    /** Current speaker name */
    UPROPERTY()
    FString CurrentSpeakerName;

    /** Current choices for the active message */
    UPROPERTY()
    TArray<FString> ActiveChoices;

    /** Is input currently visible */
    UPROPERTY()
    bool bInputVisible = false;

    /** Is thinking indicator visible */
    UPROPERTY()
    bool bThinkingVisible = false;

    /** Thinking animation state */
    float ThinkingDotTimer = 0.0f;
    int32 ThinkingDotCount = 0;

    static constexpr float ThinkingDotInterval = 0.5f;
    static constexpr int32 ThinkingMaxDots = 3;
};
