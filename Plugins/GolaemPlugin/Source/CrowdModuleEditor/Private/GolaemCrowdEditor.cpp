/***************************************************************************
 *                                                                          *
 *  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
 *                                                                          *
 ***************************************************************************/

#include "GolaemCrowdEditor.h"
#include "ISequencerModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "DesktopPlatformModule.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditor.h"
#include "Styling/SlateStyleRegistry.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Kismet/GameplayStatics.h"
#include "AssetToolsModule.h"
#include "Engine/Selection.h"
#include "Editor.h"
// #include "Editor/SceneOutliner/public/SceneOutliner.h"
#include "SSceneOutliner.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
// #include "SceneOutlinerVisitorTypes.h"
#include "ActorTreeItem.h"
#include "FolderTreeItem.h"

#include "SSceneOutliner.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "ActorTreeItem.h"
#include "FolderTreeItem.h"

#include "GolaemCharacter.h"
#include "GolaemCharacterInstanced.h"

#include "GolaemTrackEditor.h"
#include "GolaemUICustomization.h"
#include "GolaemDetailsCustomization.h"
#include "GolaemStyle.h"
#include "GolaemUELogger.h"
#include "GolaemCache.h"
#include "GolaemSimulation.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "glmCharOperators.h"
#include "glmStringOperators.h"
#include "glmLog.h"
#include "glmCore.h"
#include "glmUtils.h"
THIRD_PARTY_INCLUDES_END

#define LOCTEXT_NAMESPACE "FGolaemCrowdEditorModule"

GolaemCacheEditor::GolaemCacheEditor()
{
}

// struct FCollapseActorInSceneOutliner : SceneOutliner::IMutableTreeItemVisitor
//{
//     explicit FCollapseActorInSceneOutliner(FString& InPathToCollapse) : PathToCollapse(InPathToCollapse) {}
//
//     virtual void Visit(SceneOutliner::FActorTreeItem& ActorItem) const override
//     {
//         if (AActor* Actor = ActorItem.Actor.Get())
//         {
//             if (Actor->GetPathName() == PathToCollapse)
//             {
//                 ActorItem.Flags.bIsExpanded = false;
//                 ActorItem.OnExpansionChanged();
//             }
//         }
//     }
//     virtual void Visit(SceneOutliner::FFolderTreeItem& FolderItem) const override
//     {
//         if (FolderItem.Path.ToString() == PathToCollapse) // or FolderItem.Path.GetPlainNameString() ?
//         {
//             FolderItem.Flags.bIsExpanded = false;
//             FolderItem.OnExpansionChanged();
//         }
//     }
//
//     FString PathToCollapse;
// };

//-----------------------------------------------------------------------------
void GolaemCacheEditor::Expand(AGolaemCache* /*golaemCache*/, bool /*expand*/)
{
    // FLevelEditorModule& levelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    // TSharedPtr<ILevelEditor> aLevelEditor = levelEditorModule.GetFirstLevelEditor();
    // TSharedPtr<ISceneOutliner> sceneOutliner = aLevelEditor->GetSceneOutliner();
    // STreeView<SceneOutliner::FTreeItemPtr>& sceneOutlinerTree = const_cast<STreeView<SceneOutliner::FTreeItemPtr>&>(sceneOutliner->GetTree());

    // SceneOutliner::FActorTreeItem anActorItem(static_cast<AActor*>(golaemCache)); // did not find any way to get a ptr on the actor item in SceneOutliner / Tree
    // SceneOutliner::FTreeItemPtr testPtr(&anActorItem);
    // const_cast<STreeView<SceneOutliner::FTreeItemPtr>&>(sceneOutlinerTree).SetItemExpansion(testPtr, expand);

    // ToDo : implement expand / collapse here
    // interesting methods /class :
    // ILevelEditor, ISceneOutliner->GetTree, ISceneOutliner->GetChildren (returns SWidgets), SetItemExpansion(FTreeItemPtr, true/false)
    // did not find any way of getting a given item
    // sceneOutlinerTree.GetAllChildren(); // -> SWidget array
    // sceneOutlinerTree.ItemFromWidget(sceneOutlinerTree.GetChildren()->GetChildAt(0));
    // sceneOutlinerTree.PopulateLinearizedItems(;
    //
}

