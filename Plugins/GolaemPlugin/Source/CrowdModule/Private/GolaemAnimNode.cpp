/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemAnimNode.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/MorphTarget.h"

#include "GolaemCharacter.h"
#include "GolaemCache.h"
#include "Runtime/Launch/Resources/Version.h"

FAnimNode_GolaemPose::FAnimNode_GolaemPose()
    : FAnimNode_Base()
    , GolaemCharacter(NULL)
    , ParentGolaemSimulation(NULL)
{
}

FAnimNode_GolaemPose::~FAnimNode_GolaemPose()
{
}

void FAnimNode_GolaemPose::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
    FAnimNode_Base::Initialize_AnyThread(Context);
    //Get the Actor Owner
    GolaemCharacter = Cast<AGolaemCharacter>(Context.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner());
    if (GolaemCharacter)
        ParentGolaemSimulation = Cast<IGolaemSimulationInterface>(GolaemCharacter->GetAttachParentActor()); // may be null if something goes wrong

    // forward to used poses, none for us
    //BasePose.Initialize(Context);
}

void FAnimNode_GolaemPose::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
    // forward to used poses, none for us
    //BasePose.CacheBones(Context);
}

// PreUpdate called in game (main) thread, insures that we have no locking conflict / unexisting cache with cached Simulation
void FAnimNode_GolaemPose::PreUpdate(const UAnimInstance* InAnimInstance)
{
    if (ParentGolaemSimulation)
    {
        if (GolaemCharacter->bEnabledInCache && !GolaemCharacter->IsHidden())
        {
            // get bone transform as world, take back actor transform. Let cache transform to displace whole cache
            GolaemCharacter->SetActorTransform(FTransform::Identity);
            ParentGolaemSimulation->GetBoneSpaceTransforms_AnyThread(GolaemCharacter->CrowdFieldIndex, GolaemCharacter->CacheIndex, /*frame,*/ GolaemCharacter->BonesSpaceTransforms, GolaemCharacter->MorphTargetWeights, GolaemCharacter->CurrentTime);

            //// set actor transform to root, edit boneSpaceTransform to remove root component
            if (GolaemCharacter->BonesSpaceTransforms.Num() > 0)
            {
                //GolaemCharacter->SetActorTransform(GolaemCharacter->BonesSpaceTransforms[0]);

                //GLM_DCC_TRACE_WARNING("SETTING GolaemCharacter Transform");
                FTransform actorTransform = GolaemCharacter->BonesSpaceTransforms[0];
                FQuat transformRotation = actorTransform.GetRotation();

                // reset scale & rotation on actor, keep translation
                actorTransform.SetScale3D(FVector(1, 1, 1));
                actorTransform.SetRotation(FQuat::Identity);
                GolaemCharacter->SetActorTransform(actorTransform);

                // set scale and rotation only on root to get manipulators in the right place
                FVector scale = GolaemCharacter->BonesSpaceTransforms[0].GetScale3D();
                GolaemCharacter->BonesSpaceTransforms[0].SetIdentity();
                GolaemCharacter->BonesSpaceTransforms[0].SetRotation(transformRotation);
                GolaemCharacter->BonesSpaceTransforms[0].SetScale3D(scale);
            }
        }
    }
}

void FAnimNode_GolaemPose::Update_AnyThread(const FAnimationUpdateContext& Context)
{
    GetEvaluateGraphExposedInputs().Execute(Context);

    // forward to used poses, none for us
    //BasePose.Update(Context);
}

void FAnimNode_GolaemPose::Evaluate_AnyThread(FPoseContext& Output)
{
    bool success = false;
    if (ParentGolaemSimulation)
    {
        USkeletalMeshComponent* skelComp = GolaemCharacter->GetSkeletalMeshComponent();

        int32 expectedBoneCount = Output.Pose.GetNumBones();
        //if (GolaemCharacter->BonesSpaceTransforms.Num() != expectedBoneCount)
        //{
        //	GLM_CROWD_TRACE_WARNING_LIMIT(
        //		"Mismatched number of bones in Golaem Cache Node "
        //		<< TCHAR_TO_ANSI(*ParentGolaemCache->GetName()) << " for Golaem Entity " << TCHAR_TO_ANSI(*GolaemCharacter->GetName()) << ". Expected number of bones is "
        //		<< expectedBoneCount << ", Skeletal Mesh has " << GolaemCharacter->BonesSpaceTransforms.Num() << " bones");
        //}
        //else
        {
            // set POSE

            ////// set actor transform to root, edit boneSpaceTransform to remove root component
            //if (GolaemCharacter->BonesSpaceTransforms.Num() > 0)
            //{
            //	GolaemCharacter->BonesSpaceTransforms[0] = GolaemCharacter->BonesSpaceTransforms[0] * GolaemCharacter->GetActorTransform().Inverse();

            //	// keep only scale (this mess up the selection focus in UE if we let it to the actor)
            //	// FVector scale = GolaemCharacter->BonesSpaceTransforms[0].GetScale3D();
            //	// GolaemCharacter->BonesSpaceTransforms[0].SetIdentity();
            //	// GolaemCharacter->BonesSpaceTransforms[0].SetScale3D(scale);
            //}

            success = true;
            Output.Pose.CopyBonesFrom(GolaemCharacter->BonesSpaceTransforms);

            // we may have BLENDSHAPES weights to set if the weights match plugged skeletalMesh
            if (GolaemCharacter->MorphTargetWeights.Num() > 0 && GolaemCharacter->MorphTargetWeights.Num() == skelComp->SkeletalMesh->MorphTargets.Num())
            {
                //activeMorphs = 0;
                FName morphTargetName;
                for (int32 iMorphTarget = 0; iMorphTarget < GolaemCharacter->MorphTargetWeights.Num(); iMorphTarget++)
                {
                    morphTargetName = *skelComp->SkeletalMesh->MorphTargets[iMorphTarget]->GetName();
                    skelComp->SetMorphTarget(*skelComp->SkeletalMesh->MorphTargets[iMorphTarget]->GetName(), GolaemCharacter->MorphTargetWeights[iMorphTarget]);
                }
            }
        }
    }

    if (!success)
        Output.ResetToRefPose();

    // Evaluate the input
    // forward to used poses, none for us
    //BasePose.Evaluate(Output);
}

void FAnimNode_GolaemPose::GatherDebugData(FNodeDebugData& DebugData)
{
    //FString DebugLine = DebugData.GetNodeName(this);

    //DebugData.AddDebugItem(DebugLine);

    //BasePose.GatherDebugData(DebugData);
}
