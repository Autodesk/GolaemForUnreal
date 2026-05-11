/***************************************************************************
 *                                                                          *
 *  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
 *                                                                          *
 ***************************************************************************/

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "GameFramework/Actor.h"
#include "GlmCacheTypes.h"
#include "GolaemSimulationInterface.h"
#include "Engine/SkeletalMesh.h"

#include "Runtime/Json/Public/Dom/JsonObject.h"
#include "Runtime/Engine/Classes/Animation/AnimInstance.h"
#include "MeshDescription.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/ObjectSaveContext.h"
#include "HAL/RunnableThread.h"

// #include "stdint.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "glmCrowdIO.h"
#include "glmSet.h"
#include "glmArray.h"
#include "glmString.h"
#include "glmVector3.h"
#include "glmPODArray.h"
#include "glmAssetManagementUtils.h"
#include "glmMutex.h"
#include "glmSimulationCacheFactory.h"
#include "glmGeometryFile.h"
#include "glmGolaemCharacter.h"
#include "glmMutex.h"
THIRD_PARTY_INCLUDES_END

#include "GolaemCache.generated.h" //needs to be included last

// predeclaration
class AGolaemSimulation;
class AGolaemCharacter;
class AGolaemCharacterInstanced;
class ALandscape;
class ALandscapeProxy;
// class FCachePreloadWorker;

USTRUCT()
struct FGolaemRefresh
{
    GENERATED_BODY()

    bool DummyValue;
};

class IGolaemCacheEditor
{
public:
    virtual FVector GetPivotLocation() = 0;
    virtual FString GetSelectedEntities(const FString& golaemCacheName) = 0;
    virtual void RegisterPreSaveWorld(AGolaemCache* golaemCache) = 0;
    virtual void UnregisterPreSaveWorld(AGolaemCache* golaemCache) = 0;
    virtual void CreateUniqueAssetName(FString&, FString&) = 0;
    virtual void AddSelectionCallBack(AGolaemCharacter& character) = 0;
    virtual void Expand(AGolaemCache* golaemCache, bool expand) = 0;
};

// simplistic class that register an editor factory or not
class CROWDMODULE_API GolaemCacheEditorWrapper
{
public:
    GolaemCacheEditorWrapper();
    ~GolaemCacheEditorWrapper();
    void registerGolaemCacheEditor(IGolaemCacheEditor* theGolaemCacheEditor);
    void unregisterGolaemCacheEditor();

    IGolaemCacheEditor* getWrapper();

private:
    IGolaemCacheEditor* _golaemCacheEditor;
};

class AGolaemCharacterInstanced;
// class FCachePreloadWorker;

UENUM(BlueprintType)
enum class ETerrainAdaptationMode : uint8
{
    NONE UMETA(DisplayName = "None"),
    SNAP_HEIGHT UMETA(DisplayName = "Snap Height"),
    SNAP_HEIGHT_AND_ORI UMETA(DisplayName = "Snap Height and Ori"),
    WITH_IK UMETA(DisplayName = "With IK"),
    Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EInstanceMode : uint8
{
    DISABLED UMETA(DisplayName = "Disabled"),
    FOR_RIGID_MESHES UMETA(DisplayName = "For Rigid Meshes"),
    FORCED UMETA(DisplayName = "Forced"),
    Count UMETA(Hidden)
};

// per template character data
struct TemplateCharacterData
{
    glm::GolaemCharacter* _glmInCharacter;
    TArray<int32> _boneHierarchy;
};

class FPerSimData
{

public:
    FPerSimData()
    {
    }

    ~FPerSimData()
    {
        AllChars.Empty();

        MeshIndicesInUnrealPerCharacter.clear();
        MorphTargetsToBlindDataIndexPerChar.clear();
    }

    // All created/reloaded characters for this sim, use SIMULATION / CACHE index not asset one
    TArray<TWeakObjectPtr<AGolaemCharacter>> AllChars;

    glm::Array<glm::Array<glm::PODArray<size_t>>> MeshIndicesInUnrealPerCharacter;

    // morph target blind data indices, computed once for all at simu init
    glm::Array<glm::PODArray<int32>> MorphTargetsToBlindDataIndexPerChar;

    glm::GlmSet<int> ShownCrowdEntityIds; // need this to show hide instanced too (if in instance only mode)

    TArray<bool> PreviousEnabledChars;
};

UCLASS()
class CROWDMODULE_API AGolaemCache : public AActor, public IGolaemSimulationInterface
{
    GENERATED_BODY()

