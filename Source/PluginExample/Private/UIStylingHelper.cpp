#include "UIStylingHelper.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/SlateStyleRegistry.h"

UImage* UUIStylingHelper::CreateStyledBackground(UWidget* ParentWidget, const FUIStyleConfig& StyleConfig, UTexture2D* BackgroundTexture)
{
    if (!ParentWidget)
    {
        return nullptr;
    }

    UImage* BackgroundImage = NewObject<UImage>(ParentWidget);
    BackgroundImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (BackgroundTexture)
    {
        // Use texture as background
        FSlateBrush Brush;
        Brush.SetResourceObject(BackgroundTexture);
        Brush.ImageSize = FVector2D(BackgroundTexture->GetSizeX(), BackgroundTexture->GetSizeY());
        Brush.DrawAs = ESlateBrushDrawType::Image;
        BackgroundImage->SetBrush(Brush);
    }
    else
    {
        // Use solid color background
        BackgroundImage->SetColorAndOpacity(StyleConfig.BackgroundColor);
    }

    return BackgroundImage;
}

UBorder* UUIStylingHelper::CreateStyledBorder(UWidget* ParentWidget, const FUIStyleConfig& StyleConfig)
{
    if (!ParentWidget)
    {
        return nullptr;
    }

    UBorder* BorderWidget = NewObject<UBorder>(ParentWidget);
    BorderWidget->SetBrushColor(StyleConfig.BorderColor);

    // Configure border appearance
    FSlateBrush BorderBrush;
    BorderBrush.DrawAs = ESlateBrushDrawType::Border;
    BorderBrush.Margin = FVector4(StyleConfig.BorderThickness, StyleConfig.BorderThickness, StyleConfig.BorderThickness, StyleConfig.BorderThickness);
    BorderBrush.TintColor = StyleConfig.BorderColor;
    BorderWidget->SetBrush(BorderBrush);

    // Set padding
    BorderWidget->SetPadding(FMargin(StyleConfig.Padding.X, StyleConfig.Padding.Y));

    return BorderWidget;
}

void UUIStylingHelper::StyleButton(UButton* Button, const FUIStyleConfig& StyleConfig, const FText& ButtonText)
{
    if (!Button)
    {
        return;
    }

    // Create a custom button style with hover, pressed, and normal states
    FButtonStyle ButtonStyle;

    // Set up the normal state
    ButtonStyle.Normal.TintColor = StyleConfig.ButtonNormalColor;
    ButtonStyle.Normal.DrawAs = ESlateBrushDrawType::RoundedBox;
    ButtonStyle.Normal.OutlineSettings.CornerRadii = FVector4(
        StyleConfig.CornerRadius * 0.5f,
        StyleConfig.CornerRadius * 0.5f,
        StyleConfig.CornerRadius * 0.5f,
        StyleConfig.CornerRadius * 0.5f
    );
    ButtonStyle.Normal.OutlineSettings.Width = 1.0f;
    ButtonStyle.Normal.OutlineSettings.Color = StyleConfig.BorderColor;

    // Set up the hovered state
    ButtonStyle.Hovered.TintColor = StyleConfig.ButtonHoveredColor;
    ButtonStyle.Hovered.DrawAs = ESlateBrushDrawType::RoundedBox;
    ButtonStyle.Hovered.OutlineSettings.CornerRadii = FVector4(
        StyleConfig.CornerRadius * 0.5f,
        StyleConfig.CornerRadius * 0.5f,
        StyleConfig.CornerRadius * 0.5f,
        StyleConfig.CornerRadius * 0.5f
    );
    ButtonStyle.Hovered.OutlineSettings.Width = 1.5f;
    ButtonStyle.Hovered.OutlineSettings.Color = FLinearColor(StyleConfig.BorderColor.R * 1.2f, StyleConfig.BorderColor.G * 1.2f, StyleConfig.BorderColor.B * 1.2f, 1.0f);

    // Set up the pressed state
    ButtonStyle.Pressed.TintColor = StyleConfig.ButtonPressedColor;
    ButtonStyle.Pressed.DrawAs = ESlateBrushDrawType::RoundedBox;
    ButtonStyle.Pressed.OutlineSettings.CornerRadii = FVector4(
        StyleConfig.CornerRadius * 0.5f,
        StyleConfig.CornerRadius * 0.5f,
        StyleConfig.CornerRadius * 0.5f,
        StyleConfig.CornerRadius * 0.5f
    );
    ButtonStyle.Pressed.OutlineSettings.Width = 2.0f;
    ButtonStyle.Pressed.OutlineSettings.Color = StyleConfig.BorderColor;

    // Set padding
    ButtonStyle.Normal.ImageSize = FVector2D(100, 40);
    ButtonStyle.Hovered.ImageSize = FVector2D(100, 40);
    ButtonStyle.Pressed.ImageSize = FVector2D(100, 40);

    // Apply the custom style to the button
    Button->SetStyle(ButtonStyle);

    // Add text to button if provided
    if (!ButtonText.IsEmpty())
    {
        UTextBlock* TextWidget = NewObject<UTextBlock>(Button);
        TextWidget->SetText(ButtonText);
        StyleTextBlock(TextWidget, StyleConfig, false);
        Button->AddChild(TextWidget);
    }
}

