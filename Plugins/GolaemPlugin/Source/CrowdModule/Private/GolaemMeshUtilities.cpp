// Copyright Autodesk, Inc. All rights reserved.
#if WITH_EDITOR
#include "GolaemMeshUtilities.h"
#include "Components/SkinnedMeshComponent.h"
#include "RawMesh.h"
#include "RawIndexBuffer.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorDirectories.h"
#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Internationalization/Text.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Delegates/Delegate.h"
#include "Engine/StaticMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkinnedAssetCommon.h"

#include "GolaemCache.h"

#include "glmPODArray.h"  

bool GolaemMeshUtilities::isSkinnedMeshSectionRigid(
     USkinnedMeshComponent* InSkinnedMeshComponent,
     int32 SpecificSectionIndex,
     int32& OutRigidBoneIndex,
     bool stopIfNotRigid)
{
     bool rigidSection = false;

     if (InSkinnedMeshComponent->GetNumLODs() == 0)
         return false;

     const int32 OverallLODIndex = 0;
     FSkeletalMeshLODInfo& SrcLODInfo = *(InSkinnedMeshComponent->SkeletalMesh->GetLODInfo(OverallLODIndex));

     FSkeletalMeshModel* ImportedModel = InSkinnedMeshComponent->SkeletalMesh->GetImportedModel();
     FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[OverallLODIndex];

     // get the first influence of the first soft vertex, and set it as rigid bone index arbitrarily
     // OutRigidBoneIndex = 0;

     // can happen on lower lods that there is lower sections,
     // toDo : still need to match them
     if (SpecificSectionIndex >= LODModel.Sections.Num())
         return false;

     // get skinning for current level LOD
     FSkeletalMeshLODModel& LOD0Model = ImportedModel->LODModels[OverallLODIndex];
     if (LOD0Model.Sections[SpecificSectionIndex].SoftVertices.Num() > 0)
     {
         FSkelMeshSection& section = LOD0Model.Sections[SpecificSectionIndex];

         glm::PODArray<float> totalWeightPerBone;
         totalWeightPerBone.resize(section.BoneMap.Num(), 0.f); // InSkinnedMeshComponent->SkeletalMesh->RefSkeleton.GetNum(), 0.f);

         // get most relevant index to bind to :
         for (auto softVert : section.SoftVertices)
         {
             for (int iInf = 0; iInf < section.MaxBoneInfluences; ++iInf)
             {
                 totalWeightPerBone[softVert.InfluenceBones[iInf]] += softVert.InfluenceWeights[iInf];
             }
         }

         int nonNullBoneWeight = 0;
         for (size_t iBone = 0; iBone < totalWeightPerBone.size(); iBone++)
         {
             if (totalWeightPerBone[iBone] > 0)
             {
                 nonNullBoneWeight++;
             }
         }

         // check if rigid on lod 0 only. if it is good chance that all lods are
         rigidSection = nonNullBoneWeight <= 1;

         if (stopIfNotRigid && !rigidSection)
         {
             return false;
         }

         OutRigidBoneIndex = 0;
         float maxWeight = 0.f;
         for (size_t iBone = 0; iBone < totalWeightPerBone.size(); iBone++)
         {
             if (maxWeight < totalWeightPerBone[iBone])
             {
                 maxWeight = totalWeightPerBone[iBone];
                 OutRigidBoneIndex = iBone;
             }
         }

         // convert OutRigidBoneIndex to section mapped bone :
         if (section.BoneMap.IsValidIndex(OutRigidBoneIndex))
         {
             OutRigidBoneIndex = section.BoneMap[OutRigidBoneIndex];
         }
     }

     return rigidSection;
 }
 
// Helper function for ConvertMeshesToStaticMesh
static bool IsValidSkinnedMeshComponent(USkinnedMeshComponent* InComponent)
{
     return InComponent && InComponent->MeshObject && InComponent->IsVisible();
}

struct FRawMeshTracker
{
     FRawMeshTracker()
         : bValidColors(false)
     {
         FMemory::Memset(bValidTexCoords, 0);
     }

