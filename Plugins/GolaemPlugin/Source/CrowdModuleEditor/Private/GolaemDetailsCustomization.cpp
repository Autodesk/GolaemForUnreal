/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemDetailsCustomization.h" //needs to be included first
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "PropertyHandle.h"
#include "Runtime/Landscape/Classes/LandscapeProxy.h"
//#include "Text.h"
#include "DetailWidgetRow.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "GolaemCache.h"
#include "GolaemSimulation.h"
#include "Widgets/Images/SImage.h"
#include "GolaemStyle.h"
#include "EditorStyleSet.h"
#include "Engine/Selection.h"

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

//-----------------------------------------------------------------------------
TSharedRef<IDetailCustomization> FGolaemDetailsCustomization::MakeInstance()
{
    return MakeShareable(new FGolaemDetailsCustomization);
}

//-----------------------------------------------------------------------------
void FGolaemDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);

    for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
    {
        AGolaemCache* GolaemCache = Cast<AGolaemCache>(Object.Get());
        if (GolaemCache != NULL)
        {
            // force Categories orderSimulation Display a third category                    
            IDetailCategoryBuilder& SimulationLayoutCategory3 = DetailBuilder.EditCategory("Common");
            IDetailCategoryBuilder& SimulationLayoutCategory4 = DetailBuilder.EditCategory("Simulation Display");
            IDetailCategoryBuilder& SimulationLayoutCategory5 = DetailBuilder.EditCategory("Time");
            IDetailCategoryBuilder& SimulationLayoutCategory6 = DetailBuilder.EditCategory("Simulation Cache");

            // declare simulation layout before simulation layout terrain for a correct order
            IDetailCategoryBuilder& SimulationLayoutCategory2 = DetailBuilder.EditCategory("Simulation Layout");
            SimulationLayoutCategory2.AddCustomRow(FText::FromString("Open The Layout File relative to this cache"))
                .ValueContent()
                .VAlign(VAlign_Center)
                .MaxDesiredWidth(250)
                    [SNew(SButton)
                         .VAlign(VAlign_Center)
                         .OnClicked(this, &FGolaemDetailsCustomization::OpenLayoutFileButton)
                         .Content()
                             [SNew(STextBlock).Text(FText::FromString("Open Layout File"))]];

            IDetailCategoryBuilder& SimulationLayoutTerrainCategory = DetailBuilder.EditCategory("Simulation Layout Terrain");
            SimulationLayoutTerrainCategory.AddProperty(GET_MEMBER_NAME_CHECKED(AGolaemCache, TerrainAdaptationMode));
            IDetailPropertyRow& terrainSourceProp = SimulationLayoutTerrainCategory.AddProperty(GET_MEMBER_NAME_CHECKED(AGolaemCache, TerrainFileSource));
            terrainSourceProp.IsEnabled(false); // disable source terrain edition, that should come with the library and should not be edited except in library
            SimulationLayoutTerrainCategory.AddProperty(GET_MEMBER_NAME_CHECKED(AGolaemCache, TerrainFileDest));
            //SimulationLayoutTerrainCategory.AddCustomRow(FText::FromString("Export Selected Landscapes and/or Meshes As Destination Terrain"))
            //    .ValueContent()
            //    .VAlign(VAlign_Center)
            //    .MaxDesiredWidth(300)
            //        [SNew(SButton)
            //             .VAlign(VAlign_Center)
            //             .OnClicked(this, &FGolaemDetailsCustomization::ExportDestTerrain)
            //             .Content()
            //                 [SNew(STextBlock).Text(FText::FromString("Export Selected As Destination Terrain"))]];

            FText TooltipNameOverride = FText::FromString("Reload Terrains from Files");

            SimulationLayoutTerrainCategory.AddCustomRow(FText::FromString("Reload Terrains from Files"))
                .ValueContent()
                .MinDesiredWidth(32.0f)
                .MaxDesiredWidth(32.0f)
                [SNew(SHorizontalBox) + SHorizontalBox::Slot()
                .AutoWidth()
                [SNew(SGolaemRefreshButton)
                .Text(FText::FromString("Reload Terrains"))
                .ToolTipText(FText::FromString("Reload source and destination Terrains from the GTG files"))
                .Image(FGolaemStyle::GetBrush("Golaem.Editors.RefreshIcon"))
                //.OnClickAction(FSimpleDelegate::CreateSP(this, &FGolaemDetailsCustomization::RefreshTerrains))
                .IsEnabled(true)
                .IsFocusable(false)]];
            break;
        }
        AGolaemSimulation* GolaemSimulation = Cast<AGolaemSimulation>(Object.Get());
        if (GolaemSimulation != NULL)
        {
            IDetailCategoryBuilder& SimulationCommonCategory = DetailBuilder.EditCategory("Common");
            SimulationCommonCategory.AddCustomRow(FText::FromString("Refresh GDA File"))
                .ValueContent()
                .VAlign(VAlign_Center)
                .MaxDesiredWidth(30)
                    [SNew(SHorizontalBox) + SHorizontalBox::Slot()
                                                .AutoWidth()
                                                    [SNew(SButton)
                                                         .ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
                                                         .VAlign(VAlign_Center)
                                                         .OnClicked(this, &FGolaemDetailsCustomization::RefreshGDAButton)
                                                         .ToolTipText(FText::FromString("Refresh GDA File and Properties"))
                                                         .Content()
                                                             [SNew(SImage)
                                                                  .Image(FGolaemStyle::GetBrush("Golaem.Editors.RefreshIcon"))
                                                                  .ColorAndOpacity(FSlateColor::UseForeground())]]]
                .NameContent()[SNew(STextBlock).Text(FText::FromString("Refresh GDA File"))];
            break;
        }
    }
}

