/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemImportFactory.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Editor.h"
#include "RenderingThread.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IMainFrameModule.h"

#include "GolaemImportOptions.h"

//#include "GolaemLibraryModule.h"
//#include "GolaemImporter.h"
#include "GolaemImportLogger.h"
#include "GolaemImportSettings.h"

//#include "GlmAssetImportData.h"

//#include "GeometryCache.h"

#define LOCTEXT_NAMESPACE "GolaemImportFactory"

DEFINE_LOG_CATEGORY_STATIC(LogGolaem, Log, All);

//-------------------------------------------------------------------------
UGolaemImportFactory::UGolaemImportFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bCreateNew = false;
    bEditAfterNew = true;
    SupportedClass = nullptr;

    bEditorImport = true;
    bText = false;

	bShowOption = true;

    Formats.Add(TEXT("gcha;Golaem Character"));
}

//-------------------------------------------------------------------------
void UGolaemImportFactory::PostInitProperties()
{
    Super::PostInitProperties();
    ImportSettings = UGolaemImportSettings::Get();
}

//-------------------------------------------------------------------------
FText UGolaemImportFactory::GetDisplayName() const
{
    return LOCTEXT("GolaemImportFactoryDescription", "Golaem");
}

//-------------------------------------------------------------------------
bool UGolaemImportFactory::DoesSupportClass(UClass* Class)
{
    return Class == UGolaemSkeletalMesh::StaticClass();
}

//-------------------------------------------------------------------------
UClass* UGolaemImportFactory::ResolveSupportedClass()
{
    return UGolaemSkeletalMesh::StaticClass();
}

//-------------------------------------------------------------------------
UObject* UGolaemImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
    GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, TEXT("GCHA"));

    FGolaemImporter Importer;
    EGolaemImportError ErrorCode = Importer.OpenGolaemFileForImport(Filename);
    ImportSettings->bReimport = false;

    if (ErrorCode != GolaemImportError_NoError)
    {
        // Failed to read the file info, fail the import
        GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
        return nullptr;
    }

    // Reset (possible) changed frame start value
    //ImportSettings->SamplingSettings.FrameStart = 0;
    //ImportSettings->SamplingSettings.FrameEnd = Importer.GetEndFrameIndex();

    bOutOperationCanceled = false;

    if (bShowOption)
    {
        TSharedPtr<SGolaemImportOptions> Options;
        
        ShowImportOptionsWindow(Options, UFactory::CurrentFilename, Importer);
        ScaleVariable = Options->GetScaleValue();
		bImportBlendshapes = Options->IsBSChecked();
        MaterialImportLocation = Options->getMaterialImportLocation();
		//ImportAsStaticMesh = Options->GetImportStaticMeshValue();
        // Set whether or not the user canceled
        bOutOperationCanceled = !Options->ShouldImport();
    }

    const FString PageName = "Importing " + InName.ToString() + ".gcha";

    TArray<UObject*> ResultAssets;
    if (!bOutOperationCanceled)
    {
        GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, TEXT("GCHA"));
        int32 NumThreads = 1;
        if (FPlatformProcess::SupportsMultithreading())
        {
            NumThreads = FPlatformMisc::NumberOfCores();
        }

		//if (ImportAsStaticMesh)
		//{
		//	UObject* StaticMeshes = ImportStaticMeshes(Importer, InParent, Flags);
		//	if (StaticMeshes)
		//	{
		//		ResultAssets.Add(StaticMeshes);
		//	}
		//}
		//else
        {
            UObject* SkeletalMesh = ImportSkeletalMesh(Importer, InParent, Flags);
            if (SkeletalMesh)
            {
                ResultAssets.Add(SkeletalMesh);
            }
        }

        for (UObject* Object : ResultAssets)
        {
            if (Object)
            {
                GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, Object);
                Object->MarkPackageDirty();
                Object->PostEditChange();
            }
        }

        if (ResultAssets.Num() == 0)
        {
            FGolaemImportLogger::AddImportMessage(EMessageSeverity::Error, "Import of " + InName.ToString() + ".gcha FAILED (empty asset or see previous errors)");
        }
        else
        {
            FGolaemImportLogger::AddImportMessage(EMessageSeverity::Info, "Successfully imported " + InName.ToString() + ".gcha");
        }
        
        FGolaemImportLogger::OutputMessages(PageName);
    }

    // Determine out parent according to the generated assets outer
    UObject* OutParent = (ResultAssets.Num() > 0 && InParent != ResultAssets[0]->GetOutermost()) ? ResultAssets[0]->GetOutermost() : InParent;
    return (ResultAssets.Num() > 0) ? OutParent : nullptr;
}

//-----------------------------------------------------------------------------
UObject* UGolaemImportFactory::ImportSkeletalMesh(FGolaemImporter& Importer, UObject* InParent, EObjectFlags Flags)
{
    // Flush commands before importing
    FlushRenderingCommands();

    const uint32 NumMeshes = Importer.GetNumMeshTracks();
	// Check if the gcha file contained any meshes
    if (NumMeshes > 0)
    {
		TArray<UObject*> GeneratedObjects = Importer.ImportAsSkeletalMesh(InParent, ScaleVariable, bImportBlendshapes, MaterialImportLocation, Flags);

        if (!GeneratedObjects.Num())
        {
            return nullptr;
        }

        UGolaemSkeletalMesh* SkeletalMesh = [&GeneratedObjects]() {
            UObject** FoundObject = GeneratedObjects.FindByPredicate([](UObject* Object) { return Object->IsA<UGolaemSkeletalMesh>(); });
            return FoundObject ? CastChecked<UGolaemSkeletalMesh>(*FoundObject) : nullptr;
        }();

        return SkeletalMesh;
    }
    else
    {
        // Not able to import a static mesh
        GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
        return nullptr;
    }
}