// Editor pivot location sent to golaem cache in editor mode
//-----------------------------------------------------------------------------
FVector GolaemCacheEditor::GetPivotLocation()
{
    return GEditor->GetPivotLocation();
}

//-----------------------------------------------------------------------------
void GolaemCacheEditor::RegisterPreSaveWorld(AGolaemCache* golaemCache)
{
    golaemCache->PreSaveWorldHandle = FEditorDelegates::PreSaveWorldWithContext.AddUObject(golaemCache, &AGolaemCache::OnPreSaveWorld);
}

//-----------------------------------------------------------------------------
void GolaemCacheEditor::UnregisterPreSaveWorld(AGolaemCache* golaemCache)
{
    FEditorDelegates::PreSaveWorldWithContext.Remove(golaemCache->PreSaveWorldHandle);
}

//-----------------------------------------------------------------------------
void GolaemCacheEditor::CreateUniqueAssetName(FString& MaterialPath, FString& MaterialName)
{
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    AssetToolsModule.Get().CreateUniqueAssetName(MaterialPath, TEXT(""), MaterialPath, MaterialName);
}

//-----------------------------------------------------------------------------
void GolaemCacheEditor::AddSelectionCallBack(AGolaemCharacter& character)
{
    USelection::SelectionChangedEvent.AddUObject(&character, &AGolaemCharacter::OnSelectionChanged);
}

// Editor selected actors sent to golaem cache in editor mode
//-----------------------------------------------------------------------------
FString GolaemCacheEditor::GetSelectedEntities(const FString& golaemCacheName)
{
    FString returnValue;
    USelection* selection = GEditor->GetSelectedActors();
    if (selection != NULL)
    {
        TArray<AGolaemCharacter*> selectedCharacterActors;
        selection->GetSelectedObjects<AGolaemCharacter>(selectedCharacterActors);

        // for (int32 iChar = 0; iChar < selectedCharacterActors.Num(); iChar++)
        //{
        //     // only store selection for this current cache
        //     //if (selectedCharacterActors[iChar]->GetParentActor() == this)
        //     if (iChar > 0)
        //         returnValue += ",";
        //     returnValue += FString::FromInt(selectedCharacterActors[iChar]->CrowdEntityId);
        // }
    }
    return returnValue;
}

//-----------------------------------------------------------------------------
FGolaemCrowdEditorModule::FGolaemCrowdEditorModule()
{
}

