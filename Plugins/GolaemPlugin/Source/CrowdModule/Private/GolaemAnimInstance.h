/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimInstance.h"
#include "GolaemAnimNode.h"

#include "GolaemAnimInstance.generated.h"

class ULiveLinkRetargetAsset;

/** Proxy override for this UAnimInstance-derived class */
USTRUCT()
struct CROWDMODULE_API FGolaemInstanceProxy : public FAnimInstanceProxy
{
public:

	GENERATED_BODY()

		FGolaemInstanceProxy()
	{
	}

	FGolaemInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

	virtual FAnimNode_Base* GetCustomRootNode() override;
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override;

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
    virtual bool Evaluate(FPoseContext& Output) override;
    virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;

    FAnimNode_GolaemPose PoseNode;
};

UCLASS(transient, NotBlueprintable)
class CROWDMODULE_API UGolaemAnimInstance : public UAnimInstance
{
	GENERATED_UCLASS_BODY()

	//void SetSubject(FName SubjectName)
	//{
	//	GetProxyOnGameThread<FGolaemInstanceProxy>().PoseNode.SubjectName = SubjectName;
	//}

	//void SetRetargetAsset(TSubclassOf<ULiveLinkRetargetAsset> RetargetAsset)
	//{
	//	GetProxyOnGameThread<FGolaemInstanceProxy>().PoseNode.RetargetAsset = RetargetAsset;
	//}

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

};