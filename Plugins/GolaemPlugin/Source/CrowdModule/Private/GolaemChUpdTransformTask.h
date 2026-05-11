/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "GameFramework/Actor.h"
#include "GolaemHISMeshComponent.h"
#include "Templates/Atomic.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "glmFrameData.h"
#include "glmSimulationData.h"
#include "glmMutex.h"
#include "glmScopedLock.h"
THIRD_PARTY_INCLUDES_END

class AGolaemCharacterInstanced;

/**
 * Parallel mesh draw command pass setup task context.
 */
class FUpdateInstanceBatchTransformTaskContext
{
public:
    FUpdateInstanceBatchTransformTaskContext();

    const glm::crowdio::GlmSimulationData* simuData;
    const glm::crowdio::GlmFrameData* frameData;
    int32 crowdFieldIndex;
    float crowdUnitFactor;
    TAtomic<int32> MeshIndexToProcess;
    AGolaemCharacterInstanced* CharacterInstanced;
};

/**
 * Task for a parallel setup of batch transform updates (per mesh component).
 */
class FUpdateInstanceBatchTransformTask
{
public:
    FUpdateInstanceBatchTransformTask(FUpdateInstanceBatchTransformTaskContext& InContext);

    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FUpdateInstanceBatchTransformTask, STATGROUP_TaskGraphTasks);
    }

    ENamedThreads::Type GetDesiredThread();

    static ESubsequentsMode::Type GetSubsequentsMode();

    void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);

private:
    FUpdateInstanceBatchTransformTaskContext& Context;
};