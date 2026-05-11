/***************************************************************************
 *                                                                          *
 *  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
 *                                                                          *
 ***************************************************************************/

#pragma once

#include "GameFramework/Actor.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Engine/UserDefinedStruct.h"
#include "TimerManager.h"

#include "GlmCacheTypes.h"

#include "GolaemCache.h" // for enums
#include "GolaemSimulationInterface.h"
#include "GDAProperty.h"

// #include "stdint.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <glmCrowdIO.h>
#include <glmArray.h>
#include <glmMutex.h>
#include <glmRandomNumberGenerator.h>
#include <glmString.h>
#include <glmGolaemCharacter.h>
THIRD_PARTY_INCLUDES_END

#include "GolaemSimulation.generated.h" //needs to be included last

// predeclaration
class AGolaemCharacter;
class AGolaemCharacterInstanced;
class ALandscape;
class ALandscapeProxy;
class USkeletalMesh;

namespace glm
{
    class UnrealObjectsWrapper;

    namespace engine
    {
        class Engine;
        class EngineSimulation;
        class GDAAttribute;
        class GDAContainer;
    } // namespace engine

} // namespace glm

// Play a Golaem Simulation from a Golaem Digital Asset (gda) file
UCLASS(BlueprintType)
class CROWDMODULE_API AGolaemSimulation : public AActor, public IGolaemSimulationInterface
{
    friend class AGolaemCharacterInstanced;
    friend class AGolaemCharacter;

    GENERATED_BODY()

public:
    class FSimulationData : public FPerSimData
    {

    public:
        FSimulationData()
        {
        }

        virtual ~FSimulationData()
        {
        }

        // runtime data
        glm::GlmMap<int, USkeletalMesh*> EtSkeletalMeshes;
    };

    AGolaemSimulation(const FObjectInitializer& ObjectInitializer);

    virtual ~AGolaemSimulation();

#if WITH_EDITOR
    virtual void SetIsTemporarilyHiddenInEditor(bool bIsHidden) override;
#endif

    bool GetBoneSpaceTransforms_AnyThread(int simulationIndex, int entityIndex, TArray<FTransform>& boneSpaceTransforms, TArray<float>& morphTargetWeights, float& entityCurrentTime) override;

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    UGDAProperty* FindGdaPropertyByFullName(const FString& fullName);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    UGDAProperty* FindGdaPropertyByShortName(const FString& shortName);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    void OverridePoptoolFromMesh(const FString& poptoolName, AStaticMeshActor* mesh);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    float gdaToUnrealFloat(const float& gdaValue, GDAPropertyDataType::Type dataType = GDAPropertyDataType::Distance);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    FVector gdaToUnrealVector3(const FVector& gdaValue, GDAPropertyDataType::Type dataType = GDAPropertyDataType::Distance);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    FVector4 gdaToUnrealVector4(const FVector4& gdaValue, GDAPropertyDataType::Type dataType = GDAPropertyDataType::Distance);

    static void gdaToUnreal(
        const float& gdaValue,
        float& unrealValue,
        float crowdUnitFactor,
        GDAPropertyDataType::Type dataType);

    static void gdaToUnreal(
        const glm::Vector3& gdaValue,
        FVector& unrealValue,
        float crowdUnitFactor,
        GDAPropertyDataType::Type dataType);

    static void gdaToUnreal(
        const glm::Vector4& gdaValue,
        FVector4& unrealValue,
        float crowdUnitFactor,
        GDAPropertyDataType::Type dataType);

    static void unrealToGda(
        const float& unrealValue,
        float& gdaValue,
        float crowdUnitFactor,
        GDAPropertyDataType::Type dataType);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    float unrealToGdaFloat(const float& unrealValue, GDAPropertyDataType::Type dataType = GDAPropertyDataType::Distance);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    FVector unrealToGdaVector3(const FVector& unrealValue, GDAPropertyDataType::Type dataType = GDAPropertyDataType::Distance);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    FVector4 unrealToGdaVector4(const FVector4& unrealValue, GDAPropertyDataType::Type dataType = GDAPropertyDataType::Distance);

    static void unrealToGda(
        const float& unrealValue,
        double& gdaValue,
        float crowdUnitFactor,
        GDAPropertyDataType::Type dataType);

    static void unrealToGda(
        const FVector& unrealValue,
        glm::Vector3& gdaValue,
        float crowdUnitFactor,
        GDAPropertyDataType::Type dataType);

    static void unrealToGda(
        const FVector4& unrealValue,
        glm::Vector4& gdaValue,
        float crowdUnitFactor,
        GDAPropertyDataType::Type dataType);

    void ReloadSimuFromFile();

