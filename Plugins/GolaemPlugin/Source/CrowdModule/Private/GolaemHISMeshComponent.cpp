/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemHISMeshComponent.h"

// #include "Private/InstancedStaticMesh.h"
#include "Runtime/Engine/Classes/Engine/InstancedStaticMesh.h"

#include "Engine/StaticMesh.h"

#include "GolaemCache.h"
#include "GolaemCharacterInstanced.h"
#include "GolaemCharacter.h"

#define LOCTEXT_NAMESPACE "GolaemHierarchicalInstancedStaticMeshComponent"

//-------------------------------------------------------------------------
UGolaemHierarchicalInstancedStaticMeshComponent::UGolaemHierarchicalInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
    : UHierarchicalInstancedStaticMeshComponent(ObjectInitializer)
    ,
    //: UInstancedStaticMeshComponent(ObjectInitializer),
    //MeshAssetIndex(-1),
    GolaemCharacter(NULL)
    , ParentGolaemCache(NULL)
    , bHasCacheTransformLock(false)
{
    PrimaryComponentTick.bCanEverTick = true;
    //PrimaryComponentTick.bTickEvenWhenPaused = false;
    PrimaryComponentTick.TickGroup = TG_PostPhysics; // to be sure to be after cache update
    PrimaryComponentTick.bStartWithTickEnabled = true;
    bTickInEditor = true;
}

#if WITH_EDITOR
//-------------------------------------------------------------------------
void UGolaemHierarchicalInstancedStaticMeshComponent::NotifySMInstanceMovementStarted(const FSMInstanceId& InstanceId)
{
    //GLM_CROWD_TRACE_WARNING("Started Moving " << InstanceId.InstanceIndex);
    UHierarchicalInstancedStaticMeshComponent::NotifySMInstanceMovementStarted(InstanceId);

    if (ParentGolaemCache)
    {
        bHasCacheTransformLock = ParentGolaemCache->TryGetTransformLock();
        if (bHasCacheTransformLock)
        {
            TransformedInstanceId = InstanceId;
            this->GetInstanceTransform(InstanceId.InstanceIndex, StartTransformValue, true);
        }
    }
}

//-------------------------------------------------------------------------
void UGolaemHierarchicalInstancedStaticMeshComponent::NotifySMInstanceMovementOngoing(const FSMInstanceId& InstanceId)
{
    //GLM_CROWD_TRACE_WARNING("Currently Moving " << InstanceId.InstanceIndex);
    UHierarchicalInstancedStaticMeshComponent::NotifySMInstanceMovementOngoing(InstanceId);
    // nothing to be done, we will update layout at the end.
}

//-------------------------------------------------------------------------
void UGolaemHierarchicalInstancedStaticMeshComponent::NotifySMInstanceMovementEnded(const FSMInstanceId& InstanceId)
{
    //GLM_CROWD_TRACE_WARNING("Ended Moving " << InstanceId.InstanceIndex);
    UHierarchicalInstancedStaticMeshComponent::NotifySMInstanceMovementEnded(InstanceId);
    if (bHasCacheTransformLock
        && ParentGolaemCache)
    {
        // ToDo : fix case where several instanced of same entity are called + clashing between actor entity & HIS components. We want only one edition for actor or HIS moving
        GLM_DEBUG_ASSERT(InstanceId == TransformedInstanceId);
        FTransform newTransform;
        this->GetInstanceTransform(InstanceId.InstanceIndex, newTransform, true);

        bool sameTransform = StartTransformValue.GetTranslation().Equals(newTransform.GetTranslation());
        bool sameRotation = StartTransformValue.GetRotation().Equals(newTransform.GetRotation());
        bool sameScale = StartTransformValue.GetScale3D().Equals(newTransform.GetScale3D());

        // don't care about the translate part if we are rotating around a character
        if (!sameTransform && sameRotation && sameScale)
        {
            // send a translate to layout
            FVector translationUnreal = newTransform.GetTranslation() - StartTransformValue.GetTranslation();

            // coordinate system switch unreal -> glm
            glm::Vector3 translation(translationUnreal.X, translationUnreal.Z, translationUnreal.Y);
            ParentGolaemCache->TranslateSelection(translation);
        }
        else 
        {
            // Build the complete deformation
            FTransform deltaTransform;
            FTransform startInverse = StartTransformValue.Inverse();
            deltaTransform = startInverse * newTransform;

            FVector CumulativeDeltaTranslation = deltaTransform.GetTranslation();
            FRotator DeltaRotation = deltaTransform.GetRotation().Rotator();
            FVector CumulativeDeltaScale = deltaTransform.GetScale3D();

            FRotator CumulativeDeltaRotation;
            CumulativeDeltaRotation.Add(DeltaRotation.Yaw, DeltaRotation.Pitch, DeltaRotation.Roll);

            if (!CumulativeDeltaRotation.Equals(FRotator::ZeroRotator))
            {
                // send a Rotate to layout
                FVector eulerRotation = CumulativeDeltaRotation.Euler();
                glm::Vector3 glmEulerRotation(eulerRotation.X, -eulerRotation.Y, eulerRotation.Z);
                ParentGolaemCache->RotateSelection(glmEulerRotation);
            }

            if (!CumulativeDeltaScale.Equals(FVector::OneVector))
            {
                // send a Scale to layout
                glm::Vector3 scale(CumulativeDeltaScale.X, CumulativeDeltaScale.Z, CumulativeDeltaScale.Y);

                // only handle uniform scales
                if (scale.x == scale.y && scale.x == scale.z)
                {
                    // if (bUseScalePivot)
                    //{
                    //	glm::Vector3 scalePivot(CumulativeScalePivot.X, CumulativeScalePivot.Z, CumulativeScalePivot.Y);
                    //	ParentCache->ScaleSelection(scaleDiff, scalePivot, CrowdEntityId);
                    // }
                    // else
                    //{
                    ParentGolaemCache->ScaleSelection(scale.x);
                    //}

                    // we never handle expand, scale must be done on one entity only
                }
                else
                {
                    // refresh layout to show that we don't handle non uniform scaling
                    ParentGolaemCache->refreshLayoutFromEditor();
                }
            }
        }

        //ParentGolaemCache->refreshLayoutFromEditor();
        ParentGolaemCache->ReleaseTransformLock();
        bHasCacheTransformLock = false;
    }
}    

