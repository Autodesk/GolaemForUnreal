/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemCharacterInstanced.h" //needs to be included first

#include "Runtime/Launch/Resources/Version.h"
#include "UObject/ConstructorHelpers.h" /*Runtime/CoreUObject/Public*/
#include "RawMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/InstancedStaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Components/SceneComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
//#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "Animation/Skeleton.h"
#include "GolaemCache.h"
#include "GolaemSimulation.h"
#include "GolaemMeshUtilities.h"
//#include "GolaemHierarchicalInstancedStaticMeshComponent.h"

#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Engine/Brush.h"
#include "UObject/UObjectGlobals.h"
//#include "Engine/SkeletalMesh.h"
//#include "SkeletalMeshMerge.h"
//#include "Components/PoseableMeshComponent.h"
//#include "Components/SkeletalMeshComponent.h"
//#include "Rendering/SkeletalMeshLODModel.h"
//#include "Rendering/SkeletalMeshRenderData.h"
#include "Algo/Reverse.h"
//#include "Runtime/Engine/Classes/Components/SkinnedMeshComponent.h"

#include "Engine/SkinnedAssetCommon.h"

#include "GlmCacheTypes.h"

THIRD_PARTY_INCLUDES_START
// #include <glmAssert.h>
// #include <glmMathBase.h>
// #include <glmStringOperators.h>
// #include <glmCrowdIOUtils.h>
#include <glmGolaemCharacter.h>
#include <glmContainersUtils.h>
//#include <glmRandomNumberGenerator.h>
//#include <glmHistory.h>
//#include <glmHistoryV0.h>
//#include <glmSimulationCacheCommon.h>
//#include <glmLog.h>
//#include <glmGeometryFile.h>

THIRD_PARTY_INCLUDES_END

//-------------------------------------------------------------------------
AGolaemCharacterInstanced::AGolaemCharacterInstanced(const FObjectInitializer& ObjectInitializer)
    : AActor(ObjectInitializer)
    , CurrentFrame(FLT_MAX)
    //, bShownByPercent(true)
    //, bEnabledInCache(false)
{

	PrimaryActorTick.bCanEverTick = true;
	//PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics; // cache updated in pre physics, characters after to be sure cache is available
    PrimaryActorTick.bStartWithTickEnabled = true;

	RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("RootComponent"));

	//SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent0"));
	//SkeletalMeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	//// check BaseEngine.ini for profile setup
	//SkeletalMeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
}

//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::OnConstruction(const FTransform& Transform)
{
    AActor::OnConstruction(Transform);

    //// restore meshes to hide or show
    //if (MeshesToKeep.Num() > 0)
    //{
    //    MeshSelectionByMaterialSection(MeshesToKeep);
    //}
}

// https://docs.unrealengine.com/portals/0/images/Programming/UnrealArchitecture/Actors/ActorLifecycle/ActorLifeCycle1.png
//PostLoad est appelee lors du runtime et au startup du moteur si un GolaemCache a ete sauvegarde dans la scene
void AGolaemCharacterInstanced::PostLoad()
{
    Super::PostLoad();

	// prerequisites are NOT copied when duplicating for play ...
	AActor* ParentActor = Cast<AGolaemCache>(GetAttachParentActor()); // may be null if something goes wrong
	if (ParentActor != NULL)
	{
        this->AddTickPrerequisiteActor(ParentActor);
	}	

	// force rebuild of transient components
	//UpdateSkeletalMesh();

    //C est une fonction appelee dans MeshSelection qui cause le crash au startup donc a commenter si on n'arrive plus a acceder a l'editeur a cause du crash
    //MeshSelectionByMerge(DirMeshPath);
}

void AGolaemCharacterInstanced::UpdateTransforms()
{
	AGolaemCache* ParentGolaemCache = Cast<AGolaemCache>(GetAttachParentActor()); // may be null if something goes wrong

	if (ParentGolaemCache != nullptr)
    {
        ParentGolaemCache->UpdateTransforms_AnyThread(this);
    }
	else
	{
        AGolaemSimulation* ParentGolaemSimulation = Cast<AGolaemSimulation>(GetAttachParentActor()); // may be null if something goes wrong

        if (ParentGolaemSimulation != nullptr)
        {
            ParentGolaemSimulation->UpdateTransforms_AnyThread(this);
        }
	}
}