     bool bValidTexCoords[MAX_MESH_TEXTURE_COORDS];
     bool bValidColors;
 };

// From MeshUtilites, adapted to sections - Helper function for ConvertMeshesToStaticMesh
//-------------------------------------------------------------------------
// Helper function for ConvertMeshesToStaticMesh
static void AddOrDuplicateMaterial(UMaterialInterface* InMaterialInterface, const FString& InPackageName, TArray<UMaterialInterface*>& OutMaterials)
{
    if (InMaterialInterface && !InMaterialInterface->GetOuter()->IsA<UPackage>())
    {
        // Convert runtime material instances to new concrete material instances
        // Create new package
        FString OriginalMaterialName = InMaterialInterface->GetName();
        FString MaterialPath = FPackageName::GetLongPackagePath(InPackageName) / OriginalMaterialName;
        FString MaterialName;
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        AssetToolsModule.Get().CreateUniqueAssetName(MaterialPath, TEXT(""), MaterialPath, MaterialName);
        UPackage* MaterialPackage = CreatePackage(*MaterialPath);

        // Duplicate the object into the new package
        UMaterialInterface* NewMaterialInterface = DuplicateObject<UMaterialInterface>(InMaterialInterface, MaterialPackage, *MaterialName);
        NewMaterialInterface->SetFlags(RF_Public | RF_Standalone);

        if (UMaterialInstanceDynamic* MaterialInstanceDynamic = Cast<UMaterialInstanceDynamic>(NewMaterialInterface))
        {
            UMaterialInstanceDynamic* OldMaterialInstanceDynamic = CastChecked<UMaterialInstanceDynamic>(InMaterialInterface);
            MaterialInstanceDynamic->K2_CopyMaterialInstanceParameters(OldMaterialInstanceDynamic);
        }

        NewMaterialInterface->MarkPackageDirty();

        FAssetRegistryModule::AssetCreated(NewMaterialInterface);

        InMaterialInterface = NewMaterialInterface;
    }

    OutMaterials.Add(InMaterialInterface);
}

// Helper function for ConvertMeshesToStaticMesh
template <typename ComponentType>
static void ProcessMaterials(ComponentType* InComponent, const FString& InPackageName, TArray<UMaterialInterface*>& OutMaterials)
{
    const int32 NumMaterials = InComponent->GetNumMaterials();
    for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; MaterialIndex++)
    {
        UMaterialInterface* MaterialInterface = InComponent->GetMaterial(MaterialIndex);
        AddOrDuplicateMaterial(MaterialInterface, InPackageName, OutMaterials);
    }
}

