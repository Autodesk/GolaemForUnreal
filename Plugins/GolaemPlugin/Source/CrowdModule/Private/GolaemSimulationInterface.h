/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "Containers/Array.h"
#include "Math/TransformVectorized.h"
#include "UObject/Interface.h"

#include "GolaemSimulationInterface.generated.h"

class AGolaemCharacter;

UINTERFACE()
class UGolaemSimulationInterface : public UInterface
{
    GENERATED_BODY()
};

class IGolaemSimulationInterface
{
    GENERATED_BODY()
public:
    virtual bool GetBoneSpaceTransforms_AnyThread(int simulationIndex, int entityIndex, TArray<FTransform>& boneSpaceTransforms, TArray<float>& morphTargetWeights, float& entityCurrentTime) = 0;

    void ComputeSkeleton(class AGolaemCharacter* entityActor, TArray<int32>& boneHierarchy);
};
