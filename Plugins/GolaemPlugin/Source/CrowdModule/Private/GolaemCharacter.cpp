/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemCharacter.h" //needs to be included first

#include "Runtime/Launch/Resources/Version.h"
#include "UObject/ConstructorHelpers.h" /*Runtime/CoreUObject/Public*/
#include "Components/SceneComponent.h"

#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Engine/Brush.h"
#include "Engine/SkeletalMesh.h"
#include "SkeletalMeshMerge.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Algo/Reverse.h"
#include "Runtime/Engine/Classes/Components/SkinnedMeshComponent.h"

#include "GolaemCache.h"

//-------------------------------------------------------------------------
AGolaemCharacter::AGolaemCharacter(const FObjectInitializer& ObjectInitializer)
    : ASkeletalMeshActor(ObjectInitializer)
    , bMorphTargetFromAnim(false)
    , CurrentTime(FLT_MAX)
    , bShownByPercent(true)
    , bEnabledInCache(false)
    , CumulativeDeltaTranslation(FVector::ZeroVector)
    , CumulativeDeltaRotation(FRotator::ZeroRotator)
    , CumulativeDeltaScale(FVector::ZeroVector)
    , bUseScalePivot(false)
    , bSelectedInCache(false)
    , bHasTransformLock(false)
{
}

//void AGolaemCharacter::GetActorBounds(bool bOnlyCollidingComponents, FVector& Origin, FVector& BoxExtent, bool bIncludeFromChildActors) const
//{
//    // Get the skeletal mesh component
//    const USkeletalMeshComponent* SkelComp = GetSkeletalMeshComponent();
//    if (SkelComp && SkelComp->IsRegistered() && SkelComp->SkeletalMesh)
//    {
//        // Get bounds from the current pose
//        FBoxSphereBounds MeshBounds = SkelComp->CalcBounds(SkelComp->GetComponentTransform());
//        Origin = MeshBounds.Origin;
//        BoxExtent = MeshBounds.BoxExtent;
//        return;
//    }
//    return Super::GetActorBounds(bOnlyCollidingComponents, Origin, BoxExtent, bIncludeFromChildActors);
//}