//-----------------------------------------------------------------------------
bool UGolaemImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
    UAssetImportData* ImportData = nullptr;
    if (Obj->GetClass() == UGolaemSkeletalMesh::StaticClass())
    {
        UGolaemSkeletalMesh* Cache = Cast<UGolaemSkeletalMesh>(Obj);
        ImportData = Cache->AssetImportData;
    }

    if (ImportData)
    {
        if (FPaths::GetExtension(ImportData->GetFirstFilename()) == TEXT("gcha"))
        {
            ImportData->ExtractFilenames(OutFilenames);
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
void UGolaemImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{

    UGolaemSkeletalMesh* SkeletalMesh = Cast<UGolaemSkeletalMesh>(Obj);
    if (SkeletalMesh && SkeletalMesh->AssetImportData && ensure(NewReimportPaths.Num() == 1))
    {
        SkeletalMesh->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
    }
}

//-----------------------------------------------------------------------------
EReimportResult::Type UGolaemImportFactory::Reimport(UObject* Obj)
{
    ImportSettings->bReimport = true;
    const FString PageName = "Reimporting " + Obj->GetName() + ".gcha";

    if (Obj->GetClass() == UGolaemSkeletalMesh::StaticClass())
    {
        UGolaemSkeletalMesh* SkeletalMesh = Cast<UGolaemSkeletalMesh>(Obj);
        if (!SkeletalMesh)
        {
            return EReimportResult::Failed;
        }

        CurrentFilename = SkeletalMesh->AssetImportData->GetFirstFilename();

        EReimportResult::Type Result = ReimportSkeletalMesh(SkeletalMesh);

        if (SkeletalMesh->GetOuter())
        {
            SkeletalMesh->GetOuter()->MarkPackageDirty();
        }
        else
        {
            SkeletalMesh->MarkPackageDirty();
        }

        // Close possible open editors using this asset
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        AssetEditorSubsystem->CloseAllEditorsForAsset(SkeletalMesh);

        FGolaemImportLogger::AddImportMessage(EMessageSeverity::Info, "Successfully reimported " + Obj->GetName() + ".gcha");
        FGolaemImportLogger::OutputMessages(PageName);
        return Result;
    }

    return EReimportResult::Failed;
}

//-----------------------------------------------------------------------------
void UGolaemImportFactory::ShowImportOptionsWindow(TSharedPtr<SGolaemImportOptions>& Options, FString FilePath, const FGolaemImporter& Importer)
{

    TSharedRef<SWindow> Window =
        SNew(SWindow)
            .Title(LOCTEXT("WindowTitle", "Golaem Character Import Options"))
            .SizingRule(ESizingRule::Autosized);

    Window->SetContent(
        SAssignNew(Options, SGolaemImportOptions).WidgetWindow(Window).ImportSettings(ImportSettings).WidgetWindow(Window)
            //.PolyMeshes(Importer.GetPolyMeshes())
            .FullPath(FText::FromString(FilePath)));


    TSharedPtr<SWindow> ParentWindow;

    if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
    {
        IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
        ParentWindow = MainFrame.GetParentWindow();
    }

    FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
}

//-----------------------------------------------------------------------------
EReimportResult::Type UGolaemImportFactory::ReimportSkeletalMesh(UGolaemSkeletalMesh* SkeletalMesh)
{
    // Ensure that the file provided by the path exists
    if (IFileManager::Get().FileSize(*CurrentFilename) == INDEX_NONE)
    {
        return EReimportResult::Failed;
    }

    FGolaemImporter Importer;
    EGolaemImportError ErrorCode = Importer.OpenGolaemFileForImport(CurrentFilename);

    if (ErrorCode != GolaemImportError_NoError)
    {
        // Failed to read the file info, fail the re importing process
        return EReimportResult::Failed;
    }

    if (bShowOption)
    {
        TSharedPtr<SGolaemImportOptions> Options;
        ShowImportOptionsWindow(Options, CurrentFilename, Importer);

        if (!Options->ShouldImport())
        {
            return EReimportResult::Cancelled;
        }
    }

    int32 NumThreads = 1;
    if (FPlatformProcess::SupportsMultithreading())
    {
        NumThreads = FPlatformMisc::NumberOfCores();
    }

	TArray<UObject*> ReimportedObjects = Importer.ReimportAsSkeletalMesh(SkeletalMesh, ScaleVariable, bImportBlendshapes, MaterialImportLocation);
    UGolaemSkeletalMesh* NewSkeletalMesh = [&ReimportedObjects]() {
        UObject** FoundObject = ReimportedObjects.FindByPredicate([](UObject* Object) { return Object->IsA<UGolaemSkeletalMesh>(); });
        return FoundObject ? CastChecked<UGolaemSkeletalMesh>(*FoundObject) : nullptr;
    }();

    if (!NewSkeletalMesh)
    {
        return EReimportResult::Failed;
    }

    return EReimportResult::Succeeded;
}

//-----------------------------------------------------------------------------
int32 UGolaemImportFactory::GetPriority() const
{
    return ImportPriority;
}

#undef LOCTEXT_NAMESPACE