//-----------------------------------------------------------------------------
FReply FGolaemDetailsCustomization::RefreshTerrains()
{
    if (GEngine)
    {
        for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
        {
            AGolaemCache* GolaemCache = Cast<AGolaemCache>(Object.Get());
            if (GolaemCache)
            {
                GolaemCache->RefreshTerrains();
            }
        }
    }
    return FReply::Handled();
}



////-----------------------------------------------------------------------------
//FReply FGolaemDetailsCustomization::ExportDestTerrain()
//{
//    if (GEngine)
//    {
//        for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
//        {
//            AGolaemCache* GolaemCache = Cast<AGolaemCache>(Object.Get());
//            if (GolaemCache)
//            {
//                FString terrainPath = ExportSelectedAsTerrain();
//
//                // update terrain file dest with new desination terrain file
//                if (terrainPath.Len()>0)
//                {
//                    GolaemCache->TerrainFileDest.FilePath = terrainPath;
//                }
//            }
//        }
//    }
//    return FReply::Handled();
//}

//-----------------------------------------------------------------------------
FReply FGolaemDetailsCustomization::OpenLayoutFileButton()
{
    for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
    {
        AGolaemCache* GolaemCache = Cast<AGolaemCache>(Object.Get());
        if (GolaemCache)
        {
            if (GolaemCache->GetLayoutFilesParsed().empty())
            {
                GolaemCache->OpenNewLayoutFile();
            }
            else
            {
                for (int iLayoutFile = 0; iLayoutFile < GolaemCache->GetLayoutFilesParsed().sizeInt(); iLayoutFile++)
                {
                    if (GolaemCache->GetLayoutFilesParsed()[iLayoutFile].empty())
                    {
                        GolaemCache->OpenNewLayoutFile(iLayoutFile);
                    }
                    else
                    {
                        GolaemCache->OpenLayoutFile(iLayoutFile, true);
                    }
                }
            }
        }
    }
    return FReply::Handled();
}

//-----------------------------------------------------------------------------
FReply FGolaemDetailsCustomization::RefreshGDAButton()
{
    for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
    {
        AGolaemSimulation* GolaemSimulation = Cast<AGolaemSimulation>(Object.Get());
        if (GolaemSimulation != NULL)
        {
            GolaemSimulation->RefreshGdaFile();
        }
    }
    return FReply::Handled();
}

//-------------------------------------------------------------------------
void FGolaemDetailsCustomization::ExportSelectedAsTerrain()
{
    if (GEngine)
    {
        USelection* SelectedObjects = GEditor->GetSelectedActors();
        TArray<AActor*> terrainActors;
        TArray<AGolaemCache*> caches;

        for (FSelectionIterator Iter(*SelectedObjects); Iter; ++Iter)
        {
            ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(*Iter);
            if (Landscape)
            {
                terrainActors.Add(Landscape);
            }

            AStaticMeshActor* StaticMesh = Cast<AStaticMeshActor>(*Iter);
            if (StaticMesh)
            {
                terrainActors.Add(StaticMesh);
            }
            
            AGolaemCache* golaemCache = Cast<AGolaemCache>(*Iter);
            if (golaemCache)
            {
                caches.Add(golaemCache);
            }
        }

        if (terrainActors.Num() == 0)
        {
            GLM_CROWD_TRACE_WARNING("There is no Landscape proxy to export as terrain in this scene.");
        }
        else
        {
            FString terrainPath = AGolaemCache::ExportTerrain(terrainActors);

            // update terrain file dest with new desination terrain file
            if (terrainPath.Len() > 0)
            {
                for (int32 i = 0; i < caches.Num(); i++)
                {
                    caches[i]->TerrainFileDest.FilePath = terrainPath;
                }                
            }
        }
    }
}