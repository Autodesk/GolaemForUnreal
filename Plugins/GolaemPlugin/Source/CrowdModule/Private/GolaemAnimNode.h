/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once
#include "Runtime/Engine/Classes/Animation/AnimNodeBase.h"
//#include "Runtime/AnimGraphRuntime/Public/BoneControllers/AnimNode_SkeletalControlBase.h"
#include "GolaemAnimNode.generated.h"
/**
 * 
 */

class AGolaemCharacter;
class IGolaemSimulationInterface;

USTRUCT(BlueprintType)
struct CROWDMODULE_API FAnimNode_GolaemPose : public FAnimNode_Base
{
	GENERATED_BODY()

public:
	FAnimNode_GolaemPose();
	virtual ~FAnimNode_GolaemPose();

	// FAnimNode_Base interface

	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	//virtual void EvaluateComponentBase_AnyThread(FComponentSpacePoseContext& Output) override;
	virtual void Evaluate_AnyThread(FPoseContext & Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

private:
	AGolaemCharacter* GolaemCharacter;
    IGolaemSimulationInterface* ParentGolaemSimulation;
};
