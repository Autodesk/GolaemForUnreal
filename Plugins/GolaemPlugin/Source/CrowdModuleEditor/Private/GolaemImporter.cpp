/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

//#include "GolaemImporterModulePCH.h"
#include "GolaemImporter.h"

//#if PLATFORM_WINDOWS
//#include "Windows/WindowsHWrapper.h"
//#endif

//#include "AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <glmCrowdIO.h>
#include <glmGolaemCharacter.h>
#include <glmMeshAsset.h>
#include <glmMatrix4.h>
#include <glmList.h>
#include <glmFileName.h>
#include <glmAssetManagementUtils.h>
#include <glmUtils.h>
#include <glmStringOperators.h>
THIRD_PARTY_INCLUDES_END
//#include "HideWindowsPlatformTypes.h"

#include "RenderUtils.h" // 5.2
#include "Runtime/Engine/Classes/Engine/ObjectLibrary.h"
#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Developer/MeshUtilities/Public/MeshUtilities.h"
#include "EditorFramework/AssetImportData.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Subsystems/AssetEditorSubsystem.h"
//#include "Runtime/MeshDescription/Public/MeshDescription.h"
//#include "Runtime/MeshDescription/Public/MeshElementRemappings.h"
#include "StaticMeshDescription.h"
#include "MeshElementRemappings.h"

#include "UObject/UObjectHash.h"
#include "PackageTools.h"
#include "ObjectTools.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "SkeletalMeshTypes.h"
#include "Animation/Skeleton.h"
#include "Rendering/SkeletalMeshModel.h"

#include "Animation/MorphTarget.h"

#include "RenderMath.h"
#include "Engine/SkinnedAssetCommon.h"
#include "IMeshBuilderModule.h"
#include "BoneWeights.h"

//#if WITH_EDITOR
//#include "FbxImporter.h"
//#endif

#include "GolaemImportUtilities.h"
#include "GolaemMeshSample.h"
#include "GolaemSkeletalMesh.h"
#include "GolaemUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"

//-----------------------------------------------------------------------------
FGolaemImporter::FGolaemImporter()
    : ImportSettings(nullptr)
{
    memset(&GeometryFileGCG, 0, sizeof(glm::crowdio::GlmGeometryFile));
}

//-----------------------------------------------------------------------------
FGolaemImporter::~FGolaemImporter()
{
    glm::crowdio::glmClearGeometryFile(GeometryFileGCG);
}

//-----------------------------------------------------------------------------
const EGolaemImportError FGolaemImporter::OpenGolaemFileForImport(const FString InFilePath)
{
    glm::crowdio::glmClearGeometryFile(GeometryFileGCG);

    FString resolvedFilePath = InFilePath;
    GolaemUtils::dirmapString(resolvedFilePath);

    if (GolaemCharacterData.loadCharacterFile(TCHAR_TO_ANSI(*resolvedFilePath)) != glm::LoadCharacterFileErrorCode::SUCCESS)
    {
        return GolaemImportError_InvalidArchive;
    }

    glm::GlmString geometryFileName;
    // example in crowd io : character->getGeometryFile(assetFile, contextGlobalData->_geometryTag, glm::max(0, geoBeInfo._idGeometryFileIdx));
    short geometryTag = 0;
    size_t idGeometryFileIdx = 0;
    const glm::GeometryAsset* geoAsset = GolaemCharacterData.getGeometryAsset(geometryTag, idGeometryFileIdx);
    if (geoAsset != NULL)
    {
        geometryFileName = geoAsset->_filename;
    }

    LoadedFileName = InFilePath;

    GeometryFileName = geometryFileName.c_str();

    // dirmap GeometryFileName
    GolaemUtils::dirmapString(GeometryFileName);
    geometryFileName = TCHAR_TO_ANSI(*GeometryFileName);

    glm::FileName assetFileName;
    assetFileName.set(geometryFileName);
    glm::GlmString assetFileExtension = assetFileName.extension();
    assetFileExtension.toLowerSelf();
    if (assetFileExtension == "fbx")
    {
        // will load after options screen
        GLM_CROWD_TRACE_WARNING("The Golaem Character Importer only handles GCG geometry files. FBX can be pre-imported to get materials, which will be used by GCG import afterward.");
        return GolaemImportError_InvalidArchive;

        //#if WITH_EDITOR
        //		// import the FBX in Unreal
        //		UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();
        //
        //		// all none default options are set here :
        //		FbxImporter->ImportOptions->bImportMorph = true;
        //		FbxImporter->ImportOptions->bCreatePhysicsAsset = false;
        //		FbxImporter->ImportOptions->bImportAsSkeletalGeometry = true;
        //		FbxImporter->ImportOptions->bImportMeshesInBoneHierarchy = true;
        //		FbxImporter->ImportOptions->bImportMaterials = true;
        //		FbxImporter->ImportOptions->bImportTextures = true;
        //		FbxImporter->ImportOptions->bConvertScene = true;
        //		FString FBXFilename = geometryFileName.c_str();
        //		if (FbxImporter->ImportFromFile(FBXFilename, TEXT("FBX")))
        //		{
        //			return GolaemImportError_NoError;
        //		}
        //		else
        //		{
        //			return GolaemImportError_InvalidArchive;
        //		}
        //
        //		//FbxImporter->ImportFile(geometryFileName.c_str(), false);
        //#endif
    }
    else if (assetFileExtension == "gcg")
    {
        glm::crowdio::GlmGeometryGenerationStatus result = glm::crowdio::glmReadGeometryFile(geometryFileName.c_str(), GeometryFileGCG);
        if (result != glm::crowdio::GIO_SUCCESS)
        {
            GLM_CROWD_TRACE_WARNING("The Character '" << TCHAR_TO_ANSI(*InFilePath) << "' could not open its GCG Asset file "<< geometryFileName.c_str() <<", error was " << glmConvertGeometryGenerationStatus(result));
        }
        return result == glm::crowdio::GIO_SUCCESS ? GolaemImportError_NoError : GolaemImportError_InvalidArchive;
    }

    GLM_CROWD_TRACE_WARNING("The Character '" << TCHAR_TO_ANSI(*InFilePath) << "' has an unknow Geometry File extension for '" << geometryFileName << "'. It should be fbx or gcg.");
    return GolaemImportError_InvalidArchive;
}

//-----------------------------------------------------------------------------
template <typename T>
T* FGolaemImporter::CreateObjectInstance(UObject*& InParent, const FString& ObjectName, const EObjectFlags Flags)
{
    // Parent package to place new mesh
    UPackage* Package = nullptr;
    FString NewPackageName = InParent->GetOutermost()->GetName();

    // Setup package name and create one accordingly
    Package = FindPackage(nullptr, *NewPackageName);
    if (Package == NULL)
    {
        //NewPackageName = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName() + TEXT("/") + ObjectName);
        NewPackageName = PackageTools::SanitizePackageName(InParent->GetOutermost()->GetName());
        Package = CreatePackage(*NewPackageName);
    }

    const FString SanitizedObjectName = ObjectTools::SanitizeObjectName(ObjectName);

    T* ExistingTypedObject = FindObject<T>(Package, *SanitizedObjectName);
    UObject* ExistingObject = FindObject<UObject>(Package, *SanitizedObjectName);

    if (ExistingTypedObject != nullptr)
    {
        ExistingTypedObject->PreEditChange(nullptr);
    }
    else if (ExistingObject != nullptr)
    {
        // Replacing an object.  Here we go!
        // Delete the existing object
        const bool bDeleteSucceeded = ObjectTools::DeleteSingleObject(ExistingObject);

        if (bDeleteSucceeded)
        {
            // Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
            CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

            // Create a package for each mesh
            Package = CreatePackage(*NewPackageName);
            InParent = Package;
        }
        else
        {
            // failed to delete
            return nullptr;
        }
    }

    return NewObject<T>(Package, FName(*SanitizedObjectName), Flags | RF_Public);
}

