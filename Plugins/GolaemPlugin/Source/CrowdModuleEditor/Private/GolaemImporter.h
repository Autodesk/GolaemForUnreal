/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Containers/List.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Runtime/Launch/Resources/Version.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <glmCrowdIO.h>
#include <glmGolaemCharacter.h>
#include <glmGeometryFile.h>
THIRD_PARTY_INCLUDES_END

class UMaterial;
class USkeleton;
class USkeletalMesh;
class UGolaemSkeletalMesh;
class UGolaemImportSettings;
class FSkeletalMeshLODModel;
struct FAssetData;

struct FReferenceSkeleton;

class EGolaemMaterialImportLocation
{
public:
    enum Value
    {
        LOCAL,
        UNDER_PARENT,
        UNDER_ROOT,
        ALL_ASSETS,
        DO_NOT_SEARCH,
        END
    };
};

enum EGolaemImportError : uint32
{
    GolaemImportError_NoError,
    GolaemImportError_InvalidArchive,
    GolaemImportError_NoValidTopObject,
    GolaemImportError_NoMeshes,
    GolaemImportError_FailedToImportData
};

class FGolaemImporter
{
public:
    FGolaemImporter();
    ~FGolaemImporter();

public:
    /**
	* Opens and caches basic data from the Golaem file to be used for populating the importer UI
	*
	* @param InFilePath - Path to the Golaem file to be opened
	* @return - Possible error code
	*/
    const EGolaemImportError OpenGolaemFileForImport(const FString InFilePath);

	//TArray<UObject*> ImportAsStaticMeshes(UObject* InParent, float InScale, bool importBlendshapes, EObjectFlags Flags);
	//TArray<UObject*> ReimportAsStaticMesh(UStaticMesh* StaticMesh, float InScale);

    TArray<UObject*> ImportAsSkeletalMesh(UObject* InParent, float InScale, bool importBlendshapes, EGolaemMaterialImportLocation::Value MaterialImportLocation, EObjectFlags Flags);
    TArray<UObject*> ReimportAsSkeletalMesh(UGolaemSkeletalMesh* SkeletalMesh, float InScale, bool ImportBlendshapes, EGolaemMaterialImportLocation::Value MaterialImportLocation);

    uint32_t GetNumMeshTracks()
    {
        return GeometryFileGCG._meshCount;
    }

private:
    /**
	* Creates an template object instance taking into account existing Instances and Objects (on reimporting)
	*
	* @param InParent - ParentObject for the geometry cache, this can be changed when parent is deleted/re-created
	* @param ObjectName - Name to be used for the created object
	* @param Flags - Object creation flags
	*/
    template <typename T>
    T* CreateObjectInstance(UObject*& InParent, const FString& ObjectName, const EObjectFlags Flags);


    /** Retrieves a material according to the given name and resaves it into the parent package*/
    UMaterialInterface* RetrieveMaterial(EGolaemMaterialImportLocation::Value MaterialImportLocation, const FString& MaterialName, UObject* InParent, EObjectFlags Flags/*, TArray<FAssetData>& allMaterialsAssetData*/);

    UMaterialInterface* FindInAllAssets(const FString& MaterialName) const;

    void rebuildSmoothingGroups(TArray<uint32>& SmoothingGroupIndices, glm::crowdio::GlmFileMesh& CurrentGlmMesh);

    /** Build a skeletal mesh from the PCA compressed data */
    bool BuildSkeletalMesh(FSkeletalMeshLODModel& LODModel, UGolaemSkeletalMesh* SkeletalMesh, const FReferenceSkeleton& RefSkeleton, FVector3f& RootOffset, float InScale, bool bImportBlendshapes);
    bool ProcessImportMeshSkeleton(const USkeleton* SkeletonAsset, FReferenceSkeleton& RefSkeleton, int32& SkeletalDepth, FVector3f& RootOffset, float InScale, bool importBlendshapes);

	void GenerateMeshDescriptionFromGlmMesh(glm::PODArray< glm::crowdio::GlmFileMeshTransform*>& glmTransforms, FMeshDescription* MeshDescription, UStaticMesh* StaticMesh);
	//bool BuildStaticMesh(UObject* InParent, const FString& meshAssetName, float InScale, EObjectFlags Flags);
	//bool BuildStaticMesh(UStaticMesh* StaticMesh, FString& meshAssetName, float InScale);

private:
    /** Cached ptr for the import settings */
    UGolaemImportSettings* ImportSettings;

    /** Resulting compressed data from PCA compression */
    //TArray<FCompressedGolaemData> CompressedMeshData;

    /** Golaem file representation for currently opened filed */
    glm::GolaemCharacter GolaemCharacterData;
    glm::crowdio::GlmGeometryFile GeometryFileGCG;
    FString LoadedFileName;
	FString GeometryFileName;
};