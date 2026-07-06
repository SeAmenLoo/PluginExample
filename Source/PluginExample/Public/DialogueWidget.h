#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UIStylingHelper.h"
#include "DialogueWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UScrollBox;
class UMultiLineEditableTextBox;
class USizeBox;
class UImage;
class UBorder;
class UMaterialInterface;
class UTexture2D;

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

    /** Whether to always show a leave dialogue option (default: true) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    bool bAlwaysShowLeaveOption = true;

    /** Text for the leave dialogue option */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FText LeaveOptionText = FText::FromString(TEXT("离开"));

    /** Clear all dialogue history */
    UFUNCTION(BlueprintCallable)
    void ClearHistory();

    /** Get typing speed for typewriter effect */
    UFUNCTION(BlueprintPure)
    float GetTypingSpeed() const { return TypingSpeed; }

    /** Set typing speed for typewriter effect */
    UFUNCTION(BlueprintCallable)
    void SetTypingSpeed(float Speed) { TypingSpeed = Speed; }

    /** Apply custom styling to the dialogue widget */
    UFUNCTION(BlueprintCallable)
    void ApplyCustomStyling(const FUIStyleConfig& InStyleConfig);

    /** Set background texture for the dialogue widget */
    UFUNCTION(BlueprintCallable)
    void SetBackgroundTexture(UTexture2D* BackgroundTexture);

    /** Set the NPC portrait texture for the dialogue UI (assigned by CharacterImageSubsystem) */
    UFUNCTION(BlueprintCallable, Category = "RPG|CharacterImage")
    void SetNPCPortrait(UTexture2D* InTexture);

    /** Set the player portrait texture for the dialogue UI (assigned by CharacterImageSubsystem) */
    UFUNCTION(BlueprintCallable, Category = "RPG|CharacterImage")
    void SetPlayerPortrait(UTexture2D* InTexture);

    /** Set the NPC portrait from a material (e.g. for material-based UI effects) */
    UFUNCTION(BlueprintCallable, Category = "RPG|CharacterImage")
    void SetNPCPortraitMaterial(UMaterialInterface* InMaterial);

    /** Get the current NPC portrait texture (may be null) */
    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    UTexture2D* GetNPCPortrait() const { return CurrentNPCPortrait; }

    /** Get the current player portrait texture (may be null) */
    UFUNCTION(BlueprintPure, Category = "RPG|CharacterImage")
    UTexture2D* GetPlayerPortrait() const { return CurrentPlayerPortrait; }

    /** Whether to auto-create background and border at runtime if they don't exist in Blueprint (default: true) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
    bool bAutoCreateBackground = true;

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
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

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
    void CreateRuntimeBackground();
    void FlushPendingNPCMessageToHistory();

public:
    /** Style configuration for the dialogue widget */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
    FUIStyleConfig StyleConfig;

    /** Background image widget (optional - can be added in Blueprint) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UImage> BackgroundImage;

    /** Main container border (optional - can be added in Blueprint) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> MainBorder;

    /** NPC portrait image displayed in the dialogue UI (optional, bound from Blueprint) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UImage> NPCPortraitImage;

    /** Player portrait image displayed in the dialogue UI (optional, bound from Blueprint) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UImage> PlayerPortraitImage;

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
    TObjectPtr<UMultiLineEditableTextBox> InputTextBox;

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

    /** Last NPC message kept in DialogueText (not yet in history) */
    UPROPERTY()
    bool bHasPendingLastMessage = false;

    UPROPERTY()
    FString PendingLastSpeakerName;

    UPROPERTY()
    FString PendingLastMessage;

    /** Current choices for the active message */
    UPROPERTY()
    TArray<FString> ActiveChoices;

    /** Is input currently visible */
    UPROPERTY()
    bool bInputVisible = false;

    /** Is thinking indicator visible */
    UPROPERTY()
    bool bThinkingVisible = false;

    /** Currently displayed NPC portrait (assigned by CharacterImageSubsystem) */
    UPROPERTY()
    TObjectPtr<UTexture2D> CurrentNPCPortrait;

    /** Currently displayed player portrait (assigned by CharacterImageSubsystem) */
    UPROPERTY()
    TObjectPtr<UTexture2D> CurrentPlayerPortrait;

    /** Thinking animation state */
    float ThinkingDotTimer = 0.0f;
    int32 ThinkingDotCount = 0;

    static constexpr float ThinkingDotInterval = 0.5f;
    static constexpr int32 ThinkingMaxDots = 3;

    /** Whether we need to scroll to end next frame */
    bool bNeedScrollToEnd = false;
};