//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::TickAtThisTime(const float Time, bool bInBackwards, bool bInIsLooping)
{
	UpdateTransforms();
}

//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::Tick(float DeltaTime)
{
    //GLM_DCC_TRACE_WARNING("Golaem Character Instanced Tick");
	UpdateTransforms();
}

////-------------------------------------------------------------------------
//void AGolaemCharacterInstanced::EndPlay(const EEndPlayReason::Type EndPlayReason)
//{
//}


#if WITH_EDITOR
//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
	if (e.GetPropertyName() == FName(TEXT("InSkeletalMesh")))
	{
		//glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        AGolaemCache* ParentGolaemCache = Cast<AGolaemCache>(GetAttachParentActor()); // may be null if something goes wrong
        UpdateSkeletalMesh(ParentGolaemCache->InstanceMode);
	}

	Super::PostEditChangeChainProperty(e);
}

//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::UpdateSkeletalMesh(EInstanceMode InstanceMode)
{
	// NOTE: InSkelMesh may be NULL (useful in the editor for removing the skeletal mesh associated with
	//   this component on-the-fly)

	// clear Components 
	TArray<USceneComponent*> childrenComp;
	RootComponent->GetChildrenComponents(false, childrenComp);
	for (int i = 0; i < childrenComp.Num(); i++)
	{
		//childrenComp[i]->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		childrenComp[i]->DestroyComponent(true);
	}
	MeshComponents.Empty();
    RigidSections.Empty();
	//this->ClearInstanceComponents(true);

	if (InSkeletalMesh != NULL /*.IsValid() && InSkeletalMesh.Get() != NULL*/)
	{
		USkeletalMeshComponent* DummyComponentForInstancing = NewObject<USkeletalMeshComponent>(this, "DummyComponentForBlending");
		DummyComponentForInstancing->RegisterComponent();
		DummyComponentForInstancing->SetSkeletalMesh(InSkeletalMesh/*.Get()*/);

		FMatrix ComponentToWorld = DummyComponentForInstancing->GetComponentTransform().ToMatrixWithScale() /** WorldToRoot*/;

		// build components, one static mesh component per section
		FSkeletalMeshModel* ImportedModel = InSkeletalMesh/*.Get()*/->GetImportedModel();
		if (ImportedModel != NULL && ImportedModel->LODModels.Num() > 0)
		{
			int32 SectionCount = ImportedModel->LODModels[0].Sections.Num();
            MeshComponents.Reserve(SectionCount);
            RigidSections.Reserve(SectionCount);

			// unify screensize of each mesh based on skeletalmesh size and lod screenSizes :
			TArray< FSkeletalMeshLODInfo>& LODInfos = InSkeletalMesh->GetLODInfoArray();
			FBoxSphereBounds characterBounds = InSkeletalMesh->GetBounds();
			float CharacterRadius = characterBounds.SphereRadius;

			TArray< TArray<FFinalSkinVertex> > cpuSkinnedVerticesPerLOD;
			cpuSkinnedVerticesPerLOD.SetNum(SectionCount);

			for (int32 iSection = 0; iSection < SectionCount; iSection++)
			{
				int32 OutRigidBoneIndex = -1;
                bool stopIfNotRigid = InstanceMode == EInstanceMode::FOR_RIGID_MESHES;
                bool sectionIsRigid = GolaemMeshUtilities::isSkinnedMeshSectionRigid(DummyComponentForInstancing, iSection, OutRigidBoneIndex, stopIfNotRigid);

				RigidSections.Add(sectionIsRigid);

				if ((sectionIsRigid && InstanceMode == EInstanceMode::FOR_RIGID_MESHES) || InstanceMode == EInstanceMode::FORCED)
                {
                    TArray<UMeshComponent*> InMeshComponents;
                    InMeshComponents.Add(DummyComponentForInstancing);

                    UStaticMesh* MeshAssetStaticMesh = GolaemMeshUtilities::ConvertMeshSectionToStaticMesh(
						InMeshComponents, 
						DummyComponentForInstancing->GetComponentTransform() , //ComponentToWorld,
						/* const FTransform& InRootTransform, */
						this->GetName(),
                        iSection,
                        InSkeletalMesh,
                        OutRigidBoneIndex,
                        LODInfos,
                        CharacterRadius,
						this);

				
					// build a static mesh and add a component from this raw mesh
					// examples are in GrassInstancedStaticMeshComponent and MassEntityTestFarmPlot
					UGolaemHierarchicalInstancedStaticMeshComponent* hismc = NewObject<UGolaemHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_NoFlags/*RF_Transient*/);
					hismc->Mobility = EComponentMobility::Movable;
					hismc->SetStaticMesh(MeshAssetStaticMesh);
					hismc->MinLOD = 0;
					hismc->bSelectable = true;
					hismc->bHasPerInstanceHitProxies = true;
					hismc->bReceivesDecals = false;
					static FName NoCollision(TEXT("NoCollision"));
					hismc->SetCollisionProfileName(NoCollision);
					hismc->SetCanEverAffectNavigation(false);
					hismc->InstancingRandomSeed = 0;

					hismc->bCastStaticShadow = false;
					hismc->CastShadow = true;
					hismc->bCastDynamicShadow = true;

					hismc->InstanceStartCullDistance = 0;
					hismc->InstanceEndCullDistance = 0;

					hismc->SetupAttachment(RootComponent); 
					hismc->RegisterComponent();
					AddOwnedComponent(hismc);
					AddInstanceComponent(hismc);

					MeshComponents.Add(hismc);
                    hismc->AddTickPrerequisiteActor(this);
				
					hismc->RigidInstanceData.RigidBoneIndex = OutRigidBoneIndex;
				}
				else
				{
					MeshComponents.Add(nullptr);
				}
			}
		}

		DummyComponentForInstancing->DestroyComponent();
	}
	   
	//{
	//	//Handle destroying and recreating the renderstate
	//	FRenderStateRecreator RenderStateRecreator(this);

	//	SkeletalMesh = InSkelMesh;

	//	for (TWeakObjectPtr<USkinnedMeshComponent>& SlavePoseComponent : SlavePoseComponents)
	//	{
	//		SlavePoseComponent->UpdateMasterBoneMap();
	//	}

	//	// Don't init anim state if not registered
	//	if (IsRegistered())
	//	{
	//		AllocateTransformData();
	//		UpdateMasterBoneMap();
	//		InvalidateCachedBounds();
	//		// clear morphtarget cache
	//		ActiveMorphTargets.Empty();
	//		MorphTargetWeights.Empty();
	//	}
	//}

	//if (IsRegistered())
	//{
	//	// We do this after the FRenderStateRecreator has gone as
	//	// UpdateLODStatus needs a valid MeshObject
	//	UpdateLODStatus();
	//}
}