// From MeshUtilites, adapted to sections - Helper function for ConvertMeshesToStaticMesh
//-------------------------------------------------------------------------
void SkinnedMeshSectionToRawMesh(
    USkinnedMeshComponent* InSkinnedMeshComponent, 
    int32 InOverallMaxLODs, 
    const FMatrix& InComponentToWorld, 
    const FString& InPackageName, 
    TArray<FRawMeshTracker>& OutRawMeshTrackers, 
    TArray<FRawMesh>& OutRawMeshes, 
    TArray<UMaterialInterface*>& OutMaterials, 
    int32 SpecificSectionIndex,
    const int32 OutRigidBoneIndex)
{
    const int32 BaseMaterialIndex = OutMaterials.Num();

    // Export all LODs to raw meshes
    const int32 NumLODs = InSkinnedMeshComponent->GetNumLODs();
    //The cpu skinned vertice is not valid under min lod
    int32 MinLOD = InSkinnedMeshComponent->ComputeMinLOD();

    for (int32 OverallLODIndex = MinLOD; OverallLODIndex < InOverallMaxLODs; OverallLODIndex++)
    {
        int32 LODIndexRead = FMath::Min(OverallLODIndex, NumLODs - 1);

        FRawMesh& RawMesh = OutRawMeshes[OverallLODIndex];
        FRawMeshTracker& RawMeshTracker = OutRawMeshTrackers[OverallLODIndex];
        const int32 BaseVertexIndex = RawMesh.VertexPositions.Num();

        FSkeletalMeshLODInfo& SrcLODInfo = *(InSkinnedMeshComponent->GetSkinnedAsset()->GetLODInfo(LODIndexRead));

        // *** Specific Golaem 
        FSkeletalMeshModel* ImportedModel = InSkinnedMeshComponent->SkeletalMesh->GetImportedModel();
        FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndexRead];
        // can happen on lower lods that there is lower sections, ToDo : still need to match them
        if (SpecificSectionIndex >= LODModel.Sections.Num())
            return;
        // *** End Specific Golaem

        // Get the CPU skinned verts for this LOD
        TArray<FFinalSkinVertex> FinalVertices;
        InSkinnedMeshComponent->GetCPUSkinnedVertices(FinalVertices, LODIndexRead);

        FSkeletalMeshRenderData& SkeletalMeshRenderData = InSkinnedMeshComponent->MeshObject->GetSkeletalMeshRenderData();
        FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData.LODRenderData[LODIndexRead];

        // *** Specific Golaem 
        FMatrix referenceBoneMatrix = InSkinnedMeshComponent->SkeletalMesh->GetComposedRefPoseMatrix(OutRigidBoneIndex);
        FMatrix worldToBone = referenceBoneMatrix.Inverse() * InComponentToWorld;
        // *** End Specific Golaem

		// Copy skinned vertex positions
        RawMesh.VertexPositions.Reserve(FinalVertices.Num()); // Golaem Optim
        for (int32 VertIndex = 0; VertIndex < FinalVertices.Num(); ++VertIndex)
        {
            RawMesh.VertexPositions.Add((FVector4f)worldToBone.TransformPosition((FVector)FinalVertices[VertIndex].Position));
        }

        const uint32 NumTexCoords = FMath::Min(LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(), (uint32)MAX_MESH_TEXTURE_COORDS);
        const int32 NumSections = LODData.RenderSections.Num();
        FRawStaticIndexBuffer16or32Interface& IndexBuffer = *LODData.MultiSizeIndexContainer.GetIndexBuffer();

        int32 SectionIndex = SpecificSectionIndex;
        {
            const FSkelMeshRenderSection& SkelMeshSection = LODData.RenderSections[SectionIndex ];
            // if (InSkinnedMeshComponent->IsMaterialSectionShown(SkelMeshSection.MaterialIndex, LODIndexRead))
            {
                const int32 NumWedges = SkelMeshSection.NumTriangles * 3;

                // *** start Golaem Optim
                for (uint32 iTexCoordIndex = 0; iTexCoordIndex < NumTexCoords; iTexCoordIndex++)
                {
                    RawMesh.WedgeTexCoords[iTexCoordIndex].Reserve(NumWedges);
                }                
                RawMesh.WedgeTangentX.Reserve(NumWedges);
                RawMesh.WedgeTangentY.Reserve(NumWedges);
                RawMesh.WedgeTangentZ.Reserve(NumWedges);
                RawMesh.WedgeIndices.Reserve(NumWedges);
                RawMesh.WedgeColors.Reserve(NumWedges);
                // *** end Golaem Optim

                // Build 'wedge' info
                for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; WedgeIndex++)
                {
                    const int32 VertexIndexForWedge = IndexBuffer.Get(SkelMeshSection.BaseIndex + WedgeIndex);

                    RawMesh.WedgeIndices.Add(BaseVertexIndex + VertexIndexForWedge);

                    const FFinalSkinVertex& SkinnedVertex = FinalVertices[VertexIndexForWedge];
                    const FVector3f TangentX = (FVector4f)worldToBone.TransformVector(SkinnedVertex.TangentX.ToFVector());
                    const FVector3f TangentZ = (FVector4f)worldToBone.TransformVector(SkinnedVertex.TangentZ.ToFVector());
                    const FVector4 UnpackedTangentZ = SkinnedVertex.TangentZ.ToFVector4();
                    const FVector3f TangentY = (TangentZ ^ TangentX).GetSafeNormal() * UnpackedTangentZ.W;

                    RawMesh.WedgeTangentX.Add(TangentX);
                    RawMesh.WedgeTangentY.Add(TangentY);
                    RawMesh.WedgeTangentZ.Add(TangentZ);

                    for (uint32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; TexCoordIndex++)
                    {
                        if (TexCoordIndex >= NumTexCoords)
                        {
                            RawMesh.WedgeTexCoords[TexCoordIndex].AddDefaulted();
                        }
                        else
                        {
                            RawMesh.WedgeTexCoords[TexCoordIndex].Add(LODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(VertexIndexForWedge, TexCoordIndex));
                            RawMeshTracker.bValidTexCoords[TexCoordIndex] = true;
                        }                        
                    }

                    if (LODData.StaticVertexBuffers.ColorVertexBuffer.IsInitialized() && LODData.StaticVertexBuffers.ColorVertexBuffer.GetNumVertices() > 0)
                    {
                        RawMesh.WedgeColors.Add(LODData.StaticVertexBuffers.ColorVertexBuffer.VertexColor(VertexIndexForWedge));
                        RawMeshTracker.bValidColors = true;
                    }
                    else
                    {
                        RawMesh.WedgeColors.Add(FColor::White);
                    }
                }

                int32 MaterialIndex = SkelMeshSection.MaterialIndex;
                // use the remapping of material indices if there is a valid value
                if (SrcLODInfo.LODMaterialMap.IsValidIndex(SectionIndex ) && SrcLODInfo.LODMaterialMap[SectionIndex ] != INDEX_NONE)
                {
                    MaterialIndex = FMath::Clamp<int32>(SrcLODInfo.LODMaterialMap[SectionIndex ], 0, InSkinnedMeshComponent->GetSkinnedAsset()->GetMaterials().Num() - 1);
                }

                // copy face info
                for (uint32 TriIndex = 0; TriIndex < SkelMeshSection.NumTriangles; TriIndex++)
                {
                    RawMesh.FaceMaterialIndices.Add(BaseMaterialIndex + MaterialIndex);
                    RawMesh.FaceSmoothingMasks.Add(0); // Assume this is ignored as bRecomputeNormals is false
                }
            }
        }

        RawMesh.CompactMaterialIndices();
    }

    ProcessMaterials<USkinnedMeshComponent>(InSkinnedMeshComponent, InPackageName, OutMaterials);
}

