/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemUICustomization.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "Dialogs/Dialogs.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "IPropertyTypeCustomization.h"

#include "GolaemCache.h"

#include "GolaemStyle.h"

#include "EditorStyleSet.h"


#define LOCTEXT_NAMESPACE "GolaemCacheRefreshCustomization"

//static const TArray<EMaterialProperty> DisabledProperties = { MP_Refraction };

class SGolaemRefreshButton : public SButton
{
public:
    SLATE_BEGIN_ARGS(SGolaemRefreshButton)
        : _Text()
        , _Image(FEditorStyle::GetBrush("Default"))
        , _IsFocusable(true)
    {
    }
    SLATE_ARGUMENT(FText, Text)
    SLATE_ARGUMENT(const FSlateBrush*, Image)
    SLATE_EVENT(FSimpleDelegate, OnClickAction)

    /** Sometimes a button should only be mouse-clickable and never keyboard focusable. */
    SLATE_ARGUMENT(bool, IsFocusable)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        OnClickAction = InArgs._OnClickAction;

        SButton::FArguments ButtonArgs = SButton::FArguments()
                                             .ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
                                             .OnClicked(this, &SGolaemRefreshButton::OnClick)
                                             .ToolTipText(InArgs._Text)
                                             .ContentPadding(4.0f)
                                             .ForegroundColor(FSlateColor::UseForeground())
                                             .IsFocusable(InArgs._IsFocusable)
                                                 [SNew(SImage)
                                                      .Image(InArgs._Image)
                                                      .ColorAndOpacity(FSlateColor::UseForeground())];

        SButton::Construct(ButtonArgs);
    }

private:
    FReply OnClick()
    {
        OnClickAction.ExecuteIfBound();
        return FReply::Handled();
    }

private:
    FSimpleDelegate OnClickAction;
};

TSharedRef<IPropertyTypeCustomization> FGolaemCacheRefreshCustomization::MakeInstance()
{
    return MakeShareable(new FGolaemCacheRefreshCustomization);
}

FGolaemCacheRefreshCustomization::FGolaemCacheRefreshCustomization()
    : GolaemCache()
{
    //	PropertyRestriction = MakeShareable(new FPropertyRestriction(FText::FromString("Property already set on for a different entry")));
    //	CurrentOptions = nullptr;
}

void FGolaemCacheRefreshCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    StructPropertyHandle = InStructPropertyHandle;

    //IPropertyHandle* tempProp = StructPropertyHandle.Get();
    TArray<UObject*> propertyOuters;
    StructPropertyHandle.Get()->GetOuterObjects(propertyOuters);
    GolaemCache = NULL;
    int iOuter = 0;
    while (iOuter < propertyOuters.Num() && GolaemCache == NULL)
    {
        GolaemCache = Cast<AGolaemCache>(propertyOuters[iOuter]);
        iOuter++;
    }

    FText TooltipNameOverride = LOCTEXT("Reload Cache / Layout From Disk", "Reload Cache / Layout From Disk");

    HeaderRow
        .NameContent()
            [StructPropertyHandle->CreatePropertyNameWidget(TooltipNameOverride)]
        .ValueContent()
        .MinDesiredWidth(250.0f)
        .MaxDesiredWidth(0.0f)
            [SNew(SHorizontalBox) + SHorizontalBox::Slot()
                                        .AutoWidth()
                                            [SNew(SGolaemRefreshButton)
                                                 .Text(LOCTEXT("RefreshCache", "RefreshCache"))
                                                 .ToolTipText(LOCTEXT("RefreshCacheToolTipText", "Refresh the Golaem Cache and Layout, wiping and rebuilding everything"))
                                                 .Image(FGolaemStyle::GetBrush("Golaem.Editors.RefreshIcon"))
                                                 .OnClickAction(FSimpleDelegate::CreateSP(this, &FGolaemCacheRefreshCustomization::onRefreshSelected))
                                                 .IsEnabled(true)
                                                 .IsFocusable(false)]];
}

void FGolaemCacheRefreshCustomization::onRefreshSelected()
{
    FText ConfirmReloadLayout = LOCTEXT("ConfirmReloadLayout", "You are reloading the layout from disk, this will void any unsaved layout editions - are you sure?");

    FSuppressableWarningDialog::FSetupInfo Info(ConfirmReloadLayout, LOCTEXT("ReloadLayout", "Reload Layout"), "ReloadLayout_Warning");
    Info.ConfirmText = LOCTEXT("ReloadLayout_Yes", "Yes");
    Info.CancelText = LOCTEXT("ReloadLayout_No", "No");

    FSuppressableWarningDialog ReloadLayout(Info);
    if (ReloadLayout.ShowModal() == FSuppressableWarningDialog::Cancel)
    {
        return;
    }

    if (GolaemCache.IsValid())
        GolaemCache->RefreshCache();
}