#endif

//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::GetTransform(
	const glm::crowdio::GlmSimulationData& simuData,
	const glm::crowdio::GlmFrameData& frameData,
	uint32_t entityIndexInCache,
	int rigidBoneIndex,
	FTransform& currentTransform,
	float crowdUnitFactor)
{
    if (entityIndexInCache >= simuData._entityCount)
    {
        // mismatch between simData (cache or layout changed since generation after a UE scene reload for example) and existing characters in the scene (that have not been regenerated yet)
        return;
    }

	// scale :
	float scale = crowdUnitFactor * simuData._scales[entityIndexInCache];

	uint16_t entityType = simuData._entityTypes[entityIndexInCache];
	uint32_t indexInEntityType = simuData._indexInEntityType[entityIndexInCache];

	// bone transform : 
	uint16_t boneCount = simuData._boneCount[entityType];

	if (GchaToCacheBones.Num() != boneCount)
	{
        // mismatch between simData bone count and gchaToCacheBones. 
		// This can occur when a createEntity layout history does not find the character and pick the first available, which may not match the 
        return;
	}

	uint32_t typeBoneOffset = simuData._iBoneOffsetPerEntityType[entityType];
	uint32_t entityBoneOffset = typeBoneOffset + indexInEntityType * boneCount;

	uint32_t entityRigidBoneIndex = entityBoneOffset + GchaToCacheBones[rigidBoneIndex]; // transforms character bone index to cache one here

	FVector BonePosition;
	FQuat BoneOrientation;

	//entityRigidBoneIndex is a bone index in unreal / gcha , not in cache !! use sortedBones here

	BonePosition.X = crowdUnitFactor * frameData._bonePositions[entityRigidBoneIndex][0];// +cacheProxyTransform.GetLocation().X;
	//Y and Z positions are interverted because default up-axis of Maya is Y while Unreal's up-axis is Z
	BonePosition.Y = crowdUnitFactor * frameData._bonePositions[entityRigidBoneIndex][2];// +cacheProxyTransform.GetLocation().Y;
	BonePosition.Z = crowdUnitFactor * frameData._bonePositions[entityRigidBoneIndex][1];// +cacheProxyTransform.GetLocation().Z;

	BoneOrientation.X = frameData._boneOrientations[entityRigidBoneIndex][0];
	BoneOrientation.Y = frameData._boneOrientations[entityRigidBoneIndex][2];
	BoneOrientation.Z = frameData._boneOrientations[entityRigidBoneIndex][1];
	BoneOrientation.W = -frameData._boneOrientations[entityRigidBoneIndex][3];

	// Validate quaternion - a zero quaternion is invalid (e.g., uninitialized/corrupted frame data)
	// and will cause a crash in ToMatrixWithScale() in UGolaemHierarchicalInstancedStaticMeshComponent::UpdateInstanceTransformWithoutTree. Fall back to identity quaternion.
    if (BoneOrientation.Equals(FQuat(0, 0, 0, 0)))
	{
		BoneOrientation = FQuat::Identity;
	}

	static FQuat QuatCacheToUE4(-0.7071067811865475f, 0.f, 0.f, 0.7071067811865475f);
	BoneOrientation = BoneOrientation * QuatCacheToUE4;

	currentTransform.SetLocation(BonePosition);
	currentTransform.SetRotation(BoneOrientation);
	currentTransform.SetScale3D(FVector(scale, scale, scale));
	
	// add the cache transform
    AActor* ParentGolaemCacheOrSImu = GetAttachParentActor(); // may be null if something goes wrong
    if (ParentGolaemCacheOrSImu != NULL)
	{
        currentTransform = currentTransform * ParentGolaemCacheOrSImu->GetTransform();
	}
	
	if (frameData._entityEnabled[entityIndexInCache] != 1)
	{ 
		currentTransform.SetScale3D(FVector(0,0,0));
	}
}