/**
* Process and fill in the mesh ref skeleton bone hierarchy using the raw binary import data
*
* @param RefSkeleton - [out] reference skeleton hierarchy to update
* @param SkeletalDepth - [out] depth of the reference skeleton hierarchy
* @return true if the operation completed successfully
*/
bool FGolaemImporter::ProcessImportMeshSkeleton(const USkeleton* SkeletonAsset, FReferenceSkeleton& RefSkeleton, int32& SkeletalDepth, FVector3f& RootOffset, float InScale, bool importBlendshapes)
{
    //// information to build the skeleton :
    //GeometryFile._boneCount;
    //GeometryFile._bindPoseWorldOrientationInverse;
    //GeometryFile._bindPoseWorldPositionsInverse;
    //GeometryFile._boneJointOrientations;
    //GeometryFile._boneNames;
    //GeometryFile._boneParenting;
    //GeometryFile._invBindPoseScale;

    //// to set later :
    //GeometryFile._restPoseScale;
    //GeometryFile._restPoseWorldOrientation;
    //GeometryFile._restPoseWorldPosition;

    // Setup skeletal hierarchy + names structure.
    RefSkeleton.Empty();

    FReferenceSkeletonModifier RefSkelModifier(RefSkeleton, SkeletonAsset);

    // Digest bones to the serializable format.
    TArray<FTransform> worldBoneTransforms;
    float(&invRootTranslate)[3] = GeometryFileGCG._bindPoseWorldPositionsInverse[0];

    worldBoneTransforms.SetNumUninitialized(GeometryFileGCG._boneCount);

    FQuat QuatCacheToUE4(-0.7071067811865475f, 0.f, 0.f, 0.7071067811865475f);

    FTransform localTransform;
    //int useAdditionalRootBone = false;
    for (uint32_t b = 0; b < GeometryFileGCG._boneCount; b++)
    {
        FString BoneNameFixed = FSkeletalMeshImportData::FixupBoneName(GeometryFileGCG._boneNames[b]._string);
        BoneNameFixed.TrimStartAndEndInline();
        BoneNameFixed = BoneNameFixed.Replace(TEXT(" "), TEXT("-"));

        const FMeshBoneInfo BoneInfo(FName(*BoneNameFixed, FNAME_Add), GeometryFileGCG._boneNames[b]._string, GeometryFileGCG._boneParenting[b] >= GeometryFileGCG._boneCount ? INDEX_NONE : GeometryFileGCG._boneParenting[b]);

        if (RefSkeleton.FindRawBoneIndex(BoneInfo.Name) != INDEX_NONE)
        {
            return false;
        }
        // ref change
        // scale on inverse matrix = inverse scale on non inverse matrix
        double bindPoseScale = 1. / GeometryFileGCG._invBindPoseScale;
        FVector worldPositionInverse(
            GeometryFileGCG._bindPoseWorldPositionsInverse[b][0] * bindPoseScale,
            GeometryFileGCG._bindPoseWorldPositionsInverse[b][2] * bindPoseScale,
            GeometryFileGCG._bindPoseWorldPositionsInverse[b][1] * bindPoseScale);

        // ref change
        FQuat worldOrientationInverse(
            GeometryFileGCG._bindPoseWorldOrientationInverse[b][0],
            GeometryFileGCG._bindPoseWorldOrientationInverse[b][2],
            GeometryFileGCG._bindPoseWorldOrientationInverse[b][1],
            -GeometryFileGCG._bindPoseWorldOrientationInverse[b][3]);

        FTransform Converter;
        //Converter.SetIdentity(); // useless, transforms are init as identity
        Converter.SetTranslation(worldPositionInverse);
        Converter.SetRotation(worldOrientationInverse);

        worldBoneTransforms[b] = Converter.Inverse();
        worldBoneTransforms[b].SetRotation(worldBoneTransforms[b].GetRotation() * QuatCacheToUE4);

        if (b == 0)
        {
            // add a ref bone to 0,0,0
            float bindPoseScaleRoot = InScale * 1 / GeometryFileGCG._invBindPoseScale;
            //FVector rootTranslation = worldBoneTransforms[b].GetTranslation();
            //static FVector tempDebug(0, 0, 0.98f);
            //worldBoneTransforms[b].SetTranslation(tempDebug);
            worldBoneTransforms[b].SetScale3D(FVector(bindPoseScaleRoot, bindPoseScaleRoot, bindPoseScaleRoot));

            RootOffset = FVector3f(worldBoneTransforms[b].GetTranslation());
            FVector translation = worldBoneTransforms[b].GetTranslation();
            worldBoneTransforms[b].SetTranslation(FVector(0, 0, 0));

            //localTransform = worldBoneTransforms[b];
            //localTransform.SetRotation(QuatCacheToUE4 * localTransform.GetRotation());
            RefSkelModifier.Add(BoneInfo, worldBoneTransforms[b]); // use world as local, we are on root
        }
        else
        {
            FVector3f translation = FVector3f(worldBoneTransforms[b].GetTranslation());
            translation -= RootOffset;
            worldBoneTransforms[b].SetTranslation(FVector3d(translation));
            //BoneTransform.SetScale3D(FVector::OneVector);
            uint16_t parentIndex = GeometryFileGCG._boneParenting[b];
            if (parentIndex == 65535)
            {
                char itoaBuffer[256];
                FString errorMessage = "GCG Import - The Bone ";
                if (GeometryFileGCG._boneNames[b]._string != NULL)
                {
                    errorMessage += GeometryFileGCG._boneNames[b]._string;
                }
                errorMessage += " of index ";
                errorMessage += itoa(b, itoaBuffer, 10);
                errorMessage += " is not the root bone and has no parent, this is not supported.\nThe asset will not be imported.";
                FGolaemImportLogger::AddImportMessage(EMessageSeverity::Error, errorMessage);

                return false;
            }

            // we have world ori and world pos, get local pos & ori from parent and this world :
            // local ori is world ori * parentWorldOriInverse, easy :
            FQuat parentOriInverse = worldBoneTransforms[parentIndex].GetRotation().Inverse();
            FQuat localOri = parentOriInverse * worldBoneTransforms[b].GetRotation();
            FVector localPos = parentOriInverse * (worldBoneTransforms[b].GetTranslation() - worldBoneTransforms[parentIndex].GetTranslation());

            localTransform.SetIdentity();
            localTransform.SetRotation(localOri);
            localTransform.SetTranslation(localPos);
            //localTransform.SetRotation(QuatCacheToUE4 * localTransform.GetRotation());
            RefSkelModifier.Add(BoneInfo, localTransform); // use world as local, we are on root
        }
    }

    // meshes will be scaled at import
    RootOffset *= InScale;

    // Add hierarchy index to each bone and detect max depth.
    SkeletalDepth = 0;

    TArray<int32> SkeletalDepths;
    SkeletalDepths.Empty(GeometryFileGCG._boneCount);
    SkeletalDepths.AddZeroed(GeometryFileGCG._boneCount);
    for (int32 b = 0; b < RefSkeleton.GetRawBoneNum(); b++)
    {
        int32 Parent = RefSkeleton.GetRawParentIndex(b);
        int32 Depth = 1.0f;

        SkeletalDepths[b] = 1.0f;
        if (Parent != INDEX_NONE)
        {
            Depth += SkeletalDepths[Parent];
        }
        if (SkeletalDepth < Depth)
        {
            SkeletalDepth = Depth;
        }
        SkeletalDepths[b] = Depth;
    }

    return true;
}