    friend class AGolaemSimulation;

public:
    // friend class FCachePreloadWorker;

    class FCrowdFieldData : public FPerSimData
    {

    public:
        FCrowdFieldData()
            : _cachedSimulation(NULL)
        {
        }

        virtual ~FCrowdFieldData()
        {
            if (_cachedSimulation != NULL)
            {
                _cachedSimulation->clear(glm::crowdio::FactoryClearMode::ALL, UINT32_MAX);
            }
        }

        FString GlmFileRoot;

        glm::crowdio::CachedSimulation* _cachedSimulation;
    };

    AGolaemCache(const FObjectInitializer& ObjectInitializer);

    virtual ~AGolaemCache();

    void OnPreSaveWorld(UWorld* InWorld, FObjectPreSaveContext ObjectSaveContext);

    // virtual void BeginDestroy() override;
#if WITH_EDITOR
    virtual void SetIsTemporarilyHiddenInEditor(bool bIsHidden) override;
#endif

    bool GetBoneSpaceTransforms_AnyThread(int simulationIndex, int entityIndex, TArray<FTransform>& boneSpaceTransforms, TArray<float>& morphTargetWeights, float& entityCurrentTime) override;

    // tick from sequencer
    void TickAtThisTime(const float Time, bool bInBackwards, bool bInIsLooping);

    void UpdateTransforms_AnyThread(AGolaemCharacterInstanced* characterInstanced);

    void SetManualTick(bool bInManualTick)
    {
        bManualTick = bInManualTick;
    }
    bool GetManualTick() const
    {
        return bManualTick;
    }

    float GetDuration() const
    {
        return (EndFrame - StartFrame) / FrameRateOverride;
    }

    int getEditedLayoutIndex() const;
    FString getEditedLayoutName(int layoutIndex = -1) const;
    void RefreshCache(); // force refresh of cache and its characters
    void RefreshTerrains();
#if WITH_EDITOR
    static FString ExportTerrain(TArray<AActor*>& landscapeProxies);
#endif

    glm::Array<glm::GlmString>& GetLayoutFilesParsed()
    {
        return LayoutFilesParsed;
    }

    void SetSelectEntitySet(glm::GlmSet<int64>& crowdEntityIdSet);
    void AppendSelectEntitySet(glm::GlmSet<int64>& crowdEntityIdSet);

#if WITH_EDITOR
    bool TryGetTransformLock();
    void ReleaseTransformLock();
#endif

    const glm::GlmSet<int64>& GetSelectedEntitiesInt() const;
    void SelectEntity(int64 crowdEntityId);
    void UnselectEntity(int64 crowdEntityId);

    // entityId is used to apply the move / scale / rotate only once : apply on the first entity of selection only
    void TranslateSelection(glm::Vector3& translate);
    void RotateSelection(glm::Vector3& glmEulerRotation);
    void ScaleSelection(float scale);
    void ScaleSelection(float scale, glm::Vector3& pivot);

#if WITH_EDITOR
    void CheckLayoutEditor();
    void OpenLayoutFile(int layoutIndex = -1, bool forceFocus = false);   // open the layout file if any, or open an empty tab
    void ReloadLayoutFile(int layoutIndex = -1, bool forceFocus = false); // open the current layout file if any
    void OpenNewLayoutFile(int layoutIndex = -1);                         // open a new tab to edit current transfo when no layout exists
    void refreshLayoutFromEditor();
    void addOrEditLayoutTransformation(const glm::PODArray<int64_t>& entitiesToTransform, const char* transformationNodeTypeName, const char* parameterName, float parameterValue, float frame, int operationMode);
    void addOrEditLayoutTransformation(const glm::PODArray<int64_t>& entitiesToTransform, const char* transformationNodeTypeName, const char* parameterName, const glm::Vector3& parameterValue, float frame, int operationMode);
    void editLayoutParameter(const char* parameterName, int parameterValue);
    void SaveLayouts();

    UFUNCTION(BlueprintCallable, Category = "Layout")
    void ReloadLayout(int layoutIndex, const FString& layoutJSON, const TArray<int>& dirtyLayoutNodeID, bool doRefreshAssets);

    UFUNCTION(BlueprintCallable, Category = "Layout")
    FString GetSelectedEntities() const;

#endif

    static GolaemCacheEditorWrapper* GetGolaemCacheEditorWrapper();