//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::AddInstance(
	const glm::crowdio::GlmSimulationData* simuData,
	int32 crowdFieldIndex,
	const TArray<int>& MeshAssetsToKeep,
	uint32_t entityIndexInCache)
{    
	if (simuData == NULL || entityIndexInCache >= simuData->_entityCount)
    {
        return;
    }

	// add an instance for all MeshAssetsToKeep meshes in the range :
	// this is only called when creating from editor, which is done once for all, so MeshComponents matches MeshAssetsToKeep as there has not been any reload (which reorder components)
	FTransform defaultTransform;
	FTransform currentTransform;
	for (int iMeshToKeep = 0; iMeshToKeep < MeshAssetsToKeep.Num(); iMeshToKeep++)
	{
		int32 theMeshtoKeep = MeshAssetsToKeep[iMeshToKeep];
        if (!MeshComponents.IsValidIndex(theMeshtoKeep) || MeshComponents[theMeshtoKeep] == nullptr)
        {
            continue;
        }
		FGolaemMeshRigidInstancesData& RigidData = MeshComponents[theMeshtoKeep]->RigidInstanceData;

		// update instances by getting rigid bone location for all cache indices of this mesh :
		if (crowdFieldIndex >= RigidData.CacheIndicesPerCF.Num())
		{
			RigidData.CacheIndicesPerCF.SetNum(crowdFieldIndex + 1);
		}
		FCacheIndicesPerCF& cacheIndicesPerCF = RigidData.CacheIndicesPerCF[crowdFieldIndex];
		TArray< int32 >& cacheIndices = cacheIndicesPerCF.CacheIndices;
		TArray< int32 >& instanceIndices = cacheIndicesPerCF.InstanceIndices;

		// will be set by next calls to tick / update, use a dummy here to avoid using uninitialized data in GolaemSimulation init.
        FTransform dummyTransform;
        MeshComponents[theMeshtoKeep]->AddInstanceWithCrowdID(dummyTransform, simuData->_entityIds[entityIndexInCache]); // keep track of CrowdEntityId

		instanceIndices.Add(MeshComponents[theMeshtoKeep]->GetInstanceCount() - 1);
		cacheIndices.Add(entityIndexInCache);

		// match instance index, helper for selection
        //RigidData.CrowdEntityIds.Add(simuData->_entityIds[entityIndexInCache]);
	}
}