void UGolaemHierarchicalInstancedStaticMeshComponent::NotifySMInstanceSelectionChanged(const FSMInstanceId& InstanceId, const bool bIsSelected)
{
    UHierarchicalInstancedStaticMeshComponent::NotifySMInstanceSelectionChanged(InstanceId, bIsSelected);
    if (InstanceCrowdEntityIds.IsValidIndex(InstanceId.InstanceIndex))
    {
        int64 crowdEntityId = InstanceCrowdEntityIds[InstanceId.InstanceIndex];
        const glm::GlmSet<int64>& selectedEntities = ParentGolaemCache->GetSelectedEntitiesInt();
        if (bIsSelected)
        {
            if (selectedEntities.find(crowdEntityId) == selectedEntities.end())
            {
                ParentGolaemCache->SelectEntity(crowdEntityId);                        
            }            
        }
        else
        {
            if (selectedEntities.find(crowdEntityId) != selectedEntities.end())
            {
                ParentGolaemCache->UnselectEntity(InstanceCrowdEntityIds[InstanceId.InstanceIndex]);
            }            
        }
    }
}
#endif

int32 UGolaemHierarchicalInstancedStaticMeshComponent::AddInstanceWithCrowdID(const FTransform& InstanceTransform, int64 crowdEntityId, bool bWorldSpace)
{
    int instanceIndex = Super::AddInstance(InstanceTransform, bWorldSpace);
    if (instanceIndex != INDEX_NONE)
    {
        InstanceCrowdEntityIds.Add(crowdEntityId);
        IsInstanceShownByPercentArray.Add(1);        
    }
    return instanceIndex;
}

void UGolaemHierarchicalInstancedStaticMeshComponent::ClearInstances()
{
    InstanceCrowdEntityIds.Empty();
    IsInstanceShownByPercentArray.Empty();
    Super::ClearInstances();
}

bool UGolaemHierarchicalInstancedStaticMeshComponent::RemoveInstance(int32 InstanceIndex)
{
    if (InstanceCrowdEntityIds.IsValidIndex(InstanceIndex))
    {
        InstanceCrowdEntityIds.RemoveAt(InstanceIndex);
        IsInstanceShownByPercentArray.RemoveAt(InstanceIndex);
    }
    return Super::RemoveInstance(InstanceIndex);
}

