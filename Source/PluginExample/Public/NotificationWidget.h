#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NotificationWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class UBorder;

/** Notification type for styling */
UENUM(BlueprintType)
enum class ENotificationType : uint8
{
    Info         UMETA(DisplayName = "信息"),
    Success      UMETA(DisplayName = "成功"),
    Warning      UMETA(DisplayName = "警告"),
    Error        UMETA(DisplayName = "错误"),
    Attribute    UMETA(DisplayName = "属性变化"),
    Item         UMETA(DisplayName = "道具变化")
};

/** A single floating notification entry */
USTRUCT(BlueprintType)
struct FNotificationEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Notification")
    FString Text;

    UPROPERTY(BlueprintReadOnly, Category = "Notification")
    ENotificationType Type = ENotificationType::Info;

    UPROPERTY(BlueprintReadOnly, Category = "Notification")
    float Duration = 3.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Notification")
    int32 Priority = 0;
};

/**
 * Floating notification system widget.
 * UI is built programmatically in C++; Blueprint only needs to place the
 * NotificationCanvas in the UMG designer.
 */
UCLASS()
class PLUGINEXAMPLE_API UNotificationWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // ==================== Public API ====================

    UFUNCTION(BlueprintCallable, Category = "RPG|Notification")
    void ShowNotification(const FString& Text, ENotificationType Type = ENotificationType::Info);

    UFUNCTION(BlueprintCallable, Category = "RPG|Notification")
    void ShowNotificationEx(const FString& Text, ENotificationType Type, float Duration);

    UFUNCTION(BlueprintCallable, Category = "RPG|Notification")
    void ShowAttributeChange(const FString& AttributeName, int32 OldValue, int32 NewValue);

    UFUNCTION(BlueprintCallable, Category = "RPG|Notification")
    void ShowItemChange(const FString& ItemName, bool bAdded, int32 Quantity = 1);

    UFUNCTION(BlueprintCallable, Category = "RPG|Notification")
    void ShowError(const FString& ErrorText);

    UFUNCTION(BlueprintCallable, Category = "RPG|Notification")
    void ShowSuccess(const FString& SuccessText);

    UFUNCTION(BlueprintCallable, Category = "RPG|Notification")
    void ShowWarning(const FString& WarningText);

    UFUNCTION(BlueprintCallable, Category = "RPG|Notification")
    void ClearAllNotifications();

    // ==================== BindWidget ====================

    /** Root canvas for all notification widgets (place in Blueprint) */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UCanvasPanel> NotificationCanvas;

    // ==================== Config ====================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Layout")
    float NotificationSpacing = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Animation")
    float FloatSpeed = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Animation")
    float FadeInDuration = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Animation")
    float FadeOutDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Animation")
    float DefaultDuration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Layout")
    int32 MaxNotifications = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Layout")
    float NotificationWidth = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Style")
    float BackgroundOpacity = 0.8f;

    // ==================== Colors ====================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Colors")
    FLinearColor InfoColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Colors")
    FLinearColor SuccessColor = FLinearColor(0.3f, 0.9f, 0.3f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Colors")
    FLinearColor WarningColor = FLinearColor(1.0f, 0.7f, 0.2f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Colors")
    FLinearColor ErrorColor = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Colors")
    FLinearColor AttributeUpColor = FLinearColor(0.3f, 0.9f, 0.3f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Colors")
    FLinearColor AttributeDownColor = FLinearColor(1.0f, 0.5f, 0.5f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|Notification|Colors")
    FLinearColor ItemColor = FLinearColor(0.6f, 0.8f, 1.0f, 1.0f);

protected:
    UFUNCTION(BlueprintPure, Category = "RPG|Notification")
    FLinearColor GetNotificationColor(ENotificationType Type) const;

private:
    struct FActiveNotificationWidget
    {
        TObjectPtr<UBorder> Border;
        TObjectPtr<UTextBlock> TextBlock;
        float ElapsedTime = 0.0f;
        float Duration = 3.0f;
        float StartY = 0.0f;
    };

    void CreateNotificationWidget(const FNotificationEntry& Entry);
    void UpdateNotificationWidget(int32 Index, float Progress, float VerticalOffset);
    void RemoveNotificationWidget(int32 Index);

    /** Create a styled text block */
    UTextBlock* CreateStyledText(const FString& Text, float FontSize, FLinearColor Color);

    TArray<FActiveNotificationWidget> ActiveWidgets;
};