//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::SetShownInstances(const glm::GlmSet<int>& shownIDs, const glm::GlmSet<int>& hiddenIds)
{
    for (const auto& MeshComp : MeshComponents)
    {
        if (MeshComp != NULL)
        {
            TArray<int8> IsInstanceShownByPercentArrayNew = MeshComp->GetIsInstanceShownByPercentArray();

            // modify required values
            for (int InstanceIndex = 0; InstanceIndex < MeshComp->GetInstanceCrowdEntityIds().Num(); InstanceIndex++)
            {
				int ID = MeshComp->GetInstanceCrowdEntityIds()[InstanceIndex];
				if (shownIDs.find(ID) != shownIDs.end())
				{
					IsInstanceShownByPercentArrayNew[InstanceIndex] = true;
				}
				else if (hiddenIds.find(ID) != hiddenIds.end())
				{
					IsInstanceShownByPercentArrayNew[InstanceIndex] = false;
				}
            }

            MeshComp->SetIsInstanceShownByPercentArray(IsInstanceShownByPercentArrayNew);
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::RemoveInstances()
{
    for (int iMeshComp = 0, meshCompCount = MeshComponents.Num(); iMeshComp < meshCompCount; ++iMeshComp)
    {
        UGolaemHierarchicalInstancedStaticMeshComponent* meshComp = MeshComponents[iMeshComp];
        if (meshComp != NULL)
        {
            FGolaemMeshRigidInstancesData& rigidData = meshComp->RigidInstanceData;
            rigidData.CacheIndicesPerCF.Empty();
            meshComp->ClearInstances();
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::SetCharacterToCacheBones(const glm::GolaemCharacter* glmCharacter)
{
	GchaToCacheBones.Empty();
	if (glmCharacter != NULL)
	{
		const glm::PODArray<size_t>& charToCacheBones = glmCharacter->getConverterMapping()._skeletonDescription->getSpecificToCacheBoneIndices();
		for (size_t iBone = 0; iBone <  charToCacheBones.size(); iBone++)
		{
			GchaToCacheBones.Add(charToCacheBones[iBone]);
		}
	}
}

//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::UpdateTransforms(
	const glm::crowdio::GlmSimulationData& simuData, 
	const glm::crowdio::GlmFrameData& frameData, 
	int32 crowdFieldIndex, 
	//glm::crowdio::SimulationCacheFactory* factory, 
	float TheCurrentFrame, 
	float crowdUnitFactor)
{
	// update main currentFrame count to latestFrame
    CurrentFrame = TheCurrentFrame;
    if (CurrentFramePerCF.Num() <= crowdFieldIndex)
    {
        int lastSize = CurrentFramePerCF.Num();
        CurrentFramePerCF.SetNum(crowdFieldIndex + 1);
		for (int i = lastSize; i < CurrentFramePerCF.Num(); i++)
		{
            CurrentFramePerCF[i] = FLT_MAX;
		}
    }
    if (TheCurrentFrame != CurrentFramePerCF[crowdFieldIndex] || bTransformDirty || bWaitingForCompiling) // we need update even for same frame for editor live update
	{
		// update the current frame counter to the GolaemCache node one
        CurrentFramePerCF[crowdFieldIndex] = TheCurrentFrame;
		bTransformDirty = false;
        bWaitingForCompiling = false;

		// try to find them back, we are probably after a duplicate (beginning of play, etc)
		if (MeshComponents.Num() == 0)
		{
			TArray<USceneComponent*> components;
			GetRootComponent()->GetChildrenComponents(false, components);

			for (int iChild = 0; iChild < components.Num(); iChild++)
			{
				MeshComponents.Add(Cast<UGolaemHierarchicalInstancedStaticMeshComponent>(components[iChild]));
			}
		}

		FTransform newInstancesTransform;

		//// add an instance for all MeshAssetsToKeep meshes in the range :
		//for (int iMesh = 0; iMesh < MeshComponents.Num(); iMesh++)
		//{
		//	UGolaemHierarchicalInstancedStaticMeshComponent* golaemMeshComp = Cast<UGolaemHierarchicalInstancedStaticMeshComponent>(MeshComponents[iMesh]);
		//	if (golaemMeshComp != NULL)
		//	{
		//		FGolaemMeshRigidInstancesData& RigidData = golaemMeshComp->RigidInstanceData;

		//		if (RigidData.CacheIndicesPerCF.Num() <= crowdFieldIndex)
		//			continue;

		//		uint32_t rigidBoneIndex = RigidData.RigidBoneIndex;

		//		// update instances by getting rigid bone location for all cache indices of this mesh :
		//		FCacheIndicesPerCF& cacheIndicesPerCF = RigidData.CacheIndicesPerCF[crowdFieldIndex];
		//		TArray< int32 >& cacheIndices = cacheIndicesPerCF.CacheIndices;
		//		TArray< int32 >& instanceIndices = cacheIndicesPerCF.InstanceIndices;

		//		//newInstancesTransforms.SetNum(cacheIndices.Num(), false);

		//		// hack to get start idnex of those indices :
		//		int32 startIndex = instanceIndices[0];
		//		for (int iCacheEntity = 0; iCacheEntity < cacheIndices.Num(); iCacheEntity++)
		//		{
		//			GetTransform(simuData, frameData, cacheIndices[iCacheEntity], rigidBoneIndex, newInstancesTransform/*s[iCacheEntity]*/, crowdUnitFactor);
		//			MeshComponents[iMesh]->UpdateInstanceTransform(startIndex + iCacheEntity, newInstancesTransform, true /*world space*/, true, false);
		//		}

		//		//MeshComponents[iMesh]->BatchUpdateInstancesTransforms(startIndex, newInstancesTransforms, true /*world space*/, true, false);
		//	}
		//}
			   		 	  	  	   	
		TaskContext.simuData = &simuData;
		TaskContext.frameData = &frameData;
		TaskContext.crowdUnitFactor = crowdUnitFactor;		
		TaskContext.crowdFieldIndex = crowdFieldIndex;
		TaskContext.MeshIndexToProcess = 0; // reset atomic counter
		TaskContext.CharacterInstanced = this;
			   
		const bool bExecuteInParallel = FApp::ShouldUseThreadingForPerformance();
		if (bExecuteInParallel)
		{
			FGraphEventArray CompletionEvents;

			int coreCount = FPlatformMisc::NumberOfCores();
			for (int iTask = 0; iTask < coreCount; iTask++)
			{
				CompletionEvents.Add(TGraphTask<FUpdateInstanceBatchTransformTask>::CreateTask(
					nullptr, ENamedThreads::GameThread/*, ENamedThreads::GetRenderThread()*/).ConstructAndDispatchWhenReady(TaskContext));
			}

			// then wait for completion
			for (int iTask = 0; iTask < coreCount; iTask++)
			{
				if (CompletionEvents[iTask].IsValid())
				{
					// Need to wait on GetRenderThread_Local, as mesh pass setup task can wait on rendering thread inside InitResourceFromPossiblyParallelRendering().
					//QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitForMeshPassSetupTask);
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompletionEvents[iTask]);// , ENamedThreads::GetRenderThread_Local());
				}
			}
		}
		else
		{
			FUpdateInstanceBatchTransformTask Task(TaskContext);
			FGraphEventRef completionEvent;
			Task.DoTask(ENamedThreads::GameThread, completionEvent);
		}
		MarkPackageDirty();

		// end update transform now in game thread
		for (int iChild = 0; iChild < MeshComponents.Num(); iChild++)
		{
            if (MeshComponents[iChild] != nullptr)
            {
                //MeshComponents[iChild]->UpdateTree(); replaced by
                MeshComponents[iChild]->BuildTreeIfOutdated(true, false); 
		}
	}
}
}

//-------------------------------------------------------------------------
void AGolaemCharacterInstanced::BeginPlay()
{
	Super::BeginPlay();
    //ASkeletalMeshActor::BeginPlay();

    //// restore meshes to hide or show
    //if (MeshesToKeep.Num() > 0)
    //{
    //    MeshSelectionByMaterialSection(MeshesToKeep);
    //}
}