void UUIStylingHelper::StyleTextBlock(UTextBlock* TextBlock, const FUIStyleConfig& StyleConfig, bool bIsTitle)
{
    if (!TextBlock)
    {
        return;
    }

    // Set color
    FLinearColor Color = bIsTitle ? StyleConfig.TitleColor : StyleConfig.TextColor;
    TextBlock->SetColorAndOpacity(Color);

    // Set font size
    FSlateFontInfo FontInfo = TextBlock->GetFont();
    FontInfo.Size = bIsTitle ? StyleConfig.TitleFontSize : StyleConfig.NormalFontSize;
    TextBlock->SetFont(FontInfo);

    // Enable text wrapping
    TextBlock->SetAutoWrapText(true);
}

UBorder* UUIStylingHelper::CreateStyledListWidget(UVerticalBox* ParentContainer, const FString& ItemText, const FUIStyleConfig& StyleConfig, int32 ItemIndex)
{
    if (!ParentContainer)
    {
        return nullptr;
    }

    // Create border for the item directly
    UBorder* ItemBorder = NewObject<UBorder>(ParentContainer);
    ItemBorder->SetPadding(FMargin(StyleConfig.Padding.X * 0.5f, StyleConfig.Padding.Y * 0.5f));

    // Alternating background colors for better readability
    FLinearColor ItemBackgroundColor = (ItemIndex % 2 == 0)
        ? FLinearColor(0.12f, 0.12f, 0.18f, 0.9f)
        : FLinearColor(0.15f, 0.15f, 0.22f, 0.9f);
    ItemBorder->SetBrushColor(ItemBackgroundColor);

    // Create text for the item
    UTextBlock* ItemTextBlock = NewObject<UTextBlock>(ItemBorder);
    ItemTextBlock->SetText(FText::FromString(ItemText));
    StyleTextBlock(ItemTextBlock, StyleConfig, false);
    ItemBorder->AddChild(ItemTextBlock);

    ParentContainer->AddChild(ItemBorder);

    return ItemBorder;
}

UScrollBox* UUIStylingHelper::CreateDynamicScrollableList(UWidget* ParentWidget, const TArray<FString>& Items, const FUIStyleConfig& StyleConfig, int32 MaxVisibleItems)
{
    if (!ParentWidget)
    {
        return nullptr;
    }

    // Create scroll box
    UScrollBox* ScrollBox = NewObject<UScrollBox>(ParentWidget);
    ScrollBox->SetOrientation(EOrientation::Orient_Vertical);
    ScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);

    // Create container for items
    UVerticalBox* ListContainer = NewObject<UVerticalBox>(ScrollBox);

    // Add styled list items
    int32 ItemIndex = 0;
    for (const FString& Item : Items)
    {
        CreateStyledListWidget(ListContainer, Item, StyleConfig, ItemIndex);
        ItemIndex++;
    }

    ScrollBox->AddChild(ListContainer);

    return ScrollBox;
}

UBackgroundBlur* UUIStylingHelper::ApplyBackgroundBlur(UWidget* ParentWidget, float BlurStrength)
{
    if (!ParentWidget || BlurStrength <= 0.0f)
    {
        return nullptr;
    }

    UBackgroundBlur* BlurWidget = NewObject<UBackgroundBlur>(ParentWidget);
    BlurWidget->SetBlurStrength(BlurStrength);
    BlurWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    return BlurWidget;
}

UVerticalBox* UUIStylingHelper::CreateStyledContainer(UWidget* ParentWidget, const FUIStyleConfig& StyleConfig)
{
    if (!ParentWidget)
    {
        return nullptr;
    }

    // Create main container
    UVerticalBox* Container = NewObject<UVerticalBox>(ParentWidget);

    // Create background
    UImage* Background = CreateStyledBackground(Container, StyleConfig, nullptr);
    if (Background)
    {
        Container->AddChild(Background);
    }

    // Create border
    UBorder* Border = CreateStyledBorder(Container, StyleConfig);
    if (Border)
    {
        Container->AddChild(Border);
    }

    // Apply blur if configured
    if (StyleConfig.BlurStrength > 0.0f)
    {
        UBackgroundBlur* Blur = ApplyBackgroundBlur(Container, StyleConfig.BlurStrength);
        if (Blur)
        {
            Container->AddChild(Blur);
        }
    }

    return Container;
}

FUIStyleConfig UUIStylingHelper::GetDefaultStyleConfig()
{
    return FUIStyleConfig();
}