////-----------------------------------------------------------------------------
//TArray<UObject*> FGolaemImporter::ImportAsStaticMeshes(UObject* InParent, float InScale, bool importBlendshapes, EObjectFlags Flags)
//{
//    TArray<UObject*> GeneratedObjects;
//    // build one SkeletalMesh with all meshes bound to different materialInstances, named against the material + _meshName
//
//    // Create a Skeletal mesh instance
//
//    //const FString& ObjectName = InParent != GetTransientPackage() ? FPaths::GetBaseFilename(InParent->GetName()) : (FPaths::GetBaseFilename(LoadedFileName) + "_" + FGuid::NewGuid().ToString());
//    //const FString SanitizedObjectName = ObjectTools::SanitizeObjectName(ObjectName);
//
//    //UStaticMesh* ExistingStaticMesh = FindObject<UStaticMesh>(InParent, *SanitizedObjectName);
//    //FStaticMeshComponentRecreateRenderStateContext* RecreateExistingRenderStateContext = ExistingStaticMesh ? new FStaticMeshComponentRecreateRenderStateContext(ExistingStaticMesh, false) : nullptr;
//
//    // build a library of existing materials
//    FString pathName = FPaths::GetPath(InParent->GetPathName());
//    FAssetRegistryModule& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
//    IAssetRegistry& assetRegistry = assetRegistryModule.Get();
//    assetRegistry.AddPath(pathName);                // Should not be needed for the next function, but just do it.
//    assetRegistry.ScanPathsSynchronous({pathName}); // we need this to force loading of the assets in this path at/before first request.
//
//    // process mesh assets in order, and duplicate meshes when met (that's what gcg does)
//    for (int iMeshAsset = 0; iMeshAsset < GolaemCharacterData._meshAssets.sizeInt(); ++iMeshAsset)
//    {
//        //FString materialInstanceName = ObjectName;
//        FString materialInstanceName = GolaemCharacterData._meshAssets[iMeshAsset]._name.c_str();
//
//        glm::MeshAsset& currentMeshAsset = GolaemCharacterData._meshAssets[iMeshAsset];
//        for (int iShaderGroup = 0; iShaderGroup < currentMeshAsset._shadingGroups.sizeInt(); iShaderGroup++)
//        {
//            FString shaderGroupdInstanceName = materialInstanceName;
//            shaderGroupdInstanceName += "_";
//            shaderGroupdInstanceName += char(iShaderGroup + 48); // convert iShaderGroup to 0-9, should not go above 10 shading groups for a meshAsset
//            shaderGroupdInstanceName = ObjectTools::SanitizeObjectName(shaderGroupdInstanceName);
//
//            int currentShadingGroupIndex = currentMeshAsset._shadingGroups[iShaderGroup];
//            glm::ShadingGroup& shadingGroup = GolaemCharacterData._shadingGroups[currentMeshAsset._shadingGroups[iShaderGroup]];
//            int currentShadingGroupFirstShaderIndex = 0;
//            if (!shadingGroup._shaderAssets.empty())
//            {
//                currentShadingGroupFirstShaderIndex = shadingGroup._shaderAssets[0];
//            }
//
//            FString baseMaterialName = GolaemCharacterData._shaderAssets[currentShadingGroupFirstShaderIndex]._name.c_str();
//            baseMaterialName = ObjectTools::SanitizeObjectName(baseMaterialName);
//
//            UMaterialInterface* Material = RetrieveMaterial(baseMaterialName, InParent, Flags /*, allMaterialsAssetData /*foundMaterials*/);
//            //NewStaticMesh->Materials.Add(FSkeletalMaterial(Material, true));
//        }
//    }
//
//    // impost static meshes from mesh assets
//    for (int iMeshAsset = 0; iMeshAsset < GolaemCharacterData._meshAssets.sizeInt(); ++iMeshAsset)
//    {
//        FString ObjectName = GolaemCharacterData._meshAssets[iMeshAsset]._name.c_str();
//        UStaticMesh* NewStaticMesh = CreateObjectInstance<UStaticMesh>(InParent, ObjectName, Flags);
//
//        // Only import data if a valid object was created
//        if (NewStaticMesh)
//        {
//            NewStaticMesh->PreEditChange(NULL);
//
//            //FStaticMeshSourceModel* sourceModel = &NewStaticMesh->SourceModels[0];
//            //FRawMesh rawMesh;
//            //sourceModel->RawMeshBulkData->LoadRawMesh(rawMesh);
//
//            //for (int32 i = 0; i < rawMesh.FaceMaterialIndices.Num(); i++) {
//            //	rawMesh.FaceMaterialIndices[i] = i;
//            //}
//
//            //// Creo nuevo modelo
//            //UStaticMesh* modeloNuevo = NewObject<UStaticMesh>();
//            //new(modeloNuevo->SourceModels) FStaticMeshSourceModel();
//            //modeloNuevo->SourceModels[0].RawMeshBulkData->SaveRawMesh(rawMesh);
//
//            //for (int32 i = 0; i < rawMesh.FaceMaterialIndices.Num(); i++) {
//            //	modeloNuevo->Materials.Add(BaseMat);
//            ////}
//
//            //TArray<FText> BuildErrorsNuevo;
//            //modeloNuevo->Build(true, &BuildErrorsNuevo);
//            //modeloNuevo->MarkPackageDirty();
//
//            //NewStaticMesh->ResetLODInfo();
//            //NewStaticMesh->AddLODInfo();
//            //FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[0];
//
//            TArray<int> shaderToUse;
//            const uint32 meshCount = GeometryFileGCG._meshCount;
//
//            //TArray<UMaterialInstance*>* result = new TArray<UMaterialInstance*>();
//
//            bool bBuildSuccess = false;
//            bBuildSuccess = BuildStaticMesh(NewStaticMesh, ObjectName, InScale, Flags);
//
//            if (!bBuildSuccess)
//            {
//                NewStaticMesh->MarkPendingKill();
//                return GeneratedObjects;
//            }
//
//            // File name
//            FAssetImportInfo::FSourceFile sourceFile(LoadedFileName);
//            NewStaticMesh->AssetImportData->SourceData.Insert(sourceFile);
//
//            //NewStaticMesh->CalculateInvRefMatrices();
//            NewStaticMesh->PostEditChange();
//            NewStaticMesh->MarkPackageDirty();
//
//            //Skeleton->SetPreviewMesh(SkeletalMesh);
//            //Skeleton->PostEditChange();
//
//            GeneratedObjects.Add(NewStaticMesh);
//            //GeneratedObjects.Add(Skeleton);
//            ////GeneratedObjects.Add(Sequence);
//
//            UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
//            AssetEditorSubsystem->CloseAllEditorsForAsset(NewStaticMesh);
//        }
//    }
//
//    //if (RecreateExistingRenderStateContext)
//    //{
//    //	delete RecreateExistingRenderStateContext;
//    //}
//
//    return GeneratedObjects;
//}