// From MeshUtilites, adapted to sections - Helper function for ConvertMeshesToStaticMesh
//-------------------------------------------------------------------------
UStaticMesh* GolaemMeshUtilities::ConvertMeshSectionToStaticMesh(
    const TArray<UMeshComponent*>& InMeshComponents,
    const FTransform& InRootTransform,
    const FString& InPackageName, // This is character instanced name 
    int32 SpecificSectionIndex,
    USkeletalMesh* InSkeletalMesh,
    int32 OutRigidBoneIndex,
    TArray<FSkeletalMeshLODInfo>& LODInfos,
    float CharacterRadius,
    UObject* parent)
{
    UStaticMesh* StaticMesh = nullptr;

    // Build a package name to use
    FString MeshName;
    FString PackageName;
    if (InPackageName.IsEmpty())
        return NULL; // stripped interactive dialog for golaem import, package name should not be empty
    else
    {
        PackageName = InPackageName;
        MeshName = *FPackageName::GetLongPackageAssetName(PackageName);
    }

    if(!PackageName.IsEmpty() && !MeshName.IsEmpty())
    {
        TArray<FRawMesh> RawMeshes;
        TArray<UMaterialInterface*> Materials;

        TArray<FRawMeshTracker> RawMeshTrackers;

        //FMatrix WorldToRoot = InRootTransform.ToMatrixWithScale().Inverse();

        // first do a pass to determine the max LOD level we will be combining meshes into
        int32 OverallMaxLODs = 0;
        for (UMeshComponent* MeshComponent : InMeshComponents)
        {
            USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(MeshComponent);
            UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent);

            if (IsValidSkinnedMeshComponent(SkinnedMeshComponent))
            {
                OverallMaxLODs = FMath::Max(SkinnedMeshComponent->MeshObject->GetSkeletalMeshRenderData().LODRenderData.Num(), OverallMaxLODs);
            }
            // else if (IsValidStaticMeshComponent(StaticMeshComponent))
            //{
            //     OverallMaxLODs = FMath::Max(StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources.Num(), OverallMaxLODs);
            // }
        }

        // Resize raw meshes to accommodate the number of LODs we will need
        RawMeshes.SetNum(OverallMaxLODs);
        RawMeshTrackers.SetNum(OverallMaxLODs);

        FMatrix InRootMatrix(InRootTransform.ToMatrixWithScale());

        // Export all visible components
        for (UMeshComponent* MeshComponent : InMeshComponents)
        {
            //FMatrix ComponentToWorld = MeshComponent->GetComponentTransform().ToMatrixWithScale() * WorldToRoot;

            USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(MeshComponent);
            UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent);

            if (IsValidSkinnedMeshComponent(SkinnedMeshComponent))
            {
                SkinnedMeshSectionToRawMesh(
                    SkinnedMeshComponent,
                    OverallMaxLODs,                    
                    InRootMatrix,
                    GetTransientPackage()->GetName() /*"PackageName"*/,
                    RawMeshTrackers,
                    RawMeshes,
                    Materials,
                    SpecificSectionIndex,
                    OutRigidBoneIndex); // stops if not rigid and we are in "for rigid meshes" mode

            }
            // else if (IsValidStaticMeshComponent(StaticMeshComponent))
            //{
            //     StaticMeshToRawMeshes(StaticMeshComponent, OverallMaxLODs, ComponentToWorld, PackageName, RawMeshTrackers, RawMeshes, Materials);
            // }
        }

        // build section name 
        FString materialName = "";
        if (InSkeletalMesh->GetImportedModel() && InSkeletalMesh->GetImportedModel()->LODModels.Num() > 0)
        {            
            FSkeletalMeshLODModel& LODModel = InSkeletalMesh->GetImportedModel()->LODModels[0];
            materialName = InSkeletalMesh->Materials[LODModel.Sections[SpecificSectionIndex].MaterialIndex].MaterialSlotName.ToString();
        }
        FString sectionName = InSkeletalMesh->GetName() + "_StaticMeshForSection_" + FString::FromInt(SpecificSectionIndex) + "_" + materialName;

        uint32 MaxInUseTextureCoordinate = 0;

        // scrub invalid vert color & tex coord data
        check(RawMeshes.Num() == RawMeshTrackers.Num());
        for (int32 RawMeshIndex = 0; RawMeshIndex < RawMeshes.Num(); RawMeshIndex++)
        {
            if (!RawMeshTrackers[RawMeshIndex].bValidColors)
            {
                RawMeshes[RawMeshIndex].WedgeColors.Empty();
            }

            for (uint32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; TexCoordIndex++)
            {
                if (!RawMeshTrackers[RawMeshIndex].bValidTexCoords[TexCoordIndex])
                {
                    RawMeshes[RawMeshIndex].WedgeTexCoords[TexCoordIndex].Empty();
                }
                else
                {
                    // Store first texture coordinate index not in use
                    MaxInUseTextureCoordinate = FMath::Max(MaxInUseTextureCoordinate, TexCoordIndex);
                }
            }
        }

        // Check if we got some valid data.
        bool bValidData = false;
        for (FRawMesh& RawMesh : RawMeshes)
        {
            if (RawMesh.IsValidOrFixable())
            {
                bValidData = true;
                break;
            }
        }

        if (bValidData)
        {
            //// Then find/create it.
            //UPackage* Package = CreatePackage(*PackageName);
            //check(Package);

            // Create StaticMesh object
            StaticMesh = NewObject<UStaticMesh>(parent, *sectionName, RF_NoFlags);
            StaticMesh->InitResources();

            StaticMesh->SetLightingGuid();

            // Determine which texture coordinate map should be used for storing/generating the lightmap UVs
            const uint32 LightMapIndex = FMath::Min(MaxInUseTextureCoordinate + 1, (uint32)MAX_MESH_TEXTURE_COORDS - 1);

            // Add source to new StaticMesh
            for (FRawMesh& RawMesh : RawMeshes)
            {
                if (RawMesh.IsValidOrFixable())
                {
                    FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
                    SrcModel.BuildSettings.bRecomputeNormals = false;
                    SrcModel.BuildSettings.bRecomputeTangents = false;
                    SrcModel.BuildSettings.bRemoveDegenerates = true;
                    SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
                    SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
                    SrcModel.BuildSettings.bGenerateLightmapUVs = true;
                    SrcModel.BuildSettings.SrcLightmapIndex = 0;
                    SrcModel.BuildSettings.DstLightmapIndex = LightMapIndex;
                    SrcModel.SaveRawMesh(RawMesh);
                }
            }

            // Copy materials to new mesh
            for(UMaterialInterface* Material : Materials)
            {
                StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material));
            }

            //Set the Imported version before calling the build
            StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

            // Set light map coordinate index to match DstLightmapIndex
            StaticMesh->SetLightMapCoordinateIndex(LightMapIndex);

            StaticMesh->bAutoComputeLODScreenSize = false; // use data from the caracter

            // setup section info map
            for (int32 RawMeshLODIndex = 0; RawMeshLODIndex < RawMeshes.Num(); RawMeshLODIndex++)
            {
                const FRawMesh& RawMesh = RawMeshes[RawMeshLODIndex];
                TArray<int32> UniqueMaterialIndices;
                for (int32 MaterialIndex : RawMesh.FaceMaterialIndices)
                {
                    UniqueMaterialIndices.AddUnique(MaterialIndex);
                }

                int32 SectionIndex = 0;
                for (int32 UniqueMaterialIndex : UniqueMaterialIndices)
                {
                    if (RawMesh.MaterialIndexToImportIndex.IsValidIndex(SectionIndex))
                    {
                        StaticMesh->GetSectionInfoMap().Set(RawMeshLODIndex, SectionIndex, FMeshSectionInfo(RawMesh.MaterialIndexToImportIndex[SectionIndex]));
                    }
                    else
                    {
                        StaticMesh->GetSectionInfoMap().Set(RawMeshLODIndex, SectionIndex, FMeshSectionInfo(UniqueMaterialIndex));
                    }
                    SectionIndex++;
                }
            }
            StaticMesh->GetOriginalSectionInfoMap().CopyFrom(StaticMesh->GetSectionInfoMap());

            // Build mesh from source
            TArray<FText> errors;
            StaticMesh->Build(true, &errors);

            // *** Golaem optim - unify screensize of each mesh based on skeletalmesh size and lod screenSizes :
            FBoxSphereBounds MeshBounds = StaticMesh->GetBounds(); // returns the bound local to the mesh, we need the one with the bone scale
            FMatrix boneWorldTransform = InSkeletalMesh->GetComposedRefPoseMatrix(OutRigidBoneIndex);
            float MeshRadius = boneWorldTransform.GetMaximumAxisScale() * MeshBounds.SphereRadius;
            StaticMesh->GetSourceModel(0).ScreenSize = 1;
            for (int32 RawMeshOverallLODIndex = 1; RawMeshOverallLODIndex < RawMeshes.Num(); RawMeshOverallLODIndex++)
            {
                StaticMesh->GetSourceModel(RawMeshOverallLODIndex).ScreenSize = LODInfos[RawMeshOverallLODIndex].ScreenSize.Default * MeshRadius / CharacterRadius;
            }
            // *** End Golaem optim 

            StaticMesh->PostEditChange();
            StaticMesh->MarkPackageDirty();

            // Notify asset registry of new asset
            FAssetRegistryModule::AssetCreated(StaticMesh);
        }
    }

    return StaticMesh;
}

#endif