//-----------------------------------------------------------------------------
#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
void FGolaemCrowdEditorModule::StartupModule()
{
    ////////////////////////////////////////////////////
    // Style
    ////////////////////////////////////////////////////
    FGolaemStyle::Initialize();

    theGolaemLogger::create();

    ////////////////////////////////////////////////////
    // Register
    ////////////////////////////////////////////////////
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
    GolaemTrackEditorBindingHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FGolaemTrackEditor::CreateTrackEditor));

    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.RegisterCustomPropertyTypeLayout("GolaemRefresh", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGolaemCacheRefreshCustomization::MakeInstance));
    PropertyModule.NotifyCustomizationModuleChanged();

    // set the license information in the crowd module
    // FGolaemCrowdModule& golaemModule = FModuleManager::LoadModuleChecked<FGolaemCrowdModule>("CrowdModule");
    // golaemModule.setLicenseStatus(_licenseStatus);

    PropertyModule.RegisterCustomClassLayout(AGolaemCache::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FGolaemDetailsCustomization::MakeInstance));
    PropertyModule.RegisterCustomClassLayout(AGolaemSimulation::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FGolaemDetailsCustomization::MakeInstance));

    _golaemCacheEditor = new GolaemCacheEditor();

    GolaemCacheEditorWrapper* golaemCacheEditorWrapper = AGolaemCache::GetGolaemCacheEditorWrapper();
    golaemCacheEditorWrapper->registerGolaemCacheEditor(_golaemCacheEditor);

    // Register commands
    // FShowCrowdLibraryCommands::Register();
    CommandList = MakeShareable(new FUICommandList);

    {
        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
        CommandList->Append(LevelEditorModule.GetGlobalLevelEditorActions());

        struct Local
        {
            static void AddToolbarCommands(FToolBarBuilder& ToolbarBuilder)
            {
                FUIAction LibAction(FExecuteAction::CreateStatic(&FGolaemCrowdEditorModule::LaunchLibraryWindow));
                ToolbarBuilder.AddToolBarButton(
                    LibAction, NAME_None, FText::FromString(FString("Golaem Library")), FText::FromString(FString("Golaem Library Tool")),
                    FSlateIcon(FGolaemStyle::GetStyleSetName(), "Golaem.Icon40"));
            }

            static void AddGolaemMenuCommands(FMenuBuilder& MenuBuilder)
            {
                MenuBuilder.AddSubMenu(
                    FText::FromString(FString("Golaem")), FText::FromString(FString("Golaem Tools")),
                    FNewMenuDelegate::CreateStatic(&FGolaemCrowdEditorModule::CreateGolaemMenuItems), false, FSlateIcon(FGolaemStyle::GetStyleSetName(), "Golaem.Icon16"));
            }

            static void AddGolaemMenuNoLicenseCommands(FMenuBuilder& MenuBuilder)
            {
                MenuBuilder.AddSubMenu(
                    FText::FromString(FString("Golaem")), FText::FromString(FString("Golaem Tools")),
                    FNewMenuDelegate::CreateStatic(&FGolaemCrowdEditorModule::CreateGolaemMenuItemsNoLicense), false, FSlateIcon(FGolaemStyle::GetStyleSetName(), "Golaem.Icon16"));
            }

            // static void AddGolaemExportMenu(FMenuBuilder& MenuBuilder)
            //{
            //     FUIAction ExportAction(FExecuteAction::CreateStatic(&FGolaemUI::LaunchExportTerrainWindow));
            //     FText ExportDescription = FText::FromString(FString("Export Selection As Golaem Terrains"));
            //     FText ExportTooltip = FText::FromString(FString("Export selected landscapes as a .gtg file"));
            //     MenuBuilder.AddMenuEntry(ExportDescription, ExportTooltip, FSlateIcon(FGolaemStyle::GetStyleSetName(), "Golaem.Icon16"), ExportAction);
            // }
        };

        // FGolaemCrowdModule& golaemModule = FModuleManager::LoadModuleChecked<FGolaemCrowdModule>("CrowdModule");

        TSharedRef<FExtender> MenuExtender(new FExtender());
        MenuExtender->AddMenuExtension(TEXT("LevelEditor"), EExtensionHook::After, CommandList.ToSharedRef(), FMenuExtensionDelegate::CreateStatic(&Local::AddGolaemMenuCommands));
        LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);

        TSharedRef<FExtender> ToolbarExtender(new FExtender());
        ToolbarExtender->AddToolBarExtension(TEXT("Game"), EExtensionHook::After, CommandList.ToSharedRef(), FToolBarExtensionDelegate::CreateStatic(&Local::AddToolbarCommands));
        LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);

        // TSharedRef<FExtender> MenuExportExtender(new FExtender());
        //          MenuExportExtender->AddMenuExtension(TEXT("LevelEditor"), EExtensionHook::After, CommandList.ToSharedRef(), FMenuExtensionDelegate::CreateStatic(&Local::AddGolaemExportMenu));
        //          LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExportExtender);
    }
}