//-----------------------------------------------------------------------------
TArray<UObject*> FGolaemImporter::ImportAsSkeletalMesh(UObject* InParent, float InScale, bool bImportBlendshapes, 
    EGolaemMaterialImportLocation::Value MaterialImportLocation, EObjectFlags Flags)
{
    TArray<UObject*> GeneratedObjects;
    // build one SkeletalMesh with all meshes bound to different materialInstances, named against the material + _meshName

    // Create a Skeletal mesh instance

    const FString& ObjectName = InParent != GetTransientPackage() ? FPaths::GetBaseFilename(InParent->GetName()) : (FPaths::GetBaseFilename(LoadedFileName) + "_" + FGuid::NewGuid().ToString());
    const FString SanitizedObjectName = ObjectTools::SanitizeObjectName(ObjectName);

    UGolaemSkeletalMesh* ExistingSkeletalMesh = FindObject<UGolaemSkeletalMesh>(InParent, *SanitizedObjectName);
    FSkinnedMeshComponentRecreateRenderStateContext* RecreateExistingRenderStateContext = ExistingSkeletalMesh ? new FSkinnedMeshComponentRecreateRenderStateContext(ExistingSkeletalMesh, false) : nullptr;

    UGolaemSkeletalMesh* SkeletalMesh = CreateObjectInstance<UGolaemSkeletalMesh>(InParent, ObjectName, Flags);

    // Only import data if a valid object was created
    if (SkeletalMesh)
    {
        // Touch pre edit change
        SkeletalMesh->PreEditChange(NULL);

        // File name
        FAssetImportInfo::FSourceFile sourceFile(LoadedFileName);
        SkeletalMesh->AssetImportData->SourceData.Insert(sourceFile);

        // process reference skeleton from import data
        FVector3f RootOffset(0, 0, 0);
        int32 SkeletalDepth = 0;
        if (!ProcessImportMeshSkeleton(SkeletalMesh->Skeleton, SkeletalMesh->RefSkeleton, SkeletalDepth, RootOffset, InScale, bImportBlendshapes))
        {
            SkeletalMesh->ClearFlags(RF_Standalone);
            SkeletalMesh->Rename(NULL, GetTransientPackage());
            return GeneratedObjects;
        }

        // Retrieve the imported resource structure and allocate a new LOD model
        FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
        check(ImportedModel->LODModels.Num() == 0);
        ImportedModel->LODModels.Empty();
        ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());
        //new (ImportedModel->LODModels) FSkeletalMeshLODModel();
        SkeletalMesh->ResetLODInfo();
        SkeletalMesh->AddLODInfo();
        FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[0];

        // Forced to 1
        LODModel.NumTexCoords = 1; // MergedMeshSample->NumUVSets;
        SkeletalMesh->bHasVertexColors = true;
        SkeletalMesh->VertexColorGuid = FGuid::NewGuid();

        /* Bounding box without animation, should take the one configured in gcha */
        FBox boundingBox;
        boundingBox.Init();
        glm::Vector3 halfExtents(1, 1, 1);
        short geometryTag = 0;
        size_t idGeometryFileIdx = 0;
        const glm::GeometryAsset* geoAsset = GolaemCharacterData.getGeometryAsset(geometryTag, idGeometryFileIdx);
        if (geoAsset != NULL)
        {
            halfExtents = geoAsset->_halfExtentsYUp;
        }
        boundingBox.Min.Set(-InScale * halfExtents.x, -InScale * halfExtents.z, -InScale * halfExtents.y);
        boundingBox.Max.Set(InScale * halfExtents.x, InScale * halfExtents.z, InScale * halfExtents.y);
        SkeletalMesh->SetImportedBounds(boundingBox);

        // process one material per Mesh Section
        TArray<int> shaderToUse;
        const uint32 meshCount = GeometryFileGCG._meshCount;

        //TArray<UMaterialInstance*>* result = new TArray<UMaterialInstance*>();

        // build a library of existing materials

        FString pathName = FPaths::GetPath(InParent->GetPathName());
        FAssetRegistryModule& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& assetRegistry = assetRegistryModule.Get();
        assetRegistry.AddPath(pathName);                // Should not be needed for the next function, but just do it.
        assetRegistry.ScanPathsSynchronous({ pathName }); // we need this to force loading of the assets in this path at/before first request.

        switch (MaterialImportLocation)
        {
        case EGolaemMaterialImportLocation::LOCAL:
            FGolaemImportLogger::AddImportMessage(EMessageSeverity::Info, "Searching for Materials in local content directory");
            break;
        case EGolaemMaterialImportLocation::UNDER_PARENT:
            FGolaemImportLogger::AddImportMessage(EMessageSeverity::Info, "Searching for Materials under parent directory");
            break;
        case EGolaemMaterialImportLocation::UNDER_ROOT:
            FGolaemImportLogger::AddImportMessage(EMessageSeverity::Info, "Searching for Material under root directory");
            break;
        case EGolaemMaterialImportLocation::ALL_ASSETS:
            FGolaemImportLogger::AddImportMessage(EMessageSeverity::Info, "Searching for Material in all loaded Assets");
            break;
        case EGolaemMaterialImportLocation::DO_NOT_SEARCH:
            FGolaemImportLogger::AddImportMessage(EMessageSeverity::Info, "Searching for Materials prevented by the material import location mode(see import options).");
            break;
        default:
            FGolaemImportLogger::AddImportMessage(EMessageSeverity::Error, "Searching for Materials prevented by unknown material import location mode(see import options).");
            break;
        }

        // process mesh assets in order, and duplicate meshes when met (that's what gcg does)
        for (int iMeshAsset = 0; iMeshAsset < GolaemCharacterData._meshAssets.sizeInt(); ++iMeshAsset)
        {
            FString materialInstanceName = ObjectName;
            materialInstanceName += GolaemCharacterData._meshAssets[iMeshAsset]._name.c_str();

            glm::MeshAsset& currentMeshAsset = GolaemCharacterData._meshAssets[iMeshAsset];
            for (int iShaderGroup = 0; iShaderGroup < currentMeshAsset._shadingGroups.sizeInt(); iShaderGroup++)
            {
                FString shaderGroupdInstanceName = materialInstanceName;
                shaderGroupdInstanceName += "_";
                shaderGroupdInstanceName += char(iShaderGroup + 48); // convert iShaderGroup to 0-9, should not go above 10 shading groups for a meshAsset
                shaderGroupdInstanceName = ObjectTools::SanitizeObjectName(shaderGroupdInstanceName);

                int currentShadingGroupIndex = currentMeshAsset._shadingGroups[iShaderGroup];
                glm::ShadingGroup& shadingGroup = GolaemCharacterData._shadingGroups[currentMeshAsset._shadingGroups[iShaderGroup]];
                if (shadingGroup._shaderAssets.empty())
                {
                    GLM_CROWD_TRACE_WARNING_LIMIT("The Mesh shading group " << shadingGroup._name.c_str() << " did not have any shader asset assigned ! Skipping it.");
                    continue;
                }

                int currentShadingGroupFirstShaderIndex = shadingGroup._shaderAssets[0];
                if (currentShadingGroupFirstShaderIndex >= GolaemCharacterData._shaderAssets.size())
                {
                    GLM_CROWD_TRACE_WARNING_LIMIT("The Mesh shading group " << shadingGroup._name.c_str() << " is pointing on the shader asset " << currentShadingGroupFirstShaderIndex << " whereas there are only " << GolaemCharacterData._shaderAssets.size() << " shader assets in the character.");
                    continue;
                }

                FString baseMaterialName = GolaemCharacterData._shaderAssets[currentShadingGroupFirstShaderIndex]._name.c_str();
                baseMaterialName = ObjectTools::SanitizeObjectName(baseMaterialName);

                UMaterialInterface* Material = RetrieveMaterial(MaterialImportLocation, baseMaterialName, InParent, Flags /*, allMaterialsAssetData /*foundMaterials*/);
                SkeletalMesh->Materials.Add(FSkeletalMaterial(Material, true));
            }
        }

        bool bBuildSuccess = false;
        bBuildSuccess = BuildSkeletalMesh(LODModel, SkeletalMesh, SkeletalMesh->RefSkeleton, RootOffset, InScale, bImportBlendshapes);

        if (!bBuildSuccess)
        {
            SkeletalMesh->MarkAsGarbage();
            return GeneratedObjects;
        }

        // Create the skeleton object
        FString SkeletonName = FString::Printf(TEXT("%s_Skeleton"), *SkeletalMesh->GetName());
        USkeleton* Skeleton = CreateObjectInstance<USkeleton>(InParent, SkeletonName, Flags);

        // Merge bones to the selected skeleton
        check(Skeleton->MergeAllBonesToBoneTree(SkeletalMesh));
        Skeleton->MarkPackageDirty();
        if (SkeletalMesh->Skeleton != Skeleton)
        {
            SkeletalMesh->Skeleton = Skeleton;
            SkeletalMesh->MarkPackageDirty();
        }

        // one material per section.
        const uint32 numSections = LODModel.Sections.Num();
        for (uint32 sectionIndex = 0; sectionIndex < numSections; ++sectionIndex)
        {
            LODModel.Sections[sectionIndex].MaterialIndex = sectionIndex;
        }

        //	++ObjectIndex;
        //}

        // Set recompute tangent flag on skeletal mesh sections
        for (FSkelMeshSection& Section : LODModel.Sections)
        {
            Section.bRecomputeTangent = true;
        }

        SkeletalMesh->CalculateInvRefMatrices();
        SkeletalMesh->PostEditChange();
        SkeletalMesh->MarkPackageDirty();

        Skeleton->SetPreviewMesh(SkeletalMesh);
        Skeleton->PostEditChange();

        GeneratedObjects.Add(SkeletalMesh);
        GeneratedObjects.Add(Skeleton);
        //GeneratedObjects.Add(Sequence);

        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        AssetEditorSubsystem->CloseAllEditorsForAsset(Skeleton);
        AssetEditorSubsystem->CloseAllEditorsForAsset(SkeletalMesh);
    }

    if (RecreateExistingRenderStateContext)
    {
        delete RecreateExistingRenderStateContext;
    }

    return GeneratedObjects;
}

//-----------------------------------------------------------------------------
TArray<UObject*> FGolaemImporter::ReimportAsSkeletalMesh(UGolaemSkeletalMesh* SkeletalMesh, float InScale, bool ImportBlendshapes, EGolaemMaterialImportLocation::Value MaterialImportLocation)
{
    TArray<UObject*> ReimportedObjects = ImportAsSkeletalMesh(SkeletalMesh->GetOuter(), InScale, ImportBlendshapes, MaterialImportLocation, RF_Public | RF_Standalone);
    return ReimportedObjects;
}

// check  FbxStaticMeshImport.cpp / bool UnFbx::FFbxImporter::BuildStaticMeshFromGeometry(FbxNode* Node, UStaticMesh* StaticMesh, TArray<FFbxMaterial>& MeshMaterials, int32 LODIndex, for reference
// other reference abcImporter.cpp, void FAbcImporter::GenerateMeshDescriptionFromSample(const FAbcMeshSample* Sample, FMeshDescription* MeshDescription, UStaticMesh* StaticMesh)