    // Variables serialized and exposed in the editor, configuration
    UPROPERTY(EditAnywhere, Category = "Common")
    bool bEnableSimulation = true;

    UPROPERTY(EditAnywhere, Category = "Common")
    EInstanceMode InstanceMode = EInstanceMode::FOR_RIGID_MESHES;

    // UPROPERTY(EditAnywhere, Category = "Common", meta = (FilePathFilter = "gscb"))
    // FFilePath CacheLibraryFile;

    // UPROPERTY(EditAnywhere, Category = "Common")
    // int CacheIndex = 99999;

    UPROPERTY(EditAnywhere, Category = "Simulation Display", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
    float DrawEntityPercent = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Time")
    float CurrentFrame = 2;

    // UPROPERTY(EditAnywhere, Category = "Preload")
    // int FramesToPreload = 2;

    UPROPERTY(EditAnywhere, Category = "Simulation Cache")
    int StartFrame;

    UPROPERTY(EditAnywhere, Category = "Simulation Cache")
    int EndFrame;

    UPROPERTY(VisibleAnywhere, Category = "Simulation Cache")
    int FrameRate;

    UPROPERTY(EditAnywhere, Category = "Simulation Cache")
    float FrameRateOverride;

    UPROPERTY(EditAnywhere, Category = "Simulation Cache")
    TArray<FString> CrowdFields;

    UPROPERTY(EditAnywhere, Category = "Simulation Cache")
    FString CacheName;

    UPROPERTY(EditAnywhere, Category = "Simulation Cache")
    FString CacheDirectory;

    UPROPERTY(EditAnywhere, Category = "Simulation Cache")
    FString CharactersFiles;

    // must specify the SkeletalMesh to use for each character, either with a single mesh section (probably a skeleton to merge), or a multipleSection one (meshes are then selected by showMaterialSection)
    UPROPERTY(EditAnywhere, Category = "Simulation Cache")
    // TArray<TWeakObjectPtr<USkeletalMesh>> InCharacters;
    TArray<USkeletalMesh*> InCharacters;

    // user can disable the layout here
    // result of loading, visible for user
    UPROPERTY(EditAnywhere, Category = "Simulation Layout")
    FGolaemRefresh RefreshButton;

    UPROPERTY(EditAnywhere, Category = "Simulation Layout")
    bool bEnableLayout = true;

    UPROPERTY(EditAnywhere, Category = "Simulation Layout")
    FString LayoutFile;

    UPROPERTY(EditAnywhere, Category = "Simulation Layout Terrain")
    ETerrainAdaptationMode TerrainAdaptationMode;

    // The Source Terrain File (must be GTG Format)
    UPROPERTY(EditAnywhere, Category = "Simulation Layout Terrain")
    FFilePath TerrainFileSource;

    // The Destination Terrain File (must be GTG Format, you can use export to get it)
    UPROPERTY(EditAnywhere, Category = "Simulation Layout Terrain")
    FFilePath TerrainFileDest;

    // TArray<TWeakObjectPtr<AGolaemCharacter>> AllCharacterActors;                   // to be able to clean up before reset, does not match any special order.
    TArray<TWeakObjectPtr<AGolaemCharacterInstanced>> AllInstancedCharacterActors; // instancing characters, holding all the instances

    FDelegateHandle PreSaveWorldHandle;

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
    virtual void PreInitializeComponents() override;
    virtual void PostInitializeComponents() override;
    // Called every frame
    virtual void Tick(float DeltaTime) override;
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // void OnSelectionChanged(UObject* NewSelection);

    void CleanUpEverything();

    void RefreshCharacters();
    void RefreshInstancedCharacters();

#if WITH_EDITOR
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& e) override;
#endif
    void InitSimuIfEnoughChars(bool createCharacters);

    // glm::SpinLock CacheCurrentFrameLoadingLock; // lock during the loading of the current frame, which is blocking the update process

    static void ShowEntityPercentInternal(
        float DrawEntityPercent,
        const glm::crowdio::GlmSimulationData* SimulationData,
        FPerSimData& PerSimData,
        TArray<TWeakObjectPtr<AGolaemCharacter>>& CharactersToProcess,
        TArray<TWeakObjectPtr<AGolaemCharacterInstanced>>& AllInstancedCharacters);

    static void GetMeshIndicesInUnrealPerCharacter(
        const USkeletalMesh* InCharacter,
        const glm::GolaemCharacter* glmChar,
        FPerSimData& PerSimData,
        int characterIndex);