#undef IMAGE_BRUSH

//-----------------------------------------------------------------------------
void FGolaemCrowdEditorModule::ShutdownModule()
{
    ////////////////////////////////////////////////////
    // Unregister
    ////////////////////////////////////////////////////

    GolaemCacheEditorWrapper* golaemCacheEditorWrapper = AGolaemCache::GetGolaemCacheEditorWrapper();
    golaemCacheEditorWrapper->unregisterGolaemCacheEditor();

    delete _golaemCacheEditor;

    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
    ISequencerModule* SequencerModulePtr = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer");
    if (SequencerModulePtr)
        SequencerModulePtr->UnRegisterTrackEditor(GolaemTrackEditorBindingHandle);

    theGolaemLogger::destroy();

    ////////////////////////////////////////////////////
    // Style
    ////////////////////////////////////////////////////
    FGolaemStyle::Shutdown();
}

//-----------------------------------------------------------------------------
void FGolaemCrowdEditorModule::CreateGolaemMenuItems(class FMenuBuilder& MenuBuilder)
{
    // Golaem Library
    FUIAction LibAction(FExecuteAction::CreateStatic(&FGolaemCrowdEditorModule::LaunchLibraryWindow));
    FText LibDescription = FText::FromString(FString("Library Tool"));
    FText LibTooltip = FText::FromString(FString("Displays the Golaem Library Tool"));
    MenuBuilder.AddMenuEntry(LibDescription, LibTooltip, FSlateIcon(FGolaemStyle::GetStyleSetName(), "Golaem.Icon16"), LibAction);

    // Golaem Layout Editor
    FUIAction LayoutEditorAction(FExecuteAction::CreateStatic(&FGolaemCrowdEditorModule::LaunchLayoutEditorWindow));
    FText LayoutEditorDescription = FText::FromString(FString("Golaem Layout Editor"));
    FText LayoutEditorTooltip = FText::FromString(FString("Displays the Golaem Layout Editor"));
    MenuBuilder.AddMenuEntry(LayoutEditorDescription, LayoutEditorTooltip, FSlateIcon(FGolaemStyle::GetStyleSetName(), "Golaem.Icon16"), LayoutEditorAction);

    FUIAction ExportAction(FExecuteAction::CreateStatic(&FGolaemCrowdEditorModule::LaunchExportTerrainWindow));
    FText ExportDescription = FText::FromString(FString("Export Selection As Golaem Terrains"));
    FText ExportTooltip = FText::FromString(FString("Export selected landscapes as a .gtg file. If GolaemCaches are selected, their destination terrain will be set to this gtg file."));
    MenuBuilder.AddMenuEntry(ExportDescription, ExportTooltip, FSlateIcon(FGolaemStyle::GetStyleSetName(), "Golaem.Icon16"), ExportAction);

    FUIAction SimulationAction(FExecuteAction::CreateStatic(&FGolaemCrowdEditorModule::CreateSimulationNode));
    FText SimulationDescription = FText::FromString(FString("Golaem Simulation (GDA) "));
    FText SimulationTooltip = FText::FromString(FString("The Golaem Simulation Node will run a live simulation based on a Golaem gda scene description file."));
    MenuBuilder.AddMenuEntry(SimulationDescription, SimulationTooltip, FSlateIcon(FGolaemStyle::GetStyleSetName(), "GolaemEngine.Icon16"), SimulationAction);

    CreateGolaemMenuItemsNoLicense(MenuBuilder);
}

//-----------------------------------------------------------------------------
void FGolaemCrowdEditorModule::CreateGolaemMenuItemsNoLicense(class FMenuBuilder& MenuBuilder)
{
    // Golaem About
    FUIAction AboutAction(FExecuteAction::CreateStatic(&FGolaemCrowdEditorModule::LaunchAboutWindow));
    FText AboutDescription = FText::FromString(FString("About Golaem"));
    FText AboutTooltip = FText::FromString(FString("Displays the Golaem About"));
    MenuBuilder.AddMenuEntry(AboutDescription, AboutTooltip, FSlateIcon(FGolaemStyle::GetStyleSetName(), "Golaem.Icon16"), AboutAction);
}

//-----------------------------------------------------------------------------
void FGolaemCrowdEditorModule::LaunchLibraryWindow()
{
    GEngine->Exec(NULL, TEXT("py import glm.ui.windowUnrealLauncher as launcher"));
    GEngine->Exec(NULL, TEXT("py sclUI = launcher.SimCacheLibWindowMain()"));
}

//-----------------------------------------------------------------------------
void FGolaemCrowdEditorModule::LaunchLayoutEditorWindow()
{
    TSharedPtr<IPlugin> golaemPlugin = IPluginManager::Get().FindPlugin("GolaemPlugin");
    FString golaemUEDir = FPaths::ConvertRelativePathToFull(golaemPlugin->GetBaseDir());
    GEngine->Exec(NULL, TEXT("py import glm.ui.windowUnrealLauncher as launcher"));
    FString pyCommand = FString("py layoutUI = launcher.LayoutEditorWindowMain(golaemUEDir=\"" + golaemUEDir + "\")");
    GEngine->Exec(NULL, *pyCommand);
}

//-----------------------------------------------------------------------------
void FGolaemCrowdEditorModule::LaunchAboutWindow()
{
    // get license information
    FGolaemCrowdModule* golaemModule(FModuleManager::GetModulePtr<FGolaemCrowdModule>(FName("CrowdModule")));
    if (golaemModule == nullptr)
        return; // should not happen

    FString licenseInfo;
    licenseInfo = "1;" + licenseInfo;

    licenseInfo = licenseInfo.Replace(TEXT("\n"), TEXT(" - "));
    licenseInfo = licenseInfo.Replace(TEXT("\r"), TEXT(""));

    // open window
    FString openCommand = "py abtUI = launcher.AboutWindowMain('" + golaemModule->getPluginVersionName() + "', '" + licenseInfo + "')";
    GEngine->Exec(NULL, TEXT("py import glm.ui.windowUnrealLauncher as launcher"));
    GEngine->Exec(NULL, openCommand.GetCharArray().GetData());
}

//-----------------------------------------------------------------------------
// Not relying on a GolaemCache instance but on selected objects, so code has to be duplicated here for now
void FGolaemCrowdEditorModule::LaunchExportTerrainWindow()
{
    FGolaemDetailsCustomization::ExportSelectedAsTerrain();
}

//-----------------------------------------------------------------------------
// Not relying on a GolaemCache instance but on selected objects, so code has to be duplicated here for now
void FGolaemCrowdEditorModule::CreateSimulationNode()
{
    // open a file open dialog for gda file
    TArray<FString> OpenFilenames;
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    bool bOpened = false;
    if (DesktopPlatform)
    {
        const FString DefaultBrowsePath = FPaths::ProjectLogDir();
        FString ExtensionStr;
        ExtensionStr += TEXT("Golaem Digital Asset|*.gda|");
        bOpened = DesktopPlatform->OpenFileDialog(
            FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
            LOCTEXT("StatsLoadTitle", "Load EQS stats").ToString(),
            DefaultBrowsePath,
            TEXT(""),
            ExtensionStr,
            EFileDialogFlags::None,
            OpenFilenames);
    }

    // create the Golaem Simulation Node
    UWorld* World = GEditor->GetEditorWorldContext().World();
    for (auto& OpenFileName : OpenFilenames)
    {
        AGolaemSimulation* glmSimuNode = World->SpawnActor<AGolaemSimulation>(AGolaemSimulation::StaticClass(), FTransform::Identity);
        glmSimuNode->GDAFile.FilePath = OpenFileName;
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGolaemCrowdEditorModule, CrowdModuleEditor)