void FGolaemImporter::GenerateMeshDescriptionFromGlmMesh(glm::PODArray<glm::crowdio::GlmFileMeshTransform*>& glmTransforms, FMeshDescription* MeshDescription, UStaticMesh* StaticMesh)
{
    if (MeshDescription == nullptr)
    {
        return;
    }

    FStaticMeshAttributes Attributes(*MeshDescription);

    TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
    TEdgeAttributesRef<bool> EdgeHardnesses = Attributes.GetEdgeHardnesses();
    TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
    TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
    TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
    TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
    TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
    TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();


    size_t maxUVset = 0;

    for (size_t iMeshTransform = 0; iMeshTransform < glmTransforms.size(); ++iMeshTransform)
    {
        glm::crowdio::GlmFileMesh& glmMesh = GeometryFileGCG._meshes[glmTransforms[iMeshTransform]->_meshIndex];
        maxUVset = glm::max(glmMesh._uvSetCount, maxUVset);

        // first mesh of same name has first material, second etC..
        const FPolygonGroupID PolygonGroupID = MeshDescription->CreatePolygonGroup();
        PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = "Material" + iMeshTransform; ///*ToDo*/ StaticMesh->StaticMaterials[MatIndex].ImportedMaterialSlotName;
    }

    //Speedtree use UVs to store is data
    VertexInstanceUVs.SetNumIndices(static_cast<int32>(maxUVset));

    for (size_t iMeshTransform = 0; iMeshTransform < glmTransforms.size(); ++iMeshTransform)
    {
        glm::crowdio::GlmFileMesh& glmMesh = GeometryFileGCG._meshes[glmTransforms[iMeshTransform]->_meshIndex];

        // position
        for (uint32_t VertexIndex = 0; VertexIndex < glmMesh._vertexCount; ++VertexIndex)
        {
            float(&position)[3] = glmMesh._vertices[VertexIndex]._position;
            
            FVector3f Position(position[0], position[1], position[2]);
            FVertexID VertexID = MeshDescription->CreateVertex();
            VertexPositions[VertexID] = Position;
        }

        for (uint32_t TriangleIndex = 0; TriangleIndex < glmMesh._triangleCount; ++TriangleIndex)
        {
            TArray<FVertexInstanceID> CornerVertexInstanceIDs;
            CornerVertexInstanceIDs.SetNum(3);
            FVertexID CornerVertexIDs[3];
            for (int32 Corner = 0; Corner < 3; ++Corner)
            {
                uint32 IndiceIndex = (TriangleIndex * 3) + Corner;
                uint32 VertexIndex = glmMesh._trianglesVertexIndices[TriangleIndex][Corner]; // Sample->Indices[IndiceIndex];
                const FVertexID VertexID(VertexIndex);
                const FVertexInstanceID VertexInstanceID = MeshDescription->CreateVertexInstance(VertexID);

                uint32_t normalIndex = glmMesh._trianglesNormalIndices[TriangleIndex][Corner];
                uint32_t uvIndex = glmMesh._trianglesNormalIndices[TriangleIndex][Corner];
                // tangents
                                
                FVector TangentZ(glmMesh._normals[normalIndex][0], glmMesh._normals[normalIndex][1], glmMesh._normals[normalIndex][2]);
                FVector TangentX(glmMesh._tangents[normalIndex][0], glmMesh._tangents[normalIndex][1], glmMesh._tangents[normalIndex][2]);
                FVector TangentY = FVector::CrossProduct(TangentZ, TangentX); // Z = X ^ Y, so Y = Z ^ X

                VertexInstanceNormals[VertexInstanceID] = FVector3f(TangentZ);
                VertexInstanceTangents[VertexInstanceID] = FVector3f(TangentX);
                VertexInstanceBinormalSigns[VertexInstanceID] = GetBasisDeterminantSign(TangentX.GetSafeNormal(), TangentY.GetSafeNormal(), TangentZ.GetSafeNormal());
                VertexInstanceColors[VertexInstanceID] = FVector4f(FLinearColor::White);
                for (uint32 UVSetIndex = 0; UVSetIndex < maxUVset; ++UVSetIndex)
                {
                    FVector2f UvValue(0, 0);
                    if (UVSetIndex <= glmMesh._uvSetCount)
                    {
                        UvValue.Set(glmMesh._us[UVSetIndex][uvIndex], glmMesh._vs[UVSetIndex][uvIndex]);
                    }
                    else if (glmMesh._uvSetCount > 0)
                    {
                        UvValue.Set(glmMesh._us[UVSetIndex][0], glmMesh._vs[UVSetIndex][0]);
                    }
                    VertexInstanceUVs.Set(VertexInstanceID, UVSetIndex, UvValue);
                }
                CornerVertexInstanceIDs[Corner] = VertexInstanceID;
                CornerVertexIDs[Corner] = VertexID;
            }

            const FPolygonGroupID PolygonGroupID(iMeshTransform /*Sample->MaterialIndices[TriangleIndex]*/);
            // Insert a polygon into the mesh
            MeshDescription->CreatePolygon(PolygonGroupID, CornerVertexInstanceIDs);
        }
    }
    //Set the edge hardness from the smooth group
    //FMeshDescriptionOperations::ConvertSmoothGroupToHardEdges(Sample->SmoothingGroupIndices, *MeshDescription);
}

class EdgeIndices
{
public:
    EdgeIndices() : _edge1(-1), _edge2(-1), _normal1(-1), _normal2(-1)
    {}

    EdgeIndices(int edge1, int edge2, int normal1, int normal2)
    {
        // order at construction
        if (edge1 < edge2)
        {
            _edge1 = edge1;
            _edge2 = edge2;

            _normal1 = normal1;
            _normal2 = normal2;
        }
        else
        {
            _edge1 = edge2;
            _edge2 = edge1;

            _normal1 = normal2;
            _normal2 = normal1;
        }
    }

    int _edge1;
    int _edge2;
    int _normal1;
    int _normal2;

    bool operator<(const EdgeIndices& other) const
    {
        return (_edge1 < other._edge1 || (_edge1 == other._edge1 && _edge2 < other._edge2));
    }

    bool operator==(const EdgeIndices& other) const
    {
        return (_edge1 == other._edge1 && _edge2 == other._edge2);
    }
};

class EdgeData
{
public:
    int _triangle1;
    int _triangle2;
    bool _smooth;
};

// must only be called with assets with normal indices (not per control point)
void FGolaemImporter::rebuildSmoothingGroups(TArray<uint32>& SmoothingGroupIndices, glm::crowdio::GlmFileMesh& currentGlmMesh)
{
    glm::GlmMap< EdgeIndices, EdgeData > edgeToTriangles; // first two integers are triangle ids, third is 0 if not smoothing, 1 if smoothing

    uint32_t(*normalIndices)[3] = currentGlmMesh._trianglesNormalIndices;
    if (currentGlmMesh._normalMode == glm::crowdio::GLM_NORMAL_PER_POLYGON_VERTEX)
    {
        normalIndices = currentGlmMesh._trianglesVertexIndices;
    }

    for (uint32_t iTriangle = 0; iTriangle < currentGlmMesh._triangleCount; iTriangle++)
    {
        // by default, -1
        SmoothingGroupIndices[iTriangle * 3] = -1;
        SmoothingGroupIndices[iTriangle * 3 + 1] = -1;
        SmoothingGroupIndices[iTriangle * 3 + 2] = -1;

        for (int iEdge = 0; iEdge < 3; iEdge++)
        {
            EdgeIndices edgeIndices;
            if (currentGlmMesh._normalMode == glm::crowdio::GLM_NORMAL_PER_POLYGON_VERTEX)
            {
                edgeIndices = EdgeIndices(
                    currentGlmMesh._trianglesVertexIndices[iTriangle][iEdge],
                    currentGlmMesh._trianglesVertexIndices[iTriangle][(iEdge + 1) % 3],
                    iTriangle * 3 + iEdge,
                    iTriangle * 3 + (iEdge + 1) % 3);
            }
            else
            {
                edgeIndices = EdgeIndices(
                    currentGlmMesh._trianglesVertexIndices[iTriangle][iEdge],
                    currentGlmMesh._trianglesVertexIndices[iTriangle][(iEdge + 1) % 3],
                    currentGlmMesh._trianglesNormalIndices[iTriangle][iEdge],
                    currentGlmMesh._trianglesNormalIndices[iTriangle][(iEdge + 1) % 3]);
            }

            auto edgeIte = edgeToTriangles.find(edgeIndices);
            if (edgeIte == edgeToTriangles.end())
            {
                EdgeData edgeData;
                edgeData._triangle1 = iTriangle;
                edgeData._triangle2 = -1;
                edgeData._smooth = false;
                edgeToTriangles.insert(edgeIndices, edgeData);
            }
            else
            {
                EdgeData& edgeData = edgeIte.getValue();
                edgeData._triangle2 = iTriangle;

                glm::Vector3 otherNormal1(currentGlmMesh._normals[edgeIte.getKey()._normal1]);
                glm::Vector3 otherNormal2(currentGlmMesh._normals[edgeIte.getKey()._normal2]);

                glm::Vector3 normal1(currentGlmMesh._normals[edgeIndices._normal1]);
                glm::Vector3 normal2(currentGlmMesh._normals[edgeIndices._normal2]);

                edgeData._smooth = glm::approxEqual(normal1, otherNormal1, GLM_NUMERICAL_PRECISION) && glm::approxEqual(normal2, otherNormal2, GLM_NUMERICAL_PRECISION);
            }
        }
    }


    glm::GlmList<int> trianglesToExplore;

    int smoothingGroupIndex = 0;
    uint32_t firstUnknownSmoothingGroupTriangle = 0;
    // now we can take first unknown smoothing group
    while (firstUnknownSmoothingGroupTriangle < currentGlmMesh._triangleCount)
    {
        trianglesToExplore.push_back(firstUnknownSmoothingGroupTriangle);

        while (!trianglesToExplore.empty())
        {
            int iTriangle = trianglesToExplore.front();
            trianglesToExplore.pop_front();

            if (SmoothingGroupIndices[iTriangle * 3] != -1)
                continue;

            SmoothingGroupIndices[iTriangle * 3] = smoothingGroupIndex;
            SmoothingGroupIndices[iTriangle * 3 + 1] = smoothingGroupIndex;
            SmoothingGroupIndices[iTriangle * 3 + 2] = smoothingGroupIndex;

            // process triangle edges
            for (int i = 0; i < 3; i++)
            {
                EdgeIndices currentEdge(currentGlmMesh._trianglesVertexIndices[iTriangle][i], currentGlmMesh._trianglesVertexIndices[iTriangle][(i + 1) % 3], currentGlmMesh._trianglesNormalIndices[iTriangle][i], currentGlmMesh._trianglesNormalIndices[iTriangle][(i + 1) % 3]);
                EdgeData& edgeData = edgeToTriangles[currentEdge];
                if (edgeData._smooth)
                {
                    int otherTriangle = edgeData._triangle1;
                    if (edgeData._triangle1 == iTriangle)
                        otherTriangle = edgeData._triangle2;

                    trianglesToExplore.push_back(otherTriangle);
                }
            }
        }

        smoothingGroupIndex++;

        while (firstUnknownSmoothingGroupTriangle < currentGlmMesh._triangleCount && SmoothingGroupIndices[firstUnknownSmoothingGroupTriangle * 3] != -1)
            firstUnknownSmoothingGroupTriangle++;
    }
}