    static void SpawnActor(
        IGolaemSimulationInterface* GolaemSimulationInterface,
        AActor* ParentActor,
        EInstanceMode InstanceMode,
        int ThisEntityCharacterIndex,
        const glm::PODArray<int>& EntityAssets,
        const glm::crowdio::GlmSimulationData* SimData,
        const glm::crowdio::GlmFrameData* FrameData,
        int SimDataEntityIdx,
        int SimCrowdFieldIndex,
        USkeletalMesh* InCharacter,
        float CrowdUnitFactor,
        FPerSimData& PerSimData,
        TArray<TemplateCharacterData>& TemplateCharactersData,
        UWorld* World,
        TArray<TWeakObjectPtr<AGolaemCharacterInstanced>>& AllInstancedCharacterActors);

    static void ApplySimuShaderData(
        FPerSimData& PerSimData,
        const glm::crowdio::GlmSimulationData* SimuData,
        const glm::ShaderAssetDataContainer* ShaderDataContainer,
        const glm::PODArray<glm::GolaemCharacter*>& GolaemCharacters);

    static void ApplyFrameShaderData(
        FPerSimData& PerSimData,
        const glm::ShaderAssetDataContainer* shaderDataContainer,
        const glm::PODArray<glm::GolaemCharacter*>& GolaemCharacters);

private:
    void ResetSimulation(/*TArray<class AGolaemCharacter*> chars*/);
    void ResetCharacterActors();
    void ResetCharacterActorsInstanced();
    void ShowEntityPercent(int CrowdFieldIndex, bool locked = true);
    void InitSimulation(bool createCharacters);
    // void PreloadFrame(int frame);
    // const glm::crowdio::GlmFrameData* GetOrWaitPreloadedFrame(int crowdFieldIndex);
    void LoadFrameData(int CrowdFieldIndex, bool bInitSimuShader = false, bool localLock = false);
    void LoadFrameDataAllCrowdFields(bool bInitSimuShader = false, bool localLock = false);
    // void releaseFinalFrameDataBefore(int frame);
    void SwapReadWriteFrameData(int CrowdFieldIndex);
    void AnimateCharacterComp(int CrowdFieldIndex);
    void ApplySimuShaderData(int CrowdFieldIndex);
    void ApplyFrameShaderData(int CrowdFieldIndex);
    void UpdateSimuFrameData();
    void InvalidateCharactersFrame();
    bool FillInCharactersFromFactory(bool defaultToFirst = false);

    void ApplyExcludedEntities(int CrowdFieldIndex);

    // UPROPERTY()
    bool bManualTick;

    // UAnimBlueprintGeneratedClass* GolaemAnimBlueprint;
    FQuat QuatCacheToUE4; // consider it static, for conversion

    bool bIsCacheSetup;
    bool bIsSimuBuilt;

    bool bResetInstanced; // set if reloaded from file to reset GolaemCharacterInstanced

    // scale and unit relative

    void SetSceneUnit();
    static float GetSceneUnitFactor();

    // per template character data
    TArray<TemplateCharacterData> TemplateCharactersData;

    // Per CrowdField data
    TArray<FCrowdFieldData> DataPerCF;

    glm::Array<glm::GlmString> _meshes;

    float _crowdUnitFactor;
    glm::crowdio::SimulationCacheFactory* _factory;
    glm::crowdio::crowdTerrain::TerrainMesh* _terrainMeshSource;
    glm::crowdio::crowdTerrain::TerrainMesh* _terrainMeshDestination;
    //   glm::crowdio::GlmFileMesh* _meshesToExport;
    // glm::crowdio::GlmFileMeshTransform* _transformsToExport;

    // store time for play in editor (not used in sequencer)
    float TimeFromBeginPlay;

    glm::RWSpinLock CacheRWLock;

    glm::GlmSet<int64> SelectedCrowdIds;

    glm::PODArray<int64_t> CommandIDs; // temporary usage when sending commands

    glm::Array<glm::GlmString> LayoutFilesParsed;

    static FMatrix MatUEToGda;

    glm::GlmString CacheDirectoryDirmapped;

    bool CharactersHaveBeenReset;

    // Would be needed to compute extents, but AActor extent is only computed from its children compenents...
    // FVector MaxCharExtent;
    // FBox Bounds;

    // FCachePreloadWorker* PreloadCacheWorker;

    // glm::GlmSet<int> PreloadedFrames;

    glm::Mutex TransformLock;
};