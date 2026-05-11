/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "GameFramework/Actor.h"
#include "Animation/SkeletalMeshActor.h"
//#include "stdint.h"

#include "GolaemCharacter.generated.h" //needs to be included last

struct FAnimNode_GolaemPose;

USTRUCT()
struct FDynamicMaterialInfo
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    int MaterialIndex;

    UPROPERTY()
    int ScalarParameterIndex;

    UPROPERTY()
    int VectorParameterIndex;

    UPROPERTY()
    int shaderAttrIndex;
};

UCLASS()
class CROWDMODULE_API AGolaemCharacter : public ASkeletalMeshActor
{
    GENERATED_BODY()

    friend class AGolaemCache;
    friend class AGolaemSimulation;
    friend struct FAnimNode_GolaemPose;

public:
    AGolaemCharacter(const FObjectInitializer& ObjectInitializer);

    // AActor inherited
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostLoad() override;
    //virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    void AddShaderScalarParameter(int matIndex, int paramIndex, int shaderAttrIndex);
    void AddShaderVectorParameter(int matIndex, int paramIndex, int shaderAttrIndex);

    // any SkeletalMesh holding the proper skeleton as input
    //UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakPtr< USkeletalMesh > MasterSkel; // holding the input skeleton. Just for visibility, characters are rebuilt at load

    // properties information let to user
    UPROPERTY(VisibleAnywhere, Category = "GolaemCharacter")
    int CrowdEntityId; // holding the input skeleton. used for visibility, characters are rebuilt at load

    UPROPERTY()
    int CrowdFieldIndex; // entity crowdField in its cache
    UPROPERTY()
    int CacheIndex; // entity cache index in its cache
    UPROPERTY()
    int EntityType; // entity type in its cache
    UPROPERTY()
    int CharacterIndex; // entity character index to use in cache character list
    UPROPERTY()
    TArray<int> MeshesToKeep; // LODInfo are not serialized (transient), we need to keep track of which materials to disable, done at OnConstruction
    UPROPERTY()
    TArray<FDynamicMaterialInfo> DynamicMaterials;
    UPROPERTY()
    TArray<bool> RigidSections;
    //UPROPERTY() int CurrentFrame; // to know if the character has already been updated for this frame

    UPROPERTY(EditAnywhere, Category = "GolaemCharacter")
    bool bMorphTargetFromAnim = false;

    //void MeshSelectionByMerge(const FString& DirPath, const TArray<FString>& MeshNames); // mesh selection in merge mode way : get all resources by name convention (risky), no need to be blueprint callable
    void MeshSelectionByMaterialSection(const TArray<int>& MeshAssetsToKeep); // mesh selection in direct skeletalMesh mode way : get all resources by name convention (risky)

    //void GetActorBounds(bool bOnlyCollidingComponents, FVector& Origin, FVector& BoxExtent, bool bIncludeFromChildActors = false) const;
    
#if WITH_EDITOR
    void OnSelectionChanged(UObject* Object);

    virtual void EditorApplyTranslation(
        const FVector& DeltaTranslation,
        bool bAltDown,
        bool bShiftDown,
        bool bCtrlDown) override;

    virtual void EditorApplyScale(
        const FVector& DeltaScale,
        const FVector* PivotLocation,
        bool bAltDown,
        bool bShiftDown,
        bool bCtrlDown) override;

    virtual void EditorApplyRotation(
        const FRotator& DeltaRotation,
        bool bAltDown,
        bool bShiftDown,
        bool bCtrlDown) override;

    virtual void PostEditMove(bool bFinished) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
#endif

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    //void OnClicked() override;
    //void OnReleased() override;

    // Runtime information
    TArray<FTransform> BonesSpaceTransforms;
    TArray<float> MorphTargetWeights;
    float CurrentTime;    // reset at reload, to force a new fetching of pose from cache without having to serialize the whole current pose in the scene
    bool bShownByPercent = true; // to be able to get back the show / hide when entity becomes valid
    bool bEnabledInCache; // to be able to get back the show / hide when entity becomes valid

    FVector CumulativeDeltaTranslation;
    FRotator CumulativeDeltaRotation;
    FVector CumulativeDeltaScale;
    bool bUseScalePivot;
    FVector CumulativeScalePivot; // overriden at each call, should stay the same during an edit

    bool bSelectedInCache;

    bool bHasTransformLock;
};