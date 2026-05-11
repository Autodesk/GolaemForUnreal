/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemChUpdTransformTask.h"

#include "Engine/StaticMesh.h"

#include "GolaemCharacterInstanced.h"

//-------------------------------------------------------------------------
FUpdateInstanceBatchTransformTaskContext::FUpdateInstanceBatchTransformTaskContext()
    : simuData(NULL)
    , frameData(NULL)
    , crowdFieldIndex(-1)
    //, factory(NULL)
    , crowdUnitFactor(1.f)
    , MeshIndexToProcess(0)
    , CharacterInstanced(NULL)
{
}

//-------------------------------------------------------------------------
FUpdateInstanceBatchTransformTask::FUpdateInstanceBatchTransformTask(FUpdateInstanceBatchTransformTaskContext& InContext)
    : Context(InContext)
{
}

//-------------------------------------------------------------------------
ENamedThreads::Type FUpdateInstanceBatchTransformTask::GetDesiredThread()
{
    static FAutoConsoleTaskPriority CPrio_FUpdateInstanceBatchTransformTask(
        TEXT("TaskGraph.TaskPriorities.FUpdateInstanceBatchTransformTask"),
        TEXT("Task and thread priority for FUpdateInstanceBatchTransformTask."),
        ENamedThreads::NormalThreadPriority,
        ENamedThreads::HighTaskPriority);
    return CPrio_FUpdateInstanceBatchTransformTask.Get();
}

//-------------------------------------------------------------------------
ESubsequentsMode::Type FUpdateInstanceBatchTransformTask::GetSubsequentsMode()
{
    return ESubsequentsMode::TrackSubsequents;
}

//-------------------------------------------------------------------------
void FUpdateInstanceBatchTransformTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
    GLM_UNREFERENCED(CurrentThread);
    GLM_UNREFERENCED(MyCompletionGraphEvent);

    if (Context.CharacterInstanced == NULL || Context.crowdFieldIndex == -1 || Context.simuData == NULL || Context.frameData == NULL)
        return;

    //TArray<FTransform> newInstancesTransforms;
    FTransform newInstancesTransform;

    int32 meshIndexToProcess = Context.MeshIndexToProcess++;
    while (meshIndexToProcess < Context.CharacterInstanced->MeshComponents.Num())
    {
        UGolaemHierarchicalInstancedStaticMeshComponent* golaemMeshComp = Cast<UGolaemHierarchicalInstancedStaticMeshComponent>(Context.CharacterInstanced->MeshComponents[meshIndexToProcess]);

        if (golaemMeshComp != NULL && golaemMeshComp->GetStaticMesh())
        {

            if (!golaemMeshComp->GetStaticMesh()->IsCompiling() && golaemMeshComp->GetStaticMesh()->HasValidRenderData(false))
            {
                FGolaemMeshRigidInstancesData& RigidData = golaemMeshComp->RigidInstanceData;

                if (RigidData.CacheIndicesPerCF.Num() <= Context.crowdFieldIndex)
                    return;

                uint32_t rigidBoneIndex = RigidData.RigidBoneIndex;

                // update instances by getting rigid bone location for all cache indices of this mesh :
                FCacheIndicesPerCF& cacheIndicesPerCF = RigidData.CacheIndicesPerCF[Context.crowdFieldIndex];
                TArray<int32>& cacheIndices = cacheIndicesPerCF.CacheIndices;
                TArray<int32>& instanceIndices = cacheIndicesPerCF.InstanceIndices;
                if (instanceIndices.Num() == 0)
                {
                    meshIndexToProcess++;
                    continue;
                }                    

                //newInstancesTransforms.SetNum(cacheIndices.Num(), false);

                // hack to get start index of those indices :
                glm::ScopedLock<glm::SpinLock> lock(Context.CharacterInstanced->_updateTransformTaskLock);
                int32 startIndex = instanceIndices[0];
                for (int iCacheEntity = 0; iCacheEntity < cacheIndices.Num(); iCacheEntity++)
                {
                    Context.CharacterInstanced->GetTransform(*(Context.simuData), *(Context.frameData), cacheIndices[iCacheEntity], rigidBoneIndex, newInstancesTransform /*s[iCacheEntity]*/, Context.crowdUnitFactor);

                    // set transform scale to 0 if not shown to hide this instance
                    if (!golaemMeshComp->IsInstanceShownByPercent(startIndex + iCacheEntity))
                    {
                        newInstancesTransform.SetScale3D(FVector(0, 0, 0));
                    }

                    golaemMeshComp->UpdateInstanceTransformWithoutTree(startIndex + iCacheEntity, newInstancesTransform, true /*world space*/, true /* mark render state dirty */, false);
                }
            }
            else
            {
                // ask for update next tick until compiiling is finished
                Context.CharacterInstanced->bWaitingForCompiling = true;
            }
        }
        meshIndexToProcess = Context.MeshIndexToProcess++;
    }
}