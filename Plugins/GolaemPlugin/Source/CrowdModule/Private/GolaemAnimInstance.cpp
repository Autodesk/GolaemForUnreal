/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemAnimInstance.h"

void FGolaemInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);

	// initialize node manually 
	FAnimationInitializeContext InitContext(this);
	PoseNode.Initialize_AnyThread(InitContext);
}

// needed so that proxy register this node for preUpdate
FAnimNode_Base* FGolaemInstanceProxy::GetCustomRootNode()
{
	return &PoseNode;
}

void FGolaemInstanceProxy::GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	OutNodes.Add(&PoseNode);
}

bool FGolaemInstanceProxy::Evaluate(FPoseContext& Output)
{
	PoseNode.Evaluate_AnyThread(Output);

	return true;
}

void FGolaemInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	UpdateCounter.Increment();

	FAnimationUpdateContext UpdateContext(InContext, this);
	PoseNode.Update_AnyThread(UpdateContext);
}

UGolaemAnimInstance::UGolaemAnimInstance(const FObjectInitializer& Initializer)
	: Super(Initializer)
{

}

FAnimInstanceProxy* UGolaemAnimInstance::CreateAnimInstanceProxy()
{
	return new FGolaemInstanceProxy(this);
}