    void RefreshGdaFile();

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyInt32(const FString& shortName, int32 value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyInt64(const FString& shortName, int64 value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyFloat(const FString& shortName, float value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyVector3(const FString& shortName, const FVector& value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyVector4(const FString& shortName, const FVector4& value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyString(const FString& shortName, const FString& value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyMesh(const FString& shortName, AStaticMeshActor* value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyInt32Array(const FString& shortName, const TArray<int32>& value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyInt64Array(const FString& shortName, const TArray<int64>& value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyFloatArray(const FString& shortName, const TArray<float>& value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyVector3Array(const FString& shortName, const TArray<FVector>& value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyVector4Array(const FString& shortName, const TArray<FVector4>& value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyStringArray(const FString& shortName, const TArray<FString>& value);

    UFUNCTION(BlueprintCallable, Category = "GDA Utils")
    bool SetGDAPropertyMeshArray(const FString& shortName, const TArray<AStaticMeshActor*>& value);

    // Variables serialized and exposed in the editor, configuration
    UPROPERTY(EditAnywhere, Category = "Common")
    bool bEnableSimulation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common", meta = (FilePathFilter = "gda"))
    FFilePath GDAFile;

    UPROPERTY(EditAnywhere, Category = "Common")
    bool bRefreshGDAOnLoad = false;

    UPROPERTY(EditAnywhere, Category = "Common")
    bool bSimulationPreview = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common")
    TArray<UGDAProperty*> GDAProperties;

    UPROPERTY(EditAnywhere, Category = "Simulation Display", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
    float DrawEntityPercent = 100.0f;

    UPROPERTY(VisibleAnywhere, Category = "Time")
    float CurrentTime = 0;

    UPROPERTY(VisibleAnywhere, Category = "Time")
    int CurrentFrame = 0;

    UPROPERTY(EditAnywhere, Category = "Simulation Cache")
    bool bExportSimulationCache = false;

    UPROPERTY(EditAnywhere, Category = "Simulation Cache", meta = (EditCondition = "bExportSimulationCache"))
    FDirectoryPath SimulationCacheDir;

    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common")
    float _crowdUnitFactor = 100.f;

    UPROPERTY(EditAnywhere, Category = "Common")
    EInstanceMode InstanceMode = EInstanceMode::FOR_RIGID_MESHES;

protected:
    // AActor and parent's Methods
    /**
     * The functions of interest to initialization order for an Actor is roughly as follows:
     * PostLoad/PostActorCreated - Do any setup of the actor required for construction. PostLoad for serialized actors, PostActorCreated for spawned.
     * AActor::OnConstruction - The construction of the actor, this is where Blueprint actors have their components created and blueprint variables are initialized
     * AActor::PreInitializeComponents - Called before InitializeComponent is called on the actor's components
     * UActorComponent::InitializeComponent - Each component in the actor's components array gets an initialize call (if bWantsInitializeComponent is true for that component)
     * AActor::PostInitializeComponents - Called after the actor's components have been initialized
     * AActor::BeginPlay - Called when the level is started
     */

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Destroyed() override;
    virtual void PostLoad() override;
    virtual void PostActorCreated() override;
    virtual void PostRegisterAllComponents() override;

    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Called when the game ends
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& e) override;
#endif
    void InitSimuIfEnoughChars();

    virtual bool ShouldTickIfViewportsOnly() const override
    {
        return bSimuDirty || bForceTick; // only update in editor when something needs to update
    }

    void RefreshInstancedCharacters();

    void UpdateTransforms_AnyThread(AGolaemCharacterInstanced* characterInstanced);

private:
    void ResetSimulation();
    void ShowEntityPercent(int simulationIndex, bool locked = true);
    void ResetCharacterActors();
    void InitSimulation();
    void LoadFrameData(int simulationIndex, bool localLock = false);
    void LoadFrameDataAllSimulations(bool localLock = false);
    void AnimateCharacterComp(int CrowdFieldIndex);

    void UpdateNewAndKilledEntities();

    // license
    FString ComputeLicenseStatusStr() const;

    FQuat QuatCacheToUE4; // consider it static, for conversion

    // scale and unit relative

    void SetSceneUnit();

    bool LoadGDA();
    UGDAProperty* CreateGdaPropertyFromAttribute(const glm::engine::GDAAttribute* attribute, const glm::engine::GDAContainer& container);
    void UpdateGdaPublicAttributes();
    void OverrideGDAAttributes();

    void ApplySimuShaderData(int simulationIndex);
    void ApplyFrameShaderData(int simulationIndex);

    void SetupSimulationPreview();

    TArray<TemplateCharacterData> TemplateCharactersData;

    glm::engine::Engine* _glmEngine = NULL;

    glm::UnrealObjectsWrapper* _objectsWrapper = NULL;

    // Per Simulation data
    TArray<FSimulationData> DataPerSimulation;

    glm::RandomNumberGenerator _rng;

    glm::RWSpinLock CacheRWLock;

    bool bSimuDirty = true;
    bool bForceTick = true;

    TArray<TWeakObjectPtr<AGolaemCharacterInstanced>> AllInstancedCharacterActors;
};