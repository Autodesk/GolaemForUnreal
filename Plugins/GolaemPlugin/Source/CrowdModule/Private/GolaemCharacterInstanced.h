/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "GameFramework/Actor.h"
//#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "RawMesh.h"
#include "SkeletalRenderPublic.h"
#include "GolaemHISMeshComponent.h"
#include "GolaemCache.h"
#include "GolaemChUpdTransformTask.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "glmSet.h"
#include "glmGeometryFile.h"
#include "glmFrameData.h"
#include "glmSimulationData.h"
#include "glmMutex.h"
THIRD_PARTY_INCLUDES_END

//#include "stdint.h"

#include "GolaemCharacterInstanced.generated.h" //needs to be included last

namespace glm
{
	namespace crowdio
	{
		class SimulationCacheFactory;
	}
}

UCLASS()
class CROWDMODULE_API AGolaemCharacterInstanced : public AActor
{
	GENERATED_BODY()

	friend class FUpdateInstanceBatchTransformTask;	

public:
	AGolaemCharacterInstanced(const FObjectInitializer& ObjectInitializer);

	// AActor inherited
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;
	//virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	//virtual void PostEditChangeProperty(FPropertyChangedEvent& e) override;
    
	//void AddShaderScalarParameter(int matIndex, int paramIndex, int shaderAttrIndex);
 //   void AddShaderVectorParameter(int matIndex, int paramIndex, int shaderAttrIndex);
	
	// any SkeletalMesh holding the proper skeleton as input
	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakPtr< USkeletalMesh > MasterSkel; // holding the input skeleton. Just for visibility, characters are rebuilt at load

	// properties information let to user
	//UPROPERTY(VisibleAnywhere, Category = "GolaemCharacterInstanced")
	//int CrowdEntityId; // holding the input skeleton. used for visibility, characters are rebuilt at load

	//UPROPERTY() int CrowdFieldIndex; // entity cache index in its cache
	//UPROPERTY() int CacheIndex; // entity cache index in its cache
	//UPROPERTY() int EntityType; // entity type in its cache
	//UPROPERTY() int CharacterIndex; // entity character index to use in cache character list
	//UPROPERTY()	TArray<int> MeshesToKeep; // LODInfo are not serialized (transient), we need to keep track of which materials to disable, done at OnConstruction
	//UPROPERTY() TArray<FDynamicMaterialInfo> DynamicMaterials;
	//UPROPERTY() int CurrentFrame; // to know if the character has already been updated for this frame
	
	/** The skeletal mesh used by Actor to build up its static mesh components */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	USkeletalMesh* InSkeletalMesh;
	//TWeakObjectPtr<USkeletalMesh> InSkeletalMesh;

	//UPROPERTY(VisibleAnywhere, Category = "Mesh")
	//TArray< int32 > rigidBonePerMeshAsset; // the rigid bone to fetch transform from (shared for all instances of a mesh, one per HISMC)

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Instanced Meshes")
	TArray< int > GchaToCacheBones; // needed to reorder cache transforms relative to rigid bone index

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Instanced Meshes")
    TArray<bool> RigidSections;

	/** Creates Hierarchical Instanced Static Mesh Components, one per skeletal mesh section */
	//FUNCTION(BlueprintCallable, Category = "Components|SkinnedMesh")
	

	//void MeshSelectionByMerge(const FString& DirPath, const TArray<FString>& MeshNames); // mesh selection in merge mode way : get all resources by name convention (risky), no need to be blueprint callable
	//void MeshSelectionByMaterialSection(const TArray<int>& MeshAssetsToKeep); // mesh selection in direct skeletalMesh mode way : get all resources by name convention (risky)

	//void ConstructFrom(glm::crowdio::GlmGeometryFile gcgFile); -> bad as it must be constructed from Unreal assets
	void AddInstance(const glm::crowdio::GlmSimulationData* simuData, int32 crowdFieldIndex, const TArray<int>& MeshAssetsToKeep, uint32_t entityIndexInCache); // apply asset variation by only adding instances on used meshes
	void UpdateTransforms(const glm::crowdio::GlmSimulationData& simuData, const glm::crowdio::GlmFrameData& frameData, int32 crowdFieldIndex, /*glm::crowdio::SimulationCacheFactory* factory,*/ float CurrentFrame, float crowdUnitFactor);

	void RemoveInstances();

#if WITH_EDITOR
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
    virtual void UpdateSkeletalMesh(EInstanceMode InstanceMode);
#endif
    void SetCharacterToCacheBones(const glm::GolaemCharacter* glmCharacter);

	int GetCurrentFrame()
    {
        return CurrentFrame;
    }

	virtual bool ShouldTickIfViewportsOnly() const override
    {
        return true; // important to get animation update on instances when editing
    }

	void setTransformDirty()
    {
        bTransformDirty = true;
    }

	void SetShownInstances(const glm::GlmSet<int>& shownIDs, const glm::GlmSet<int>& hiddenIds);

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;
	
protected:
	// tick from sequencer
	void TickAtThisTime(const float Time, bool bInBackwards, bool bInIsLooping);

	void UpdateTransforms();

#if WITH_EDITOR
	// returns if the section is rigid or not
	bool SkinnedMeshSectionToRawMesh(
		TArray< TArray<FFinalSkinVertex> >& cpuSkinnedVerticesPerLOD, 
		USkeletalMeshComponent* InSkeletalMesh,
		int32 section,
		const FMatrix& InComponentToWorld,
		const FString& InPackageName,
		TArray<FRawMesh>& OutRawMeshes,
		TArray<UMaterialInterface*>& OutMaterials,
		int32& OutRigidBoneIndex,
        bool stopIfNotRigid = false);
#endif

	//void SkinnedMeshSectionToRawMesh(USkinnedMeshComponent* InSkinnedMeshComponent, int32 section, const FMatrix& InComponentToWorld, const FString& InPackageName, TArray<FRawMesh>& OutRawMeshes, TArray<UMaterialInterface*>& OutMaterials);
	void GetTransform(const glm::crowdio::GlmSimulationData& simuData, const glm::crowdio::GlmFrameData& frameData, uint32_t entityIndexInCache, int rigidBoneIndex, FTransform& currentTransform, float crowdUnitFactor);

	// Runtime information
	//TArray<FTransform> BonesSpaceTransforms;
	//TArray<float> MorphTargetWeights;
    float CurrentFrame; // for check with cache
	TArray<float> CurrentFramePerCF; // reset at reload, to force a new fetching of pose from cache without having to serialize the whole current pose in the scene
	//bool bShownByPercent; // to be able to get back the show / hide when entity becomes valid
	//bool bEnabledInCache; // to be able to get back the show / hide when entity becomes valid
	
	TObjectPtr<USceneComponent> RootComponent;
	TArray<TObjectPtr<UGolaemHierarchicalInstancedStaticMeshComponent>> MeshComponents; // as much as mesh assets.

	FUpdateInstanceBatchTransformTaskContext TaskContext;

	glm::SpinLock _updateTransformTaskLock;

    TArray< TBitArray<> > lastSelectedInstances;

	bool bTransformDirty = false;
    bool bWaitingForCompiling = false;
};
