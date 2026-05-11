/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "Components/MeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "UObject/UObjectGlobals.h"

THIRD_PARTY_INCLUDES_START
#include <glmAssert.h>
THIRD_PARTY_INCLUDES_END

#include "GolaemHISMeshComponent.generated.h" //needs to be included last

class AGolaemCharacterInstanced;
class AGolaemCache;

USTRUCT()
struct FCacheIndicesPerCF
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Mesh")
    TArray<int32> CacheIndices; // each entry matches an instance of the mesh, and is used to fetch the bone transform of this mesh

    UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Mesh")
    TArray<int32> InstanceIndices; // each entry matches an instance of the mesh, and is used to fetch the bone transform of this mesh
};

USTRUCT()
struct FGolaemMeshRigidInstancesData
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Mesh")
    int32 RigidBoneIndex;

    UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Mesh")
    TArray<FCacheIndicesPerCF> CacheIndicesPerCF; // each entry matches an instance of the mesh, and is used to fetch the bone transform of this mesh

    //UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Mesh")
    //TArray<int64> CrowdEntityIds;

    //TBitArray<> EnabledInstances;
};

UCLASS()
class CROWDMODULE_API UGolaemHierarchicalInstancedStaticMeshComponent : public UHierarchicalInstancedStaticMeshComponent
{

    GENERATED_UCLASS_BODY()

public:
    bool UpdateInstanceTransformWithoutTree(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false);
    void UpdateTree();

    bool IsInstanceShownByPercent(int32 InstanceIndex) const;

#if WITH_EDITOR
    virtual void NotifySMInstanceMovementStarted(const FSMInstanceId& InstanceId) override;
    virtual void NotifySMInstanceMovementOngoing(const FSMInstanceId& InstanceId) override;
    virtual void NotifySMInstanceMovementEnded(const FSMInstanceId& InstanceId) override;
    virtual void NotifySMInstanceSelectionChanged(const FSMInstanceId& InstanceId, const bool bIsSelected) override;
#endif
    //UGolaemHierarchicalInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer);

    //virtual ~UGolaemHierarchicalInstancedStaticMeshComponent();

    //UPROPERTY(VisibleAnywhere, Category = "Mesh Matching")
    //int MeshAssetIndex;

    int32 AddInstanceWithCrowdID(const FTransform& InstanceTransform, int64 crowdEntityId, bool bWorldSpace = false);

    virtual void ClearInstances() override;
    virtual bool RemoveInstance(int32 InstanceIndex) override;


    UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Instanced Meshes")
    FGolaemMeshRigidInstancesData RigidInstanceData; // each entry matches an instance of the mesh, and is used to fetch the bone transform of this mesh

    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction);

    const TArray<int64>& GetInstanceCrowdEntityIds() const
    {
        return InstanceCrowdEntityIds;
    }

    void SetIsInstanceShownByPercentArray(const TArray<int8>& IsInstanceShownByPercentArrayNew)
    {
        GLM_ASSERT(IsInstanceShownByPercentArray.Num() == IsInstanceShownByPercentArrayNew.Num());
        IsInstanceShownByPercentArray = IsInstanceShownByPercentArrayNew;
    }

    const TArray<int8>& GetIsInstanceShownByPercentArray()
    {
        return IsInstanceShownByPercentArray;
    }

private:
    AGolaemCharacterInstanced* GolaemCharacter;
    AGolaemCache* ParentGolaemCache;
    bool bHasCacheTransformLock;

    FSMInstanceId TransformedInstanceId; // UE 5 + for notify ... only (ToDo check exact version!!)
    FTransform StartTransformValue;

    UPROPERTY() // serialize to be able to select even after reload of a scene ! 
    TArray<int64> InstanceCrowdEntityIds;

    UPROPERTY() // serialize to be able to select even after reload of a scene ! 
    TArray<int8> IsInstanceShownByPercentArray; // is the entity enabled for this instance (else set scale to 0 0 0)
};