//-------------------------------------------------------------------------
bool UGolaemHierarchicalInstancedStaticMeshComponent::UpdateInstanceTransformWithoutTree(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
    if (!PerInstanceSMData.IsValidIndex(InstanceIndex))
    {
        return false;
    }

    bIsOutOfDate = true;
    // invalidate the results of the current async build we need to modify the tree
    bConcurrentChanges |= IsAsyncBuilding();

    const int32 RenderIndex = GetRenderIndex(InstanceIndex);
    const FMatrix OldTransform = PerInstanceSMData[InstanceIndex].Transform;
    const FTransform NewLocalTransform = bWorldSpace ? NewInstanceTransform.GetRelativeTransform(GetComponentTransform()) : NewInstanceTransform;
    const FVector NewLocalLocation = NewLocalTransform.GetTranslation();

    // if we are only updating rotation/scale we update the instance directly in the cluster tree
    const bool bIsOmittedInstance = (RenderIndex == INDEX_NONE);
    const bool bIsBuiltInstance = !bIsOmittedInstance && RenderIndex < NumBuiltRenderInstances;
    bool bAllowInPlaceUpdateForRotationOrScaleChange = true;

    // Code path using 'bDoInPlaceUpdate' indicates that it updates the cluster tree but
    // bounds are not updated until next tree rebuild and some overlapping queries rely on those bounds.
    // We want to make sure a manipulation in an Editor world will fully update the information so queries
    // will return proper information to callers (e.g. navigation rebuild and preview)
#if WITH_EDITOR
    if (const UWorld* World = GetWorld())
    {
        const bool bIsGameWorld = World->IsGameWorld();
        bAllowInPlaceUpdateForRotationOrScaleChange = bIsGameWorld;
    }
#endif // WITH_EDITOR

    // if we are only updating rotation/scale then we update the instance directly in the cluster tree
    const bool bDoInPlaceUpdate = bAllowInPlaceUpdateForRotationOrScaleChange && bIsBuiltInstance && NewLocalLocation.Equals(OldTransform.GetOrigin());

    bool Result = true;
    // bool Result = UInstancedStaticMeshComponent::UpdateInstanceTransform(InstanceIndex, NewInstanceTransform, bWorldSpace, bMarkRenderStateDirty, bTeleport);
    {
        // we don't handle : Modify() and PreviousTransform for undo, navigation, physics
        if (!PerInstanceSMData.IsValidIndex(InstanceIndex))
        {
            Result = false;
        }
        else
        {
            FInstancedStaticMeshInstanceData& InstanceData = PerInstanceSMData[InstanceIndex];

            // TODO: Computing LocalTransform is useless when we're updating the world location for the entire mesh.
            // Should find some way around this for performance.

            // Render data uses local transform of the instance
            FTransform LocalTransform = bWorldSpace ? NewInstanceTransform.GetRelativeTransform(GetComponentTransform()) : NewInstanceTransform;
            InstanceData.Transform = LocalTransform.ToMatrixWithScale();

            if (bMarkRenderStateDirty)
            {
                MarkRenderStateDirty();
            }
        }
    }

    // The tree will be fully rebuilt once the static mesh compilation is finished, no need for incremental update in that case.
    if (Result && GetStaticMesh() && !GetStaticMesh()->IsCompiling() && GetStaticMesh()->HasValidRenderData())
    {
        const FBox NewInstanceBounds = GetStaticMesh()->GetBounds().GetBox().TransformBy(NewLocalTransform);

        if (bDoInPlaceUpdate)
        {
            // If the new bounds are larger than the old ones, then expand the bounds on the tree to make sure culling works correctly
            const FBox OldInstanceBounds = GetStaticMesh()->GetBounds().GetBox().TransformBy(OldTransform);
            if (!OldInstanceBounds.IsInside(NewInstanceBounds))
            {
                BuiltInstanceBounds += NewInstanceBounds;
            }
        }
        else
        {
            UnbuiltInstanceBounds += NewInstanceBounds;
            UnbuiltInstanceBoundsList.Add(NewInstanceBounds);

            //BuildTreeIfOutdated(true, false); //moved to EndUpdateInstanceTransforms
        }
    }

    return Result;
}

//-------------------------------------------------------------------------
void UGolaemHierarchicalInstancedStaticMeshComponent::UpdateTree()
{
    BuildTreeIfOutdated(true, false);
}

//-------------------------------------------------------------------------
bool UGolaemHierarchicalInstancedStaticMeshComponent::IsInstanceShownByPercent(int32 InstanceIndex) const
{
    return IsInstanceShownByPercentArray.IsValidIndex(InstanceIndex) ? IsInstanceShownByPercentArray[InstanceIndex]==1 : false;
}

//-------------------------------------------------------------------------
void UGolaemHierarchicalInstancedStaticMeshComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    GLM_UNREFERENCED(DeltaTime);
    GLM_UNREFERENCED(TickType);
    GLM_UNREFERENCED(ThisTickFunction);

    if (GolaemCharacter == NULL)
    {
        //Get the Actor Owner
        GolaemCharacter = Cast<AGolaemCharacterInstanced>(GetOwner());
        if (GolaemCharacter)
            ParentGolaemCache = Cast<AGolaemCache>(GolaemCharacter->GetAttachParentActor()); // may be null if something goes wrong
    }

    // need to double check if we are monothread here, but we should eb in game (main) thread in tick components (as for the one calling preUpdate of anim node for skeletal mesh)
    // so that currentFrame should be updated accordingly
    if (ParentGolaemCache && GolaemCharacter->GetCurrentFrame() != ParentGolaemCache->CurrentFrame)
    {
        // call transforms update from the cache (attached AActor)
        ParentGolaemCache->UpdateTransforms_AnyThread(GolaemCharacter);
    }

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

#undef LOCTEXT_NAMESPACE