//-----------------------------------------------------------------------------
bool FGolaemImporter::BuildSkeletalMesh(FSkeletalMeshLODModel& LODModel, UGolaemSkeletalMesh* SkeletalMesh, const FReferenceSkeleton& RefSkeleton, FVector3f& RootOffset, float InScale, bool bImportBlendshapes)
{
    // based on current GeometryFile, iterate on meshes and build mesh sections

    // Module manager is not thread safe, so need to prefetch before parallelfor
    IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

    // Create Skeletal mesh LOD sections
    int MeshSectionCount = GeometryFileGCG._transformCount;
    LODModel.Sections.Empty(MeshSectionCount);
    LODModel.NumVertices = 0;
    LODModel.IndexBuffer.Empty();

    TArray<uint32> RawPointIndices;
    TArray<TArray<uint32>> VertexIndexRemap;
    VertexIndexRemap.Empty(MeshSectionCount);

    glm::Array<glm::Array<glm::PODArray<int>>> glmVertexIndexToMultiIndexPerSection;
    glmVertexIndexToMultiIndexPerSection.resize(MeshSectionCount);

    TArray<FVector3f> Vertices;
    TArray<uint32> Indices;
    TArray<uint32> SmoothingGroupIndices;
    TArray<FVector2f> UVs;
    TArray<FVector3f> TangentX;
    TArray<FVector3f> TangentY;
    TArray<FVector3f> Normals;
    
    float meshScale = InScale * (1.f / GeometryFileGCG._invBindPoseScale);

    // Create actual skeletal mesh sections
    for (int32 SectionIndex = 0; SectionIndex < MeshSectionCount; ++SectionIndex)
    {
        //useBone.Empty(RefSkeleton.GetNum());
        //useBone.AddZeroed(RefSkeleton.GetNum());

        glm::crowdio::GlmFileMeshTransform& currentGlmMeshTransform = GeometryFileGCG._transforms[SectionIndex];
        glm::crowdio::GlmFileMesh& currentGlmMesh = GeometryFileGCG._meshes[currentGlmMeshTransform._meshIndex];
        if (currentGlmMesh._vertexCount == 0 || (currentGlmMesh._triangleCount == 0 && currentGlmMesh._polygonCount == 0))
        {
            FString errorMsg = "Imported mesh ";
            errorMsg += currentGlmMesh._meshName._string;
            errorMsg += " has 0 vertex or 0 face.";

            FGolaemImportLogger::AddImportMessage(EMessageSeverity::Error, errorMsg);
            continue;
        }

        glmVertexIndexToMultiIndexPerSection[SectionIndex].resize(currentGlmMesh._vertexCount);

        glm::Matrix4 defaultMeshMatrix;
        memcpy(defaultMeshMatrix._matrix, currentGlmMeshTransform._defaultLocalToWorldMatrix, sizeof(float) * 16);

        // we don't have tangents in GCG, need to compute them, so need to provide vertices, indices, UVs, smoothingGroupIndices :
        Vertices.Empty(currentGlmMesh._vertexCount);
        Vertices.SetNum(currentGlmMesh._vertexCount);
        for (uint32_t iVertex = 0; iVertex < currentGlmMesh._vertexCount; iVertex++)
        {
            glm::Vector3 currentVertex(currentGlmMesh._vertices[iVertex]._position);
            currentVertex = glm::transformPoint(currentVertex, defaultMeshMatrix);
            Vertices[iVertex].Set(currentVertex.x * meshScale, currentVertex.z * meshScale, currentVertex.y * meshScale);
            Vertices[iVertex] -= RootOffset;
            //Vertices[iVertex] = transfoMat.TransformPosition(Vertices[iVertex]);
        }

        int indicesCount = currentGlmMesh._triangleCount * 3;
        Indices.Empty(indicesCount);
        for (uint32_t iTriangle = 0; iTriangle < currentGlmMesh._triangleCount; iTriangle++)
        {
            Indices.Add(currentGlmMesh._trianglesVertexIndices[iTriangle][0]);
            Indices.Add(currentGlmMesh._trianglesVertexIndices[iTriangle][1]);
            Indices.Add(currentGlmMesh._trianglesVertexIndices[iTriangle][2]);
        }

        UVs.Empty(indicesCount);
        UVs.SetNumZeroed(indicesCount);

        if (currentGlmMesh._uvSetCount > 0)
        {
            for (uint32_t iTriangle = 0; iTriangle < currentGlmMesh._triangleCount; iTriangle++)
            {
                for (uint32_t iTriangleVertex = 0; iTriangleVertex < 3; iTriangleVertex++)
                {
                    int uvIndex =
                        currentGlmMesh._uvMode == glm::crowdio::GLM_UV_PER_POLYGON_VERTEX
                        ? iTriangle * 3 + iTriangleVertex
                        : currentGlmMesh._trianglesUVIndices[iTriangle][iTriangleVertex];

                    UVs[iTriangle * 3 + iTriangleVertex].Set(currentGlmMesh._us[0][uvIndex], -currentGlmMesh._vs[0][uvIndex]);
                }
            }
        }

        // if we don't have the tangents, rebuild the smoothing groups as good as we can :/
        if (currentGlmMesh._tangentCount == 0)
        {
            SmoothingGroupIndices.Empty(indicesCount);
            SmoothingGroupIndices.SetNum(indicesCount);
            if (currentGlmMesh._normalMode == glm::crowdio::GLM_NORMAL_PER_CONTROL_POINT)
            {
                // everything smoothed, thus in same smoothing group
                for (int iIndex = 0; iIndex < indicesCount; iIndex++)
                {
                    SmoothingGroupIndices[iIndex] = 1;
                }
            }
            else
            {
                //one smoothing group per triangle face
                //for (uint32_t iTriangle = 0; iTriangle < currentGlmMesh._triangleCount; iTriangle++)
                //{
                //    SmoothingGroupIndices[iTriangle * 3] = iTriangle;
                //    SmoothingGroupIndices[iTriangle * 3 + 1] = iTriangle;
                //    SmoothingGroupIndices[iTriangle * 3 + 2] = iTriangle;
                //}
                rebuildSmoothingGroups(SmoothingGroupIndices, currentGlmMesh);
            }

            uint32 TangentOptions = 0;

            MeshUtilities.CalculateTangents(Vertices, Indices, UVs, SmoothingGroupIndices, TangentOptions, TangentX, TangentY, Normals);
        }
        else
        {
            TangentX.SetNumZeroed(indicesCount);
            TangentY.SetNumZeroed(indicesCount);
            Normals.SetNumZeroed(indicesCount);
        }

        // if we have normals and / or tangents, override computed ones :
        if (currentGlmMesh._normalCount > 0)
        {
            // iterate on all indices
            for (uint32_t iTriangle = 0; iTriangle < currentGlmMesh._triangleCount; iTriangle++)
            {
                for (uint32_t iTriangleVertex = 0; iTriangleVertex < 3; iTriangleVertex++)
                {
                    uint32_t glmNormalIndex;
                    if (currentGlmMesh._normalMode == glm::crowdio::GLM_NORMAL_PER_CONTROL_POINT)
                    {
                        glmNormalIndex = currentGlmMesh._trianglesVertexIndices[iTriangle][iTriangleVertex];
                    }
                    else if (currentGlmMesh._normalMode == glm::crowdio::GLM_NORMAL_PER_POLYGON_VERTEX_INDEXED)
                    {
                        glmNormalIndex = currentGlmMesh._trianglesNormalIndices[iTriangle][iTriangleVertex];
                    }
                    else
                    {
                        glmNormalIndex = iTriangle * 3 + iTriangleVertex;
                    }
                    uint32_t unrealNormalIndex = iTriangle * 3 + iTriangleVertex;
                        
                    glm::Vector3 currentNormal(currentGlmMesh._normals[glmNormalIndex]);
                    currentNormal = glm::transformVector(currentNormal, defaultMeshMatrix);
                    Normals[unrealNormalIndex] = FVector3f(currentNormal.x, currentNormal.z, currentNormal.y);

                    if (currentGlmMesh._tangentCount > 0)
                    {
                        glm::Vector3 currentTangent(currentGlmMesh._tangents[glmNormalIndex]);
                        currentTangent = glm::transformVector(currentTangent, defaultMeshMatrix);
                        TangentX[unrealNormalIndex] = FVector3f(currentTangent.x, currentTangent.z, currentTangent.y);
                        TangentY[unrealNormalIndex] = FVector3f::CrossProduct(Normals[unrealNormalIndex], TangentX[unrealNormalIndex]);
                    }                        
                }
            }
        }

        //const FMeshSection& SourceSection = MeshSections[SectionIndex];
        FSkelMeshSection& TargetSection = *new (LODModel.Sections) FSkelMeshSection();
        TargetSection.MaterialIndex = SectionIndex; // one material per mesh // (uint16)SourceSection.MaterialIndex;
        TargetSection.NumTriangles = currentGlmMesh._triangleCount;
        TargetSection.BaseVertexIndex = LODModel.NumVertices;
        TargetSection.bUse16BitBoneIndex = GeometryFileGCG._boneCount > 255;

        // Separate the section's vertices into rigid and soft vertices.
        TArray<uint32>& ChunkVertexIndexRemap = *new (VertexIndexRemap) TArray<uint32>();
        ChunkVertexIndexRemap.AddUninitialized(currentGlmMesh._triangleCount * 3);

        TMultiMap<uint32, uint32> FinalVertices; // = FinalVerticesAllSections[SectionIndex]; // in section scope !
        TMap<FSoftSkinVertex*, uint32> VertexMapping;

        // Reused soft vertex
        FSoftSkinVertex NewVertex;

        uint32 VertexOffset = 0;
        FColor WhiteColor = FColor::White;

        // Generate Soft Skin vertices (used by the skeletal mesh)
        for (uint32 FaceIndex = 0; FaceIndex < currentGlmMesh._triangleCount; ++FaceIndex)
        {
            const uint32 FaceOffset = FaceIndex * 3;
            const int32 MaterialIndex = SectionIndex /*->MaterialIndices[FaceIndex]*/;

            for (uint32 FaceVertexIndex = 0; FaceVertexIndex < 3; ++FaceVertexIndex)
            {
                const uint32 GlmVertexIndex = currentGlmMesh._trianglesVertexIndices[FaceIndex][FaceVertexIndex];

                // check for all vertex duplicates (due to normal or uv for example)
                TArray<uint32> DuplicateVertexIndices;
                FinalVertices.MultiFind(GlmVertexIndex, DuplicateVertexIndices);

                // Populate vertex data, referential change from GCG/Maya to Unreal
                //
                NewVertex.Position = Vertices[GlmVertexIndex];
                //fileMeshVertex._position[0] * 100.f, fileMeshVertex._position[2] * 100.f, fileMeshVertex._position[1] * 100.f);

                FMemory::Memzero(NewVertex.InfluenceBones);
                FMemory::Memzero(NewVertex.InfluenceWeights);

                uint16 maxBoneWeight = UE::AnimationCore::MaxRawBoneWeight;               

                switch (currentGlmMesh._skinningType)
                {
                case 0: // rigid
                    //useBone[currentGlmMesh._rigidSkinningBoneId] = 1;
                    //useBoneAllMeshes[currentGlmMesh._rigidSkinningBoneId] = 1;
                    NewVertex.InfluenceBones[0] = currentGlmMeshTransform._rigidSkinningBoneId;
                    NewVertex.InfluenceWeights[0] = maxBoneWeight;
                    break;
                default:
                case 1: // linear
                case 2: // dualQ
                case 3: // blend
                    uint32 TotalInfluenceWeight = 0;
                    glm::crowdio::GlmFileMeshVertex& fileMeshVertex = currentGlmMesh._vertices[GlmVertexIndex];
                    uint8_t endIndex = fileMeshVertex._skinVertexInfluenceCount < MAX_TOTAL_INFLUENCES ? fileMeshVertex._skinVertexInfluenceCount : MAX_TOTAL_INFLUENCES;
                    for (int32 InfluenceIndex = 0; InfluenceIndex < endIndex; InfluenceIndex++)
                    {
                        uint16_t boneId = fileMeshVertex._skinInfluenceBoneId[InfluenceIndex];
                        //useBone[boneId] = 1;
                        //useBoneAllMeshes[boneId] = 1;
                        NewVertex.InfluenceBones[InfluenceIndex] = boneId;

                        NewVertex.InfluenceWeights[InfluenceIndex] = static_cast<uint16>(fileMeshVertex._skinInfluenceWeights[InfluenceIndex] * maxBoneWeight);
                        TotalInfluenceWeight += NewVertex.InfluenceWeights[InfluenceIndex];
                    }
                    NewVertex.InfluenceWeights[0] += maxBoneWeight - TotalInfluenceWeight;
                    break;
                }

                NewVertex.TangentX = TangentX[FaceOffset + FaceVertexIndex];
                NewVertex.TangentY = TangentY[FaceOffset + FaceVertexIndex];
                NewVertex.TangentZ = Normals[FaceOffset + FaceVertexIndex];

                NewVertex.UVs[0] = FVector2f::ZeroVector;
                if (currentGlmMesh._uvSetCount > 0)
                {
                    NewVertex.UVs[0] = UVs[FaceOffset + FaceVertexIndex];
                }

                // color is not an input of gcg
                NewVertex.Color = WhiteColor; // SourceSection.Colors[FaceOffset + VertexIndex];

                // Add vertex only if a perfect duplicate does not exist yet, else use the existing index
                int32 FinalVertexIndex = INDEX_NONE;
                if (DuplicateVertexIndices.Num())
                {
                    for (const uint32 DuplicateVertexIndex : DuplicateVertexIndices)
                    {
                        if (GolaemImporterUtilities::AreVerticesEqual(TargetSection.SoftVertices[DuplicateVertexIndex], NewVertex))
                        {
                            // Use the existing vertex
                            FinalVertexIndex = DuplicateVertexIndex;
                            break;
                        }
                    }
                }

                if (FinalVertexIndex == INDEX_NONE)
                {
                    FinalVertexIndex = TargetSection.SoftVertices.Add(NewVertex);
                    FinalVertices.Add(GlmVertexIndex, FinalVertexIndex);
                    glmVertexIndexToMultiIndexPerSection[SectionIndex][GlmVertexIndex].push_back(FinalVertexIndex);

                    LODModel.MaxImportVertex;
                    LODModel.MeshToImportVertexMap;

                    //OutUsedVertexIndicesForMorphs.Add(Index);
                    //OutMorphTargetVertexRemapping.Add(FaceIndex * 3 + VertexIndex);
                }

                RawPointIndices.Add(FinalVertexIndex);
                ChunkVertexIndexRemap[VertexOffset] = TargetSection.BaseVertexIndex + FinalVertexIndex;
                ++VertexOffset;
            }
        }

        LODModel.NumVertices += TargetSection.SoftVertices.Num();
        TargetSection.NumVertices = TargetSection.SoftVertices.Num();

        // ToDo SMA : get really used bones from influences ?

        // Using all skeleton for this LODModel section
        // -> if useBone is uncommented, we propably need to rematch bone index of the softVertex to match this map (else it produces odd results with flickering)
        for (int iBone = 0; iBone < RefSkeleton.GetNum(); iBone++)
        {
            //if (useBone[iBone])
            TargetSection.BoneMap.Add(iBone);
        }

        TargetSection.CalcMaxBoneInfluences();
    } // end for each mesh

    for (int iBone = 0; iBone < RefSkeleton.GetNum(); iBone++)
    {
        //if (useBoneAllMeshes[iBone])
        LODModel.ActiveBoneIndices.Add(iBone);
    }

    // Finish building the sections.
    for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
    {
        FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

        glm::crowdio::GlmFileMeshTransform& currentGlmMeshTransform = GeometryFileGCG._transforms[SectionIndex];
        glm::crowdio::GlmFileMesh& currentGlmMesh = GeometryFileGCG._meshes[currentGlmMeshTransform._meshIndex];

        //const TArray<uint32>& SectionIndices = MeshSections[SectionIndex].Indices;
        Section.BaseIndex = LODModel.IndexBuffer.Num();
        const int32 NumIndices = currentGlmMesh._triangleCount * 3; // SectionIndices.Num();
        const TArray<uint32>& SectionVertexIndexRemap = VertexIndexRemap[SectionIndex];
        for (int32 Index = 0; Index < NumIndices; Index++)
        {
            uint32 VertexIndex = SectionVertexIndexRemap[Index];
            LODModel.IndexBuffer.Add(VertexIndex);
        }
    }

    // Compute the required bones for this model.
    UGolaemSkeletalMesh::CalculateRequiredBones(LODModel, RefSkeleton, NULL);

    if (bImportBlendshapes)
    {
        // put this mode or only one morphTarget will be registered, due to some renderData cache update in RegisterMorphTarget itself

        for (int32 SectionIndex = 0; SectionIndex < MeshSectionCount; ++SectionIndex)
        {
            FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
            glm::crowdio::GlmFileMeshTransform& currentGlmMeshTransform = GeometryFileGCG._transforms[SectionIndex];
            glm::crowdio::GlmFileMesh& currentGlmMesh = GeometryFileGCG._meshes[currentGlmMeshTransform._meshIndex];

            const TArray<uint32>& SectionVertexIndexRemap = VertexIndexRemap[SectionIndex];
            //TMultiMap<uint32, uint32>& FinalVertices = FinalVerticesAllSections[SectionIndex]; // in section scope !

            glm::Matrix4 defaultMeshMatrix;
            memcpy(defaultMeshMatrix._matrix, currentGlmMeshTransform._defaultLocalToWorldMatrix, sizeof(float) * 16);

            // Add morph target data
            for (uint16_t iBS = 0; iBS < currentGlmMesh._blendShapeCount; iBS++)
            {
                FName targetName;
                glm::crowdio::GlmBlendShape& CurrentBlendShape = currentGlmMesh._blendShapes[iBS]; // equivalent of blendshape node, will be mixed here

                for (uint16_t iBSChannel = 0; iBSChannel < CurrentBlendShape._blendShapeChannelCount; iBSChannel++)
                {
                    // /!\ BLENDSHAPE IN BETWEEN ARE NOT HANDLED BY UNREAL AND WILL BE DISPLAYED AS SEPARATE CHANNELS
                    glm::crowdio::GlmBlendShapeChannel& bsChannel = CurrentBlendShape._blendShapeChannels[iBSChannel];
                    for (uint16_t iBSTarget = 0; iBSTarget < bsChannel._blendTargetCount; iBSTarget++)
                    {
                        glm::crowdio::GlmBlendTarget& bsTarget = bsChannel._blendTargets[iBSTarget];
                        FString morphTargetName = bsTarget._targetName._string;
                        FString dummy;
                        morphTargetName.Split(":", NULL, &morphTargetName);
                        UMorphTarget* MorphTarget = NewObject<UMorphTarget>(SkeletalMesh, TCHAR_TO_ANSI(*morphTargetName));
                        TArray<FMorphTargetDelta> MorphDeltas;

                        //MorphDeltas.SetNum(bsTarget._controlPointsCount);

                        for (uint32_t iTargetControlPoint = 0; iTargetControlPoint < bsTarget._controlPointsCount; iTargetControlPoint++)
                        {
                            //FMorphTargetDelta& currentTargetDelta = MorphDeltas[iTargetControlPoint];
                            int glmMeshLocalVertexIndex = bsTarget._controlPointsIndices[iTargetControlPoint];

                            // displacement :
                            float(&originalPosition)[3] = currentGlmMesh._vertices[glmMeshLocalVertexIndex]._position;
                            float(&controlPointPosition)[3] = bsTarget._controlPointsPosition[iTargetControlPoint];

                            glm::Vector3 originalVec(originalPosition[0], originalPosition[1], originalPosition[2]);
                            glm::Vector3 controlPointVec(controlPointPosition[0], controlPointPosition[1], controlPointPosition[2]);
                            //originalVec = glm::transformPoint(originalVec, defaultMeshMatrix);
                            //controlPointVec = glm::transformPoint(controlPointVec, defaultMeshMatrix);

                            glm::Vector3 currentVertexDelta = controlPointVec - originalVec;// (controlPointPosition[0] - originalPosition[0], controlPointPosition[1] - originalPosition[1], controlPointPosition[2] - originalPosition[2]);
                            //currentVertexDelta = glm::transformPoint(currentVertexDelta, defaultMeshMatrix);

                            FMorphTargetDelta currentTargetDelta;
                            currentTargetDelta.PositionDelta.Set(currentVertexDelta.x * meshScale, currentVertexDelta.z * meshScale, currentVertexDelta.y * meshScale);

                            // no modification on normals for now
                            currentTargetDelta.TangentZDelta.Set(0, 0, 0);

                            //apply morphTarget to all vertices which were originally glmMeshLocalVertexIndex :
                            glm::PODArray<int>& duplicateVertexIndices = glmVertexIndexToMultiIndexPerSection[SectionIndex][glmMeshLocalVertexIndex];
                            for (size_t iDup = 0; iDup < duplicateVertexIndices.size(); iDup++)
                            {
                                if (duplicateVertexIndices[iDup] >= 0)
                                {
                                    currentTargetDelta.SourceIdx = duplicateVertexIndices[iDup] + Section.BaseVertexIndex;
                                    MorphDeltas.Add(currentTargetDelta);
                                }
                            }
                        }
                        MorphTarget->PopulateDeltas(MorphDeltas, 0, LODModel.Sections);
                        if (MorphTarget->HasValidData()) // can happen that some morph targets contain no delta
                        {
                            try
                            {
                                SkeletalMesh->RegisterMorphTarget(MorphTarget);
                                MorphTarget->MarkPackageDirty();
                            }
                            catch (const std::exception& e)
                            {
                                FGolaemImportLogger::AddImportMessage(EMessageSeverity::Error, e.what());
                            }
                        }
                        else
                        {
                            FGolaemImportLogger::AddImportMessage(EMessageSeverity::Warning, "Morph Target " + MorphTarget->GetFullName() + " Has been dropped, it contained no valid data.");
                        }

                        // Note : no curve is exported in gcg
                    }
                }
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
UMaterialInterface* FGolaemImporter::FindInAllAssets(const FString& MaterialName) const
{
    TArray<FString> listedAssets = UEditorAssetLibrary::ListAssets("/Game"); // by default recursive and do not include folders
    for (auto& listedAsset : listedAssets)
    {
        if (listedAsset.EndsWith(MaterialName))
        {
            auto contentTag = UEditorAssetLibrary::GetTagValues(listedAsset);
            if (contentTag.Find("MaterialDomain") != nullptr)
            {
                //auto ExistingMaterial = static_cast<UMaterialInterface*>(UEditorAssetLibrary::LoadAsset(listedAsset));
                auto ExistingMaterial = LoadObject<UMaterialInterface>(NULL, *listedAsset);// , nullptr, (uint32)(ELoadFlags::LOAD_FindIfFail | LOAD_PackageForPIE));

                return ExistingMaterial;
            }
        }
    }
    return nullptr;
}

//-----------------------------------------------------------------------------
UMaterialInterface* FGolaemImporter::RetrieveMaterial(EGolaemMaterialImportLocation::Value MaterialImportLocation, const FString& MaterialName, UObject* InParent, EObjectFlags Flags)
{
    // this get a material for golaem.
    // First need to check that the base material exists or create it
    // then build a matInstance per mesh, one for enabled and one for disabled
    //UMaterialInterface* returnedMaterial = nullptr;
    UMaterialInterface* baseMaterial = nullptr;

    FString pathName;

    UMaterialInterface* ExistingMaterial = nullptr;
    switch (MaterialImportLocation)
    {
    case EGolaemMaterialImportLocation::LOCAL:
    {
        pathName = FPaths::GetPath(InParent->GetPathName());
    }
        break;
    case EGolaemMaterialImportLocation::UNDER_PARENT:
    {
        UObject* parent = InParent;
        pathName = FPaths::GetPath(parent->GetPathName());
        // go up one level in the path structure
        int lastSlashIndex = pathName.Len() -1;
        pathName.FindLastChar('/', lastSlashIndex);
        if (lastSlashIndex != INDEX_NONE && lastSlashIndex != 0)
        {
            pathName = pathName.Mid(0, lastSlashIndex);
        }
    }
        break;
    case EGolaemMaterialImportLocation::UNDER_ROOT:
    {
        UObject* parent = InParent;
        while (parent->GetOuter() != nullptr)
            parent = parent->GetOuter();
        pathName = "/Game";// GetPath(parent->GetPathName());
    }
    break;
    case EGolaemMaterialImportLocation::ALL_ASSETS:
    {
        // try to find on already loaded other packages :
        ExistingMaterial = FindInAllAssets(*MaterialName);
        //ExistingMaterial = FindObject<UMaterialInterface>(ANY_PACKAGE, *MaterialName); // -> does not work for assets present in cnotent but not loaded yet
    }
        break;
    case EGolaemMaterialImportLocation::DO_NOT_SEARCH:
    default:
        break;
    }

    if (pathName != "")
    {
        pathName += '/';
        pathName += MaterialName;
        ExistingMaterial = LoadObject<UMaterialInterface>(NULL, *pathName /**MaterialName*/);
    }

    if (!ExistingMaterial)
    {
        FGolaemImportLogger::AddImportMessage(EMessageSeverity::Warning, "Material " + MaterialName + " NOT found, creating a default material node.");
        // In this case recreate the material
        baseMaterial = NewObject<UMaterial>(InParent, *MaterialName);
        baseMaterial->SetFlags(Flags);
        FAssetRegistryModule::AssetCreated(baseMaterial);
    }
    else
    {
        FGolaemImportLogger::AddImportMessage(EMessageSeverity::Info, "Material " + MaterialName + " found.");
        ExistingMaterial->PreEditChange(nullptr);
        baseMaterial = ExistingMaterial;
    }


    return baseMaterial;
}

#undef LOCTEXT_NAMESPACE