#if WITH_EDITOR
void AGolaemCharacter::OnSelectionChanged(UObject* Object)
{
    AGolaemCache* ParentCache = Cast<AGolaemCache>(GetAttachParentActor());
    if (ParentCache)
    {
        if (IsSelected())
        {
            bSelectedInCache = true;
            // register in cache
            ParentCache->SelectEntity(CrowdEntityId);
        }
        else if (bSelectedInCache)
        {
            // unregister from cache
            ParentCache->UnselectEntity(CrowdEntityId);
            bSelectedInCache = false;
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemCharacter::EditorApplyTranslation(
    const FVector& DeltaTranslation,
    bool bAltDown,
    bool bShiftDown,
    bool bCtrlDown)
{
    USkeletalMeshComponent* skelMeshComponent = GetSkeletalMeshComponent();
    skelMeshComponent->SetUpdateAnimationInEditor(false);

    AGolaemCache* ParentCache = Cast<AGolaemCache>(GetAttachParentActor());
    if (ParentCache && !bHasTransformLock)
    {
        bHasTransformLock = ParentCache->TryGetTransformLock();
    }  

    if (DeltaTranslation != FVector::ZeroVector)
    {
        CumulativeDeltaTranslation += DeltaTranslation;
        glm::Vector3 translation(DeltaTranslation.X, DeltaTranslation.Y, DeltaTranslation.Z);

        Super::EditorApplyTranslation(DeltaTranslation, bAltDown, bShiftDown, bCtrlDown);
    }
}

//-------------------------------------------------------------------------
void AGolaemCharacter::EditorApplyScale(
    const FVector& DeltaScale,
    const FVector* PivotLocation,
    bool bAltDown,
    bool bShiftDown,
    bool bCtrlDown)
{
    USkeletalMeshComponent* skelMeshComponent = GetSkeletalMeshComponent();
    skelMeshComponent->SetUpdateAnimationInEditor(false);

    AGolaemCache* ParentCache = Cast<AGolaemCache>(GetAttachParentActor());
    if (ParentCache && !bHasTransformLock)
    {
        bHasTransformLock = ParentCache->TryGetTransformLock();
    }  
    if (DeltaScale != FVector::ZeroVector)
    {
        glm::Vector3 scale(DeltaScale.X, DeltaScale.Y, DeltaScale.Z);

        bUseScalePivot = PivotLocation != NULL;

        if (PivotLocation)
        {
            CumulativeScalePivot = *PivotLocation;
            glm::Vector3 scalePivot(PivotLocation->X, PivotLocation->Y, PivotLocation->Z);
        }

        CumulativeDeltaScale += DeltaScale;

        Super::EditorApplyScale(DeltaScale, PivotLocation, bAltDown, bShiftDown, bCtrlDown);
    }
}

//-------------------------------------------------------------------------
void AGolaemCharacter::EditorApplyRotation(
    const FRotator& DeltaRotation,
    bool bAltDown,
    bool bShiftDown,
    bool bCtrlDown)
{
    USkeletalMeshComponent* skelMeshComponent = GetSkeletalMeshComponent();
    skelMeshComponent->SetUpdateAnimationInEditor(false);

    AGolaemCache* ParentCache = Cast<AGolaemCache>(GetAttachParentActor());
    if (ParentCache && !bHasTransformLock)
    {
        bHasTransformLock = ParentCache->TryGetTransformLock();
    }  
    CumulativeDeltaRotation.Add(DeltaRotation.Yaw, DeltaRotation.Pitch, DeltaRotation.Roll);

    Super::EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);
}

//-------------------------------------------------------------------------
void AGolaemCharacter::PostEditMove(bool bFinished)
{
    USkeletalMeshComponent* skelMeshComponent = GetSkeletalMeshComponent();

    // here we must send cumulative delta of translation / rotation / scale when finished
    if (bFinished)
    {
        skelMeshComponent->SetUpdateAnimationInEditor(true);

        AGolaemCache* ParentCache = Cast<AGolaemCache>(GetAttachParentActor());
        if (ParentCache && bHasTransformLock)
        {
            // don't care about the translate part if we are rotating around a character
            if (CumulativeDeltaTranslation != FVector::ZeroVector && CumulativeDeltaRotation == FRotator::ZeroRotator && CumulativeDeltaScale == FVector::ZeroVector)
            {
                // send a translate to layout
                glm::Vector3 translation(CumulativeDeltaTranslation.X, CumulativeDeltaTranslation.Z, CumulativeDeltaTranslation.Y);
                ParentCache->TranslateSelection(translation);
            }

            if (CumulativeDeltaRotation != FRotator::ZeroRotator)
            {
                // send a Rotate to layout
                FVector eulerRotation = CumulativeDeltaRotation.Euler();
                glm::Vector3 glmEulerRotation(eulerRotation.X, -eulerRotation.Y, eulerRotation.Z);
                ParentCache->RotateSelection(glmEulerRotation);
            }

            if (CumulativeDeltaScale != FVector::ZeroVector)
            {
                // send a Scale to layout
                glm::Vector3 scale(CumulativeDeltaScale.X, CumulativeDeltaScale.Z, CumulativeDeltaScale.Y);

                const FTransform& currentTransform = GetTransform();

                // only handle uniform scales
                if (scale.x == scale.y && scale.x == scale.z)
                {
                    FVector currentScale = currentTransform.GetScale3D();
                    FVector prevScale = currentScale - CumulativeDeltaScale;

                    float scaleDiff = currentScale.X / prevScale.X;

                    //if (bUseScalePivot)
                    //{
                    //	glm::Vector3 scalePivot(CumulativeScalePivot.X, CumulativeScalePivot.Z, CumulativeScalePivot.Y);
                    //	ParentCache->ScaleSelection(scaleDiff, scalePivot, CrowdEntityId);
                    //}
                    //else
                    //{
                    ParentCache->ScaleSelection(scaleDiff);
                    //}

                    // we never handle expand, scale must be done on one entity only
                }
            }

            bHasTransformLock = false;
            ParentCache->ReleaseTransformLock();
        }

        // reset cumulative stuff
        CumulativeDeltaTranslation = FVector::ZeroVector;
        CumulativeDeltaRotation = FRotator::ZeroRotator;
        CumulativeDeltaScale = FVector::ZeroVector;
        bUseScalePivot = false;
    }
}

//-------------------------------------------------------------------------
void AGolaemCharacter::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
    FName propertyName = e.GetPropertyName();
    GLM_UNREFERENCED(propertyName);

    //GLM_CROWD_TRACE_WARNING("PostEditChangeChainProperty  actor " << CrowdEntityId << ", property : " << TCHAR_TO_ANSI(*(propertyName.ToString())));

    Super::PostEditChangeChainProperty(e);
}

//-------------------------------------------------------------------------
void AGolaemCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    FName propertyName = PropertyChangedEvent.GetPropertyName();
    GLM_UNREFERENCED(propertyName);

    //GLM_CROWD_TRACE_WARNING("PostEditChangeProperty  actor " << CrowdEntityId << ", property : " << TCHAR_TO_ANSI(*(propertyName.ToString())));

    Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

//-------------------------------------------------------------------------
void AGolaemCharacter::OnConstruction(const FTransform& Transform)
{
    ASkeletalMeshActor::OnConstruction(Transform);

    // restore meshes to hide or show
    if (MeshesToKeep.Num() > 0)
    {
        MeshSelectionByMaterialSection(MeshesToKeep);
    }
}

//-------------------------------------------------------------------------
void AGolaemCharacter::PostLoad()
{
    AGolaemCache* ParentCache = Cast<AGolaemCache>(GetAttachParentActor());
    GolaemCacheEditorWrapper* golaemCacheEditorWrapper = ParentCache->GetGolaemCacheEditorWrapper();
    if (golaemCacheEditorWrapper->getWrapper() != NULL)
    {
        golaemCacheEditorWrapper->getWrapper()->AddSelectionCallBack(*this);
    }

    Super::PostLoad();

	// prerequisites are NOT copied when duplicating for play ...
    AActor* ParentActor = Cast<AGolaemCache>(GetAttachParentActor()); // may be null if something goes wrong
    if (ParentActor != NULL && GetSkeletalMeshComponent() != NULL)
    {
        // get skinned mesh comp and set a prerequisite
        GetSkeletalMeshComponent()->AddTickPrerequisiteActor(ParentActor);
    }

    //C est une fonction appelee dans MeshSelection qui cause le crash au startup donc a commenter si on n'arrive plus a acceder a l'editeur a cause du crash
    //MeshSelectionByMerge(DirMeshPath);
}

////-------------------------------------------------------------------------
//void AGolaemCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
//{
//}

//-------------------------------------------------------------------------
void AGolaemCharacter::MeshSelectionByMaterialSection(const TArray<int>& MeshAssetsToKeep)
{
    // MeshesToKeep are indices in asset materials

    MeshesToKeep = MeshAssetsToKeep;

    // we have the mesh asset asso
    // LODSections have been built in the same order, let's disable via showMaterialSection the unwanted meshes
    USkeletalMeshComponent* skelMeshComponent = GetSkeletalMeshComponent();

	//if (skelMeshComponent->GetNumBones() > 256) A try when debugging, did not fix the issue, will be a hard limit in doc
	//	skelMeshComponent->SetCPUSkinningEnabled(true, true);

    // disable all materials sections
    int materialCount = skelMeshComponent->GetNumMaterials();
    int lodCount = skelMeshComponent->GetNumLODs();

    for (int iLOD = 0; iLOD < lodCount; iLOD++)
    {
        FSkeletalMeshLODInfo& SrcLODInfo = *(skelMeshComponent->SkeletalMesh->GetLODInfo(iLOD));

        if (SrcLODInfo.LODMaterialMap.Num() == 0) // no remapping, set all material sections
        {
            for (int iMat = 0; iMat < materialCount; iMat++)
            {
                skelMeshComponent->ShowMaterialSection(iMat, iMat, false, iLOD);
            }
        }
        else
        {
            for (int iSection = 0; iSection < SrcLODInfo.LODMaterialMap.Num(); iSection++)
            {
                if (SrcLODInfo.LODMaterialMap[iSection] == INDEX_NONE) // no remapping
                {
                    skelMeshComponent->ShowMaterialSection(iSection, iSection, false, iLOD);
                }
                else
                {
                    skelMeshComponent->ShowMaterialSection(SrcLODInfo.LODMaterialMap[iSection], iSection, false, iLOD);
                }
            }
        }

        // reenable chosen ones (if not rigid instanced)
        for (int i = 0; i < MeshesToKeep.Num(); i++)
        {
            if (!RigidSections.IsValidIndex(MeshesToKeep[i]) || !RigidSections[MeshesToKeep[i]])
            {
                if (SrcLODInfo.LODMaterialMap.Num() == 0) // no remapping, set all material sections
                {
                    skelMeshComponent->ShowMaterialSection(MeshesToKeep[i], MeshesToKeep[i], true, iLOD);
                }
                else
                {
                    if (SrcLODInfo.LODMaterialMap.IsValidIndex(MeshesToKeep[i]) && SrcLODInfo.LODMaterialMap[MeshesToKeep[i]] == INDEX_NONE) // case where section is not remapped (= asset material)
                    {
                        skelMeshComponent->ShowMaterialSection(MeshesToKeep[i], MeshesToKeep[i], false, iLOD);
                    }
                    else // try to find the mesh to keep in remapped values
                    {
                        int iSection = SrcLODInfo.LODMaterialMap.Find(MeshesToKeep[i]);
                        if (iSection != INDEX_NONE)
                        {
                            skelMeshComponent->ShowMaterialSection(SrcLODInfo.LODMaterialMap[iSection], iSection, true, iLOD);
                        }
                    }
                }
            }
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemCharacter::AddShaderScalarParameter(int matIndex, int paramIndex, int shaderAttrIndex)
{
    FDynamicMaterialInfo dynMaterial;
    dynMaterial.MaterialIndex = matIndex;
    dynMaterial.ScalarParameterIndex = paramIndex;
    dynMaterial.VectorParameterIndex = -1;
    dynMaterial.shaderAttrIndex = shaderAttrIndex;
    DynamicMaterials.Add(dynMaterial);
}

//-------------------------------------------------------------------------
void AGolaemCharacter::AddShaderVectorParameter(int matIndex, int paramIndex, int shaderAttrIndex)
{
    FDynamicMaterialInfo dynMaterial;
    dynMaterial.MaterialIndex = matIndex;
    dynMaterial.ScalarParameterIndex = -1;
    dynMaterial.VectorParameterIndex = paramIndex;
    dynMaterial.shaderAttrIndex = shaderAttrIndex;
    DynamicMaterials.Add(dynMaterial);
}

//-------------------------------------------------------------------------
void AGolaemCharacter::BeginPlay()
{
    ASkeletalMeshActor::BeginPlay();

    // restore meshes to hide or show
    if (MeshesToKeep.Num() > 0)
    {
        MeshSelectionByMaterialSection(MeshesToKeep);
    }
}