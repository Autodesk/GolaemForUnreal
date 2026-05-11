// Copyright Autodesk, Inc. All rights reserved.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Modules/ModuleInterface.h"
#include "Components.h"
#include "MeshBuild.h"

#include "IMeshMergeUtilities.h"
#include "SkeletalRenderPublic.h"

class UMeshComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
struct FFlattenMaterial;
struct FRawMesh;
struct FRawSkinWeight;
struct FStaticMeshLODResources;
class FSkeletalMeshLODModel;
class FSourceMeshDataForDerivedDataTask;

typedef FIntPoint FMeshIdAndLOD;
struct FFlattenMaterial;
struct FReferenceSkeleton;
struct FStaticMeshLODResources;
class UMeshComponent;
class UStaticMesh;
struct FBoneVertInfo;



 class GolaemMeshUtilities
{
public:
    static bool isSkinnedMeshSectionRigid(
        USkinnedMeshComponent* InSkinnedMeshComponent,
        int32 SpecificSectionIndex, 
        int32& OutRigidBoneIndex, 
        bool stopIfNotRigid);

    static UStaticMesh* ConvertMeshSectionToStaticMesh(
        const TArray<UMeshComponent*>& InMeshComponents, 
        const FTransform& InRootTransform,
        const FString& InPackageName,
        int32 SpecificSectionIndex,
        USkeletalMesh* InSkeletalMesh,
        int32 OutRigidBoneIndex,
        TArray<FSkeletalMeshLODInfo>& LODInfos,
        float CharacterRadius,
        UObject* parent);
};
//
//class GolaemMeshUtilities
//{
//public:
//    static bool isSkinnedMeshSectionRigid(USkinnedMeshComponent* InSkinnedMeshComponent, int32 SpecificSectionIndex, int32& OutRigidBoneIndex, bool stopIfNotRigid);
//
//    //static void SkinnedMeshSectionToRawMeshes(USkinnedMeshComponent* InSkinnedMeshComponent, int32 SpecificSectionIndex, int32 InOverallMaxLODs, const FMatrix& InComponentToWorld, const FString& InPackageName, TArray<FRawMeshTracker>& OutRawMeshTrackers, TArray<FRawMesh>& OutRawMeshes, TArray<UMaterialInterface*>& OutMaterials);
//    static UStaticMesh* ConvertMeshSectionToStaticMesh(const TArray<UMeshComponent*>& InMeshComponents, int32 SpecificSectionIndex, const FTransform& InRootTransform, const FString& InPackageName);
//};
