/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h" /*Runtime/SlateCore/Public*/
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GlmCrowdModule.h"
#include "Editor.h"
#include "GolaemCache.h"

// -----------------------------------------------------------------------------------
class GolaemCacheEditor : public IGolaemCacheEditor
{
public:
    GolaemCacheEditor();

    virtual FVector GetPivotLocation() override;
    virtual FString GetSelectedEntities(const FString& golaemCacheName) override;
    virtual void RegisterPreSaveWorld(AGolaemCache* golaemCache) override;
    virtual void UnregisterPreSaveWorld(AGolaemCache* golaemCache) override;
    virtual void CreateUniqueAssetName(FString&, FString&) override;
    virtual void AddSelectionCallBack(AGolaemCharacter& character) override;
    virtual void Expand(AGolaemCache* golaemCache, bool expand) override;
};

// -----------------------------------------------------------------------------------
class FGolaemCrowdEditorModule : public IModuleInterface
{
public:
    FGolaemCrowdEditorModule();

    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    FDelegateHandle GolaemTrackEditorBindingHandle;
    TSharedPtr<FSlateStyleSet> StyleSet;

    // UI
    static void CreateGolaemMenuItems(class FMenuBuilder& MenuBuilder);

    static void LaunchLibraryWindow();
    static void LaunchLayoutEditorWindow();
    static void LaunchAboutWindow();

    static void LaunchExportTerrainWindow();

    static void CreateSimulationNode();

protected:
    TSharedPtr<FUICommandList> CommandList;

    GolaemCacheEditor* _golaemCacheEditor;
};