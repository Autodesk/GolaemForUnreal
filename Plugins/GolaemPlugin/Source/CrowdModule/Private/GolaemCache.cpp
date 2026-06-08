/***************************************************************************
 *                                                                          *
 *  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
 *                                                                          *
 ***************************************************************************/

#include "GolaemCache.h" //needs to be included first

#include "Components/SceneComponent.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Misc/Paths.h"
#include "Math/Quat.h"
#include "Engine.h"
#include "Engine/World.h"
#include "Runtime/Core/Public/HAL/FileManagerGeneric.h"
#include "Runtime/Engine/Classes/Components/ActorComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "Algo/Reverse.h"
#include "Runtime/Engine/Classes/Components/TimelineComponent.h"
#include "TimerManager.h"
#include "Math/Box.h"
#include "Runtime/Engine/Public/Rendering/SkeletalMeshModel.h"
#include "Runtime/Engine/Classes/Animation/AnimSequence.h"
#include "Runtime/Engine/Classes/Components/TimelineComponent.h"
#include "Runtime/MovieScene/Public/MovieSceneSection.h"
#include "Misc/FileHelper.h"
#include "Containers/Queue.h"
#include "Animation/MorphTarget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Runtime/Landscape/Classes/Landscape.h"
#include "Runtime/Landscape/Classes/LandscapeProxy.h"
#include "Runtime/Landscape/Classes/LandscapeComponent.h"
#include "Runtime/MeshDescription/Public/MeshDescription.h"
#include "Runtime/MeshDescription/Public/MeshElementRemappings.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "StaticMeshAttributes.h"
#include "Engine/StaticMeshActor.h"
#include "MeshTypes.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/Class.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/UObjectBaseUtility.h"
#include "Engine/StaticMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"

#include "GlmCacheTypes.h"
#include "GolaemAnimInstance.h"
// #include "GolaemCachePreloadThread.h"
#include "GolaemCharacter.h"
#include "GolaemCharacterInstanced.h"
#include "GolaemUtils.h"
#include "GolaemCustomSettings.h"
#include "GolaemSkeletalMesh.h"
#include "GolaemUELogger.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <glmAssert.h>
#include <glmMathBase.h>
#include <glmStringOperators.h>
#include <glmCrowdIO.h>
#include <glmCrowdIOUtils.h>
#include <glmGolaemCharacter.h>
#include <glmRandomNumberGenerator.h>
#include <glmHistory.h>
#include <glmHistoryV0.h>
#include <glmSimulationCacheCommon.h>
#include <glmLog.h>
#include <glmGeometryFile.h>
#include "glmEulerAngles.h"
#include "glmEulerAnglesConversions.h"
#include "glmSleep.h"
#include "glmSingleton.h"
#include "glmAssetManagementUtils.h"
#include "glmUtils.h"
#include "glmStringOperators.h"
#include <glmFileName.h>
#include "glmADP.h"
THIRD_PARTY_INCLUDES_END

FMatrix AGolaemCache::MatUEToGda = FMatrix(FVector(1, 0, 0), FVector(0, 0, 1), FVector(0, 1, 0), FVector(0, 0, 0));

//-------------------------------------------------------------------------
GolaemCacheEditorWrapper::GolaemCacheEditorWrapper()
    : _golaemCacheEditor(NULL)
{
}

//-------------------------------------------------------------------------
GolaemCacheEditorWrapper::~GolaemCacheEditorWrapper()
{
}

//-------------------------------------------------------------------------
void GolaemCacheEditorWrapper::registerGolaemCacheEditor(IGolaemCacheEditor* theGolaemCacheEditor)
{
    _golaemCacheEditor = theGolaemCacheEditor;
}

//-------------------------------------------------------------------------
void GolaemCacheEditorWrapper::unregisterGolaemCacheEditor()
{
    _golaemCacheEditor = NULL;
}

IGolaemCacheEditor* GolaemCacheEditorWrapper::getWrapper()
{
    return _golaemCacheEditor;
}

//-------------------------------------------------------------------------
GolaemCacheEditorWrapper* AGolaemCache::GetGolaemCacheEditorWrapper()
{
    return &(glm::Singleton<GolaemCacheEditorWrapper>::getInstance());
}

//-------------------------------------------------------------------------
AGolaemCache::AGolaemCache(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , CurrentFrame(2.f)
    , FrameRateOverride(0.f)
    , bManualTick(false)
    , bIsCacheSetup(false)
    , bIsSimuBuilt(false)
    , bResetInstanced(false)
    , _crowdUnitFactor(100.f) // meters
    , _factory(NULL)
    , _terrainMeshSource(NULL)
    , _terrainMeshDestination(NULL)
    , CharactersHaveBeenReset(false)
    , TransformLock()
{
    PrimaryActorTick.bCanEverTick = true;
    // PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    // Bounds.Init();
    RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("RootComponent"));
    QuatCacheToUE4 = FQuat(-0.7071067811865475f, 0.f, 0.f, 0.7071067811865475f);

#if WITH_EDITOR
    switch (GetDefault<UGolaemCustomSettings>()->CacheImportMode)
    {
    default:
    case EGolaemCacheMode::GCM_MIXED:
        InstanceMode = EInstanceMode::FOR_RIGID_MESHES;
        break;
    case EGolaemCacheMode::GCM_SKINNED:
        InstanceMode = EInstanceMode::DISABLED;
        break;
    case EGolaemCacheMode::GCM_RIGID:
        InstanceMode = EInstanceMode::FORCED;
        break;
    }
#endif

    // #if WITH_EDITORONLY_DATA
    //	USelection::SelectionChangedEvent.AddUObject(this, &AGolaemCache::OnSelectionChanged);
    // #endif
}

//-------------------------------------------------------------------------
AGolaemCache::~AGolaemCache()
{
    GolaemCacheEditorWrapper* golaemCacheEditorWrapper = GetGolaemCacheEditorWrapper();
    if (golaemCacheEditorWrapper->getWrapper() != NULL)
    {
        golaemCacheEditorWrapper->getWrapper()->UnregisterPreSaveWorld(this);
    }

    ResetSimulation();
}

#if WITH_EDITOR
void AGolaemCache::editLayoutParameter(const char* parameterName, int parameterValue)
{
    GEngine->Exec(NULL, TEXT("py import glm.layout.layoutEditorWrapper as layoutWrapper\n"));
    GEngine->Exec(NULL, TEXT("py wrapper = layoutWrapper.getTheLayoutEditorWrapperInstance(True)\n"));

    for (size_t iLayout = 0; iLayout < LayoutFilesParsed.size(); iLayout++)
    {
        // insure that the layout is saved if any :
        FString layoutIdName = GetName();
        layoutIdName += " ";
        layoutIdName += FString::FromInt(iLayout);

        std::stringstream editLayoutParamCmd;
        editLayoutParamCmd << "py wrapper.editLayoutParameter";
        editLayoutParamCmd << "( \"" << TCHAR_TO_ANSI(*layoutIdName) << "\"";
        editLayoutParamCmd << ", \"" << parameterName << "\"";
        editLayoutParamCmd << ", 3"; // parameterType
        editLayoutParamCmd << ", " << (int)parameterValue;
        editLayoutParamCmd << ")\n";

        FString editLayoutParamFStringCmd = editLayoutParamCmd.str().c_str();
        GEngine->Exec(NULL, *editLayoutParamFStringCmd);
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::SaveLayouts()
{
    if (LayoutFilesParsed.size() > 0)
    {
        GEngine->Exec(NULL, TEXT("py import glm.layout.layoutEditorWrapper as layoutWrapper\n"));
        GEngine->Exec(NULL, TEXT("py wrapper = layoutWrapper.getTheLayoutEditorWrapperInstance(True)\n"));

        for (size_t iLayout = 0; iLayout < LayoutFilesParsed.size(); iLayout++)
        {
            // insure that the layout is saved if any :
            FString layoutIdName = GetName();
            layoutIdName += " ";
            layoutIdName += FString::FromInt(iLayout);

            // for each logical index, save it
            FString saveLayoutCmd = "py wrapper.saveCacheProxyTab(\"";
            saveLayoutCmd += layoutIdName;
            saveLayoutCmd += "\")\n";

            GEngine->Exec(NULL, *saveLayoutCmd);
        }
    }
}
#endif

//-------------------------------------------------------------------------
void AGolaemCache::OnPreSaveWorld(UWorld* InWorld, FObjectPreSaveContext ObjectSaveContext)
{
    // call to save layouts of this cache
    // if same world
    if (InWorld == GetWorld())
    {
#if WITH_EDITOR
        SaveLayouts();
#endif
    }
}

//
// #if WITH_EDITORONLY_DATA
// void AGolaemCache::OnSelectionChanged(UObject* NewSelection)
//{
//	GLM_CROWD_TRACE_WARNING("Cache Selection Changed");
//
//	// get crowdEntityID List from selection
//
//	////AGolaemCache* ParentCache = Cast<AGolaemCache>(GetAttachParentActor());
//	////if (ParentCache)
//	////{
//	//	if (Object == this)
//	//	{
//	//		bSelectedInCache = true;
//	//		// register in cache
//	//		ParentCache->SelectEntity(CrowdEntityId);
//	//	}
//	//	else if (bSelectedInCache/*!IsSelected()*/)
//	//	{
//	//		// unregister from cache
//	//		ParentCache->UnselectEntity(CrowdEntityId);
//	//		bSelectedInCache = false;
//	//	}
//	//}
//}
// #endif

////-------------------------------------------------------------------------
// void AGolaemCache::BeginDestroy()
//{
//	// was for save layout, moved to presave delegate
//	Super::BeginDestroy()
// }

// Creation:
//
// Reload de level :
// - PostLoad : ResetSimulation(), NO ResetCharacterActors which destroys all attached actors
//
// PIE :
// Character actors exist (everything is duplicated)
// - PostLoad : ResetSimulation(), NO ResetCharacterActors which destroys all attached actors
// - PreInitializeComponents->InitSimulation(false); // use whatever has been created
// - PostLoad : ResetSimulation(), NO ResetCharacterActors which destroys all attached actors
// - OnConstruction->InitSimulation(false); // use whatever has been created
//
// Drop from Library :
// -PostActorCreated->ResetCharacterActors(), ResetSimulation()
// - OnConstruction(0, 0, 0)->InitSimulation(false); // use whatever has been created
// - OnConstruction(0, 0, 0)->InitSimulation(false); // use whatever has been created
// - PostEditChangeChainProperty->RefreshCache->InitSimulation(True); // use whatever has been created
//
// Duplicate in level->duplicate or sequencer : only GolaemCache is duplicated (not children)
// - PostActorCreated->ResetCharacterActors(), ResetSimulation()
// - OnConstruction(0, 0, 0)->InitSimulation(false); // use whatever has been created
// - OnConstruction(0, 0, 0)->InitSimulation(false); // use whatever has been created

//-------------------------------------------------------------------------
// https://docs.unrealengine.com/portals/0/images/Programming/UnrealArchitecture/Actors/ActorLifecycle/ActorLifeCycle1.png
// For actors statically placed in a level, the normal UObject PostLoad gets called both in the editorand during gameplay.This is not called for newly spawned actors.
void AGolaemCache::PostLoad()
{
    Super::PostLoad();
    SetSceneUnit();

    // force rebuild of Golaem Cache children (rebuild instanced or standard characters)
    glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);

    bIsCacheSetup = true;
    // keep characters allocated as they are present in the scene already
    ResetSimulation();
}

//-------------------------------------------------------------------------
// https://docs.unrealengine.com/portals/0/images/Programming/UnrealArchitecture/Actors/ActorLifecycle/ActorLifeCycle1.png
// When an actor is created in the editor or during gameplay, this gets called right before construction. This is not called for components loaded from a level.
void AGolaemCache::PostActorCreated()
{
    Super::PostActorCreated();
    SetSceneUnit();

    // consider that we defined the cache properties, they have been unserialized
    glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);

    GolaemCacheEditorWrapper* golaemCacheEditorWrapper = GetGolaemCacheEditorWrapper();
    if (golaemCacheEditorWrapper->getWrapper() != NULL)
    {
        golaemCacheEditorWrapper->getWrapper()->RegisterPreSaveWorld(this);
    }

    bIsCacheSetup = true;
    ResetCharacterActors();
    ResetSimulation();

    glm::crowdio::ADPTrackEvent("CREATE_GOLAEMCACHE");
}

//-------------------------------------------------------------------------
// https://docs.unrealengine.com/portals/0/images/Programming/UnrealArchitecture/Actors/ActorLifecycle/ActorLifeCycle1.png
// OnConstruction est appelee quand on drag&drop le GolaemCache dans la scene et au startup du moteur si un GolaemCache a ete sauvegarde dans la scene
void AGolaemCache::OnConstruction(const FTransform& transform)
{
    Super::OnConstruction(transform);

    if (!bIsSimuBuilt)
    {
        // Need to load frame data and restore characters info in real time data once it has been attached. PostLoad actors are not attached yet
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        float CurrentFrameBackup = CurrentFrame; // backup because InitSim could cap current frame to [0;0] if the start/current frame is not set yet
        InitSimulation(CharactersHaveBeenReset); // use whatever has been created or reset characters if they have been reset by a PostActorCreated
        CurrentFrame = CurrentFrameBackup;
        LoadFrameDataAllCrowdFields(true);
    }
}

//-------------------------------------------------------------------------
// void AGolaemCache::PreloadFrame(int frame)
//{
//	for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
//	{
//		FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];
//
//		// simu must exist, it is created in initSimulation before the preload thread
//		thisCrowdFieldData._cachedSimulation = &_factory->getCachedSimulation(TCHAR_TO_ANSI(*CacheDirectory), TCHAR_TO_ANSI(*CacheName), TCHAR_TO_ANSI(*CrowdFields[crowdFieldIndex]));
//
//		// allow to free some frames before getting next ones.
//		thisCrowdFieldData._cachedSimulation->unlockFrameDeletionNoBalance();
//	}
//
//	// balance when all simulations have deletion enabled or we will empty a simu cache before tackling next one
//	_factory->keepMemoryBudgetBalance();
//
//	for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
//	{
//		FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];
//
//		// simu must exist, it is created in initSimulation before the preload thread
//		thisCrowdFieldData._cachedSimulation = &_factory->getCachedSimulation(TCHAR_TO_ANSI(*CacheDirectory), TCHAR_TO_ANSI(*CacheName), TCHAR_TO_ANSI(*CrowdFields[crowdFieldIndex]));
//
//		// lock back frames once clean up have occured
//		thisCrowdFieldData._cachedSimulation->lockFrameDeletion();
//
//		// force a frame get to be sure that frame is available
//		const glm::crowdio::GlmSimulationData* simuData = thisCrowdFieldData._cachedSimulation->getFinalSimulationData();
//		const glm::crowdio::GlmFrameData* frameData = thisCrowdFieldData._cachedSimulation->getFinalFrameData(frame, -1, false, true); // keep frame locked and protected against deletion, until we change of frame
//	}
//
//	PreloadedFrames.insert(frame);
//}

//-------------------------------------------------------------------------
void AGolaemCache::LoadFrameDataAllCrowdFields(bool bInitSimuShader, bool localLock)
{
    // Bounds.Init();
    for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
    {
        LoadFrameData(crowdFieldIndex, bInitSimuShader, localLock);
    }
}

//-------------------------------------------------------------------------
// called before begin play, but after we press play
void AGolaemCache::PreInitializeComponents()
{
    Super::PreInitializeComponents();

    // Need to load frame data and restore characters info in real time data once it has been attached. PostLoad actors are not attached yet
    glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);

    float CurrentFrameBackup = CurrentFrame;  	// backup because InitSim could cap current frame to [0;0] if the start/current frame is not set yet
    InitSimulation(false); 						// use whatever has been created
    CurrentFrame = CurrentFrameBackup;

    LoadFrameDataAllCrowdFields(true);
}

//-------------------------------------------------------------------------
void AGolaemCache::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    // if we are following a postload, force add instances again, as they have been removed by the components delete
}

//-------------------------------------------------------------------------
void AGolaemCache::ResetCharacterActorsInstanced()
{
    TArray<AActor*> allChildActors;
    GetAttachedActors(allChildActors);
    for (int iChildActor = 0; iChildActor < allChildActors.Num(); iChildActor++)
    {
        AActor* attachedActor = allChildActors[iChildActor];
        AGolaemCharacterInstanced* aGolaemCharacterInstanced = Cast<AGolaemCharacterInstanced>(attachedActor);
        if (aGolaemCharacterInstanced != NULL)
        {
            aGolaemCharacterInstanced->Destroy();
        }
    }
    AllInstancedCharacterActors.Empty();
}

//-------------------------------------------------------------------------
void AGolaemCache::RefreshInstancedCharacters()
{
    if (InstanceMode == EInstanceMode::FORCED || InstanceMode == EInstanceMode::FOR_RIGID_MESHES)
    {
#if WITH_EDITOR
        // init all instanced characters :
        AllInstancedCharacterActors.SetNumZeroed(InCharacters.Num(), true);
        for (int iCharacter = 0; iCharacter < InCharacters.Num(); iCharacter++)
        {
            if (!AllInstancedCharacterActors.IsValidIndex(iCharacter) || AllInstancedCharacterActors[iCharacter].Get() == NULL || AllInstancedCharacterActors[iCharacter]->InSkeletalMesh != InCharacters[iCharacter])
            {
                UWorld* world = GetWorld();
                FTransform spawnTransform = FTransform();
                AGolaemCharacterInstanced* thisCharacterInstanced = world->SpawnActorDeferred<AGolaemCharacterInstanced>(AGolaemCharacterInstanced::StaticClass(), spawnTransform);
                thisCharacterInstanced->InSkeletalMesh = InCharacters[iCharacter] /*.Get()*/;
                thisCharacterInstanced->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
                if (!AllInstancedCharacterActors.IsValidIndex(iCharacter))
                {
                    AllInstancedCharacterActors.Add(thisCharacterInstanced);
                }
                else
                {
                    if (AllInstancedCharacterActors[iCharacter].Get() != NULL)
                        AllInstancedCharacterActors[iCharacter]->Destroy();

                    AllInstancedCharacterActors[iCharacter] = thisCharacterInstanced;
                }

                thisCharacterInstanced->AddTickPrerequisiteActor(this);
                if (thisCharacterInstanced->InSkeletalMesh != NULL)
                {
                    thisCharacterInstanced->SetActorLabel(thisCharacterInstanced->InSkeletalMesh->GetName() + FString("_Instances"));
                }

                thisCharacterInstanced->FinishSpawning(spawnTransform);

                // update components after spawning or it fails on some checks
                thisCharacterInstanced->UpdateSkeletalMesh(InstanceMode);

                // GolaemCacheEditorWrapper* golaemCacheEditorWrapper = GetGolaemCacheEditorWrapper();
            }
            else
            {
                // cleanup all used instances to prepare for a new character init, but avoid flickering of instance master creation
                AllInstancedCharacterActors[iCharacter]->RemoveInstances();
            }
            // else the character already exists, no need to stack
        }

        if (_factory)
        {
            // init all instanced characters inverse SortedBones.
            // characters in factory should match allInstancedCharacterActors as InCharacters and AllInstancedCharacterActors are based on factory order
            for (int iCharacter = 0; iCharacter < AllInstancedCharacterActors.Num(); iCharacter++)
            {
                const glm::GolaemCharacter* glmCharacter = _factory->getGolaemCharacter(iCharacter);
                if (glmCharacter != NULL)
                {
                    AllInstancedCharacterActors[iCharacter]->SetCharacterToCacheBones(glmCharacter);
                }
            }
        }
#endif
    }
    else
    {
        ResetCharacterActorsInstanced();
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::InitSimuIfEnoughChars(bool createCharacters)
{
    if (!bIsCacheSetup)
    {
        GLM_CROWD_TRACE_WARNING("You haven't loaded a Golaem simulation yet");
        return;
    }

    // if sim exists and this function is called, characters have changed
    if (bIsSimuBuilt)
    {
        ResetCharacterActors();
        ResetCharacterActorsInstanced();
        ResetSimulation();
    }

    // ToDo : add a check on bones number per character here ?
    if (!CacheName.IsEmpty() && !CacheDirectory.IsEmpty() && CrowdFields.Num() > 0)
    {
        // no CurrentFrameBackup here, we consider we have all that we need once the characters are ready to load
        InitSimulation(createCharacters); // should be valid as it comes from gscb, else it is an error, no need to parse for first available file

        LoadFrameDataAllCrowdFields(true);
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::RefreshTerrains() // force refresh of cache and its characters
{
    if (_factory)
    {
        _factory->setTerrainMeshes(NULL, NULL);
        if (_terrainMeshSource && _terrainMeshSource != _terrainMeshDestination)
        {
            closeTerrainAsset(_terrainMeshSource);
        }
        if (_terrainMeshDestination)
        {
            closeTerrainAsset(_terrainMeshDestination);
        }

        _terrainMeshSource = NULL;
        _terrainMeshDestination = NULL;

        if (!TerrainFileSource.FilePath.IsEmpty())
            _terrainMeshSource = glm::crowdio::crowdTerrain::loadTerrainAsset(TCHAR_TO_ANSI(*TerrainFileSource.FilePath));
        if (!TerrainFileDest.FilePath.IsEmpty())
            _terrainMeshDestination = glm::crowdio::crowdTerrain::loadTerrainAsset(TCHAR_TO_ANSI(*TerrainFileDest.FilePath));
        if (_terrainMeshDestination == NULL)
            _terrainMeshDestination = _terrainMeshSource;
        _factory->setTerrainMeshes(_terrainMeshSource, _terrainMeshDestination);

        // find first effective layout to clear its last node cache (as first active layout adapts to ground) :
        uint32_t historyIndexToClear = static_cast<uint32_t>(_factory->getLayoutHistoryCount()) - 1;
        for (size_t iLayout = 0; iLayout < _factory->getLayoutHistoryCount(); iLayout++)
        {
            if (_factory->getLayoutHistory(iLayout) != NULL)
            {
                historyIndexToClear = static_cast<uint32_t>(iLayout);
                break;
            }
        }

        _factory->clear(glm::crowdio::FactoryClearMode::MODIFIED_SINGLE_NODE, historyIndexToClear, -1, UINT32_MAX); // clear root cache of last history for computation

        // invalidate current frame
        InvalidateCharactersFrame();
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::RefreshCache() // force refresh of cache and its characters
{
    // Force reload in layout editor tab to avoid mismatch between viewport and layout, leading to first edition reloading memory cached layout instead of reloaded from disk one
#if WITH_EDITOR
    ReloadLayoutFile();
#endif

    ResetCharacterActors();
    ResetSimulation();

    float CurrentFrameBackup = CurrentFrame;  // backup because InitSim could cap current frame to [0;0] if the start/current frame is not set yet
    InitSimulation(true);
    CurrentFrame = CurrentFrameBackup;

    LoadFrameDataAllCrowdFields(true);
}

//-------------------------------------------------------------------------
float AGolaemCache::GetSceneUnitFactor() // sets the unit for the Golaem node if a GLMCROWD_UNIT env variable is set
{
    return GolaemUtils::getCrowdUnitFactor();
}

//-------------------------------------------------------------------------
void AGolaemCache::SetSceneUnit() // sets the unit for the Golaem node if a GLMCROWD_UNIT env variable is set
{
    _crowdUnitFactor = AGolaemCache::GetSceneUnitFactor();
}

#if WITH_EDITOR
//-------------------------------------------------------------------------
void AGolaemCache::SetIsTemporarilyHiddenInEditor(bool bIsHidden)
{
    // hide all children
    Super::SetIsTemporarilyHiddenInEditor(bIsHidden);

    // update show percent per CF, will take into account if bEnableSimulation or not
    for (int CrowdFieldIndex = 0; CrowdFieldIndex < CrowdFields.Num(); ++CrowdFieldIndex)
    {
        ShowEntityPercent(CrowdFieldIndex);
    }
}
#endif

//-------------------------------------------------------------------------
// const glm::crowdio::GlmFrameData* AGolaemCache::GetOrWaitPreloadedFrame(int crowdFieldIndex)
//{
//	FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];
//	glm::crowdio::GlmSimulationCacheStatus frameStatus = thisCrowdFieldData._cachedSimulation->getFinalFrameStatus(CurrentFrame, UINT32_MAX);
//
//	while (frameStatus == glm::crowdio::GlmSimulationCacheStatus::GSC_END // no status yet
//		|| frameStatus == glm::crowdio::GlmSimulationCacheStatus::GSC_OPENING // currently opening the frame file
//		|| frameStatus == glm::crowdio::GlmSimulationCacheStatus::GSC_SIMULATION_FRAME_NOT_IN_CACHE) // not in cache yet. As we load current frame, it should arrive quickly in preload thread
//	{
//		glm::sleep(1);
//		frameStatus = thisCrowdFieldData._cachedSimulation->getFinalFrameStatus(CurrentFrame, UINT32_MAX);
//	}
//	return thisCrowdFieldData._cachedSimulation->getFinalFrameData(CurrentFrame, UINT32_MAX, true); // interpolate, don't lock as default
//	//thisCrowdFieldData._cachedSimulation->getFinalFrameData(CurrentFrame, UINT32_MAX, true);
//}

void AGolaemCache::ShowEntityPercentInternal(
    float DrawEntityPercent,
    const glm::crowdio::GlmSimulationData* SimulationData,
    FPerSimData& PerSimData,
    TArray<TWeakObjectPtr<AGolaemCharacter>>& CharactersToProcess,
    TArray<TWeakObjectPtr<AGolaemCharacterInstanced>>& AllInstancedCharacters)
{
    if (SimulationData == NULL)
    {
        PerSimData.ShownCrowdEntityIds.clear();

        // hide everything
        for (int iChar = 0; iChar < CharactersToProcess.Num(); iChar++)
        {
            if (CharactersToProcess[iChar].IsValid())
            {
                AGolaemCharacter* characterActor = CharactersToProcess[iChar].Get();
                characterActor->bShownByPercent = false;
                characterActor->SetActorHiddenInGame(true);
#if WITH_EDITOR
                characterActor->SetIsTemporarilyHiddenInEditor(true);
#endif
            }
        }
    }
    else
    {
        int totalChars = SimulationData->_entityCount;

        // check if an entity enabled status changed
        bool enabledEntitiesChanged = PerSimData.PreviousEnabledChars.Num() != totalChars;
        if (enabledEntitiesChanged)
        {
            PerSimData.PreviousEnabledChars.SetNum(totalChars, true); // in case the size changed
        }

        for (int iChar = 0; iChar < CharactersToProcess.Num(); ++iChar)
        {
            if (CharactersToProcess[iChar].IsValid())
            {
                enabledEntitiesChanged |= CharactersToProcess[iChar]->bEnabledInCache != PerSimData.PreviousEnabledChars[iChar];
                PerSimData.PreviousEnabledChars[iChar] = CharactersToProcess[iChar]->bEnabledInCache;
            }
        }

        int numActorsToShow = FMath::FloorToInt((DrawEntityPercent * totalChars) / 100);
        int numActorsToHide = totalChars - numActorsToShow;

        if (numActorsToShow == static_cast<int32>(PerSimData.ShownCrowdEntityIds.size()) && !enabledEntitiesChanged)
            return;

        // build the new set of crowdEntityIds, keeping existing if we grow, and removing randomly if we shrink
        TArray<int> ActorsHidden;
        TArray<int> ActorsVisible;

        // prepare list of hidden or visible Ids
        {
            ActorsVisible.Empty(numActorsToShow);
            ActorsHidden.Empty(SimulationData->_entityCount - numActorsToShow);

            // use set to init current selection
            for (uint32_t iID = 0; iID < SimulationData->_entityCount; iID++)
            {
                if (PerSimData.ShownCrowdEntityIds.find(SimulationData->_entityIds[iID]) != PerSimData.ShownCrowdEntityIds.end())
                {
                    ActorsVisible.Push(SimulationData->_entityIds[iID]);
                }
                else
                {
                    ActorsHidden.Push(SimulationData->_entityIds[iID]);
                }
            }

            while (ActorsVisible.Num() < numActorsToShow)
            {
                int RandIndex = FMath::RandRange(0.0f, (float)FMath::Max(ActorsHidden.Num() - 1, 0));
                ActorsVisible.Add(ActorsHidden[RandIndex]);
                PerSimData.ShownCrowdEntityIds.insert(ActorsHidden[RandIndex]);
                ActorsHidden.RemoveAt(RandIndex);
            }

            while (ActorsHidden.Num() < numActorsToHide)
            {
                int RandIndex = FMath::RandRange(0.0f, (float)FMath::Max(ActorsVisible.Num() - 1, 0));
                ActorsHidden.Add(ActorsVisible[RandIndex]);
                PerSimData.ShownCrowdEntityIds.erase(ActorsVisible[RandIndex]);
                ActorsVisible.RemoveAt(RandIndex);
            }
        }

        // update GolaemCharacters
        for (int iChar = 0; iChar < CharactersToProcess.Num(); ++iChar)
        {
            if (CharactersToProcess[iChar].IsValid())
            {
                AGolaemCharacter* characterActor = CharactersToProcess[iChar].Get();

                bool shown = PerSimData.ShownCrowdEntityIds.find(characterActor->CrowdEntityId) != PerSimData.ShownCrowdEntityIds.end();
                shown &= characterActor->bEnabledInCache;
                if (characterActor->bShownByPercent && !shown)
                {
                    // hide
                    characterActor->SetActorHiddenInGame(true);
#if WITH_EDITOR
                    characterActor->SetIsTemporarilyHiddenInEditor(true);
#endif
                }
                else if (!characterActor->bShownByPercent && shown)
                {
                    // show
                    characterActor->SetActorHiddenInGame(false);
#if WITH_EDITOR
                    characterActor->SetIsTemporarilyHiddenInEditor(false);
#endif
                }
                characterActor->bShownByPercent = shown;
            }
        }
    }

    // for instances : build the hiddenCrowdEntityIds (we don't want to touch other crowdFields instances)
    glm::GlmSet<int> HiddenCrowdEntityIds;

    for (const auto& characterToProcess : CharactersToProcess)
    {
        if (characterToProcess.IsValid())
        {
            if (PerSimData.ShownCrowdEntityIds.find(characterToProcess->CrowdEntityId) == PerSimData.ShownCrowdEntityIds.end() 
                || !characterToProcess->bEnabledInCache)
            {
                HiddenCrowdEntityIds.insert(characterToProcess->CrowdEntityId);
            }
        }
    }

    // manage all InstancedCharacterActors;
    for (const auto& instancedChar : AllInstancedCharacters)
    {
        if (instancedChar.IsValid())
        {
            // tag instances to enable / disable in a bitArray to set scale to 0 for hidden ones
            instancedChar->SetShownInstances(PerSimData.ShownCrowdEntityIds, HiddenCrowdEntityIds);
            instancedChar->setTransformDirty();
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::ShowEntityPercent(int crowdFieldIndex, bool locked)
{
    if (crowdFieldIndex < DataPerCF.Num())
    {
        FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];
        TArray<TWeakObjectPtr<AGolaemCharacter>>& charactersToProcess = DataPerCF[crowdFieldIndex].AllChars;

        bool isCacheHiddenInEditor = false;
#if WITH_EDITOR
        isCacheHiddenInEditor = IsTemporarilyHiddenInEditor(true); // true : include parent hidden
#endif
        // wait for frame data status :
        const glm::crowdio::GlmSimulationData* simulData = thisCrowdFieldData._cachedSimulation->getFinalSimulationData();
        const glm::crowdio::GlmFrameData* frameData = thisCrowdFieldData._cachedSimulation->getFinalFrameData(CurrentFrame, UINT32_MAX, true);
        // const glm::crowdio::GlmFrameData* frameData = GetOrWaitPreloadedFrame(crowdFieldIndex);

        if (frameData == NULL || !bEnableSimulation || isCacheHiddenInEditor)
        {
            simulData = NULL;
        }

        ShowEntityPercentInternal(DrawEntityPercent, simulData, thisCrowdFieldData, charactersToProcess, AllInstancedCharacterActors);
    }
}

//-------------------------------------------------------------------------
// void AGolaemCache::RefreshLayoutActors(int entityID, FString TransformType, FString TransformValue)
//{
//    char* entityIDchar = "";
//    IntToChar(entityID, entityIDchar);
//	char* trType = TCHAR_TO_ANSI(*TransformType);
//	char* trValue = TCHAR_TO_ANSI(*TransformValue);
//    GEngine->Exec(NULL, TEXT("py import glm.layout.layoutEditorWrapper as layoutWrapper"));
//    FString pyCmd = "py wrapper = layoutWrapper.getTheLayoutEditorWrapperInstance(True)";
//    FString pyCmd2 = "py wrapper.addOrEditLayoutTransformation('" + GetName() + "', " + entityIDchar + ", " + trType + ", Nom, " + trValue + ")";
//    GEngine->Exec(NULL, *pyCmd);
//    GEngine->Exec(NULL, *pyCmd2);
//}

#if WITH_EDITOR

//-------------------------------------------------------------------------
void AGolaemCache::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
    if (e.GetPropertyName() == FName(TEXT("FilePath")))
    {
        // FString memberProperty = e.MemberProperty->GetName();
        // const char* test = TCHAR_TO_ANSI(*memberProperty);

        FString headPropertyName = e.PropertyChain.GetHead()->GetValue()->GetName();

        // if (headPropertyName == TEXT("TerrainFileSource") || headPropertyName == TEXT("TerrainFileDest"))
        //{
        //     RefreshTerrains();
        // }

        if (headPropertyName == TEXT("TerrainFileDest"))
        {
            RefreshTerrains();
        }
    }

    if (e.GetPropertyName() == FName(TEXT("CharactersFiles")))
    {
        CharactersFiles = CharactersFiles.TrimStartAndEnd();
        TArray<FString> CharacterFileList;
        CharactersFiles.ParseIntoArray(CharacterFileList, TEXT(";"), true);
        if (_factory != NULL)
        {
            InCharacters.SetNum(_factory->getGolaemCharacters().sizeInt()); // make one input objet per needed character
        }
        else
        {
            InCharacters.SetNum(CharacterFileList.Num());
        }
    }

    if (e.GetPropertyName() == FName(TEXT("InstanceMode")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        InitSimuIfEnoughChars(true);
    }

    if (e.GetPropertyName() == FName(TEXT("bEnableLayout")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        RefreshCache();
    }

    if (e.GetPropertyName() == FName(TEXT("LayoutFile")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        RefreshCache();
    }

    if (e.GetPropertyName() == FName(TEXT("CurrentFrame")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        if (bIsSimuBuilt)
        {

            if (CurrentFrame < StartFrame)
            {
                CurrentFrame = StartFrame;
            }
            else if (CurrentFrame > EndFrame)
            {
                CurrentFrame = EndFrame;
            }

            LoadFrameDataAllCrowdFields();

            UpdateSimuFrameData();
        }
    }

    if (e.GetPropertyName() == FName(TEXT("InCharacters")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        InitSimuIfEnoughChars(true);
    }

    if (e.GetPropertyName() == FName(TEXT("CrowdFields")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        InitSimuIfEnoughChars(true);
    }

    if (e.GetPropertyName() == FName(TEXT("CacheName")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        InitSimuIfEnoughChars(true);
    }

    if (e.GetPropertyName() == FName(TEXT("CacheDirectory")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        InitSimuIfEnoughChars(true);
    }

    if (e.GetPropertyName() == FName(TEXT("EnableSimulation")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);

        for (auto& CFData : DataPerCF)
        {
            for (const auto& Char : CFData.AllChars)
            {
                if (Char.IsValid())
                {
                    Char->SetActorTickEnabled(bEnableSimulation);
                }
            }
        }

        for (int i = 0; i < AllInstancedCharacterActors.Num(); ++i)
        {
            if (AllInstancedCharacterActors[i].Get() != NULL)
            {
                AllInstancedCharacterActors[i]->SetActorTickEnabled(bEnableSimulation);
            }
        }

        // update show percent per CF, will take into account if bEnableSimulation or not
        for (int CrowdFieldIndex = 0; CrowdFieldIndex < CrowdFields.Num(); ++CrowdFieldIndex)
        {
            ShowEntityPercent(CrowdFieldIndex);
        }
    }

    if (e.GetPropertyName() == FName(TEXT("UseInstancedCharacters")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        ResetCharacterActors();
        ResetSimulation();
        InitSimuIfEnoughChars(true);
    }

    if (e.GetPropertyName() == FName(TEXT("DrawEntityPercent")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        for (int CrowdFieldIndex = 0; CrowdFieldIndex < CrowdFields.Num(); ++CrowdFieldIndex)
        {
            ShowEntityPercent(CrowdFieldIndex);
        }
    }

    if (e.GetPropertyName() == FName(TEXT("TerrainAdaptationMode")))
    {
        if (bEnableLayout)
        {
            // open the gscl in layout editor if not done, or this will have no effect as there won't be any refresh from the layoutEditor
            OpenLayoutFile();

            editLayoutParameter("defaultGroundAdaptationMode", (uint8)TerrainAdaptationMode);

            // reload the python side layout setting in the factory
            refreshLayoutFromEditor();

            // refresh history to readapt
            RefreshTerrains();
        }
    }

    Super::PostEditChangeChainProperty(e);
}

//-------------------------------------------------------------------------
void AGolaemCache::PostEditChangeProperty(FPropertyChangedEvent& e)
{
    // Super::PostEditChangeProperty(e);
}
#endif

//-------------------------------------------------------------------------
// Called when the game starts or when spawned
void AGolaemCache::BeginPlay()
{
    Super::BeginPlay();

    glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
    // PrimaryActorTick.bCanEverTick = true;

    TimeFromBeginPlay = 0.f;
    CurrentFrame = StartFrame;

    //_cumulInCharacters = 0;
    // Permet de mettre les personnages sur leur position en premiere frame avant que la timeline soit appelee
    LoadFrameDataAllCrowdFields();

    //// Hack to preload 100 frames and see impact on fps :
    // for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
    //{
    //	FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];
    //	for (int i = 1; i < 1000; i++)
    //	{
    //		thisCrowdFieldData._cachedSimulation->getFinalFrameData(i, UINT32_MAX, true);
    //	}
    // }
}

//-------------------------------------------------------------------------
void AGolaemCache::InvalidateCharactersFrame()
{
    // force character update
    for (auto& CFData : DataPerCF)
    {
        for (const auto& Char : CFData.AllChars)
        {
            if (Char.IsValid())
            {
                Char->CurrentTime = FLT_MAX;
            }
        }
    }

    // force instanced character update
    // Animation will be updated while dragging, update instances to follow
    for (int iInstanceCharacter = 0; iInstanceCharacter < AllInstancedCharacterActors.Num(); iInstanceCharacter++)
    {
        AllInstancedCharacterActors[iInstanceCharacter].Get()->setTransformDirty();
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::UpdateSimuFrameData()
{
    if (_factory && (InstanceMode == EInstanceMode::FORCED || InstanceMode == EInstanceMode::FOR_RIGID_MESHES))
    {
        for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
        {
            FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];

            thisCrowdFieldData._cachedSimulation = &_factory->getCachedSimulation(CacheDirectoryDirmapped.c_str(), TCHAR_TO_ANSI(*CacheName), TCHAR_TO_ANSI(*CrowdFields[crowdFieldIndex]));

            // let frames locked here, clean up is done by preload thread
            // thisCrowdFieldData._cachedSimulation->unlockFrameDeletion();
            // thisCrowdFieldData._cachedSimulation->lockFrameDeletion();

            // force a frame get to be sure that frame is available
            const glm::crowdio::GlmSimulationData* simuData = thisCrowdFieldData._cachedSimulation->getFinalSimulationData();
            const glm::crowdio::GlmFrameData* frameData = thisCrowdFieldData._cachedSimulation->getFinalFrameData(CurrentFrame, UINT32_MAX, true);
            if (simuData == NULL || frameData == NULL)
                return;

            // commented out : dead code
            //            // update visibility of entities
            //            bool isCacheHiddenInEditor = false;
            // #if WITH_EDITOR
            //            isCacheHiddenInEditor = IsTemporarilyHiddenInEditor(true); // true : include parent hidden
            // #endif
            //            if (frameData != NULL && bEnableSimulation && !isCacheHiddenInEditor)
            //            {
            //                // update entities visibility
            //            }

            //
            // const glm::crowdio::GlmFrameData* frameData = GetOrWaitPreloadedFrame(crowdFieldIndex);

            //// ONLY WORKS FOR ONE CROWD FIELD IN THIS VERSION
            // if (simuData != NULL && frameData != NULL)
            //{
            //	for (int iInstanceCharacter = 0; iInstanceCharacter < AllInstancedCharacterActors.Num(); iInstanceCharacter)
            //	{
            //		// ToDo : multiple crowdFields !!! need to handle instances "per simuData"
            //		AllInstancedCharacterActors[iInstanceCharacter].Get()->UpdateTransforms(*simuData, *frameData);
            //	}
            // }
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::UpdateTransforms_AnyThread(AGolaemCharacterInstanced* characterInstanced)
{
    if (_factory)
    {
        // tell the character that it is up-to-date
        for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
        {
            FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];

            thisCrowdFieldData._cachedSimulation = &_factory->getCachedSimulation(CacheDirectoryDirmapped.c_str(), TCHAR_TO_ANSI(*CacheName), TCHAR_TO_ANSI(*CrowdFields[crowdFieldIndex]));

            //// allow to free some frames before getting next ones.
            // thisCrowdFieldData._cachedSimulation->unlockFrameDeletion();
            // thisCrowdFieldData._cachedSimulation->lockFrameDeletion();

            // force a frame get to be sure that frame is available
            const glm::crowdio::GlmSimulationData* simuData = thisCrowdFieldData._cachedSimulation->getFinalSimulationData();
            const glm::crowdio::GlmFrameData* frameData = thisCrowdFieldData._cachedSimulation->getFinalFrameData(CurrentFrame, -1, true);

            if (simuData != NULL && frameData != NULL)
            {
                // GLM_DCC_TRACE_WARNING("CharacterInstanced->UpdateTransforms updated to current frame  : " << CurrentFrame);
                characterInstanced->UpdateTransforms(*simuData, *frameData, crowdFieldIndex, /* _factory,*/ CurrentFrame, _crowdUnitFactor);
            }
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::TickAtThisTime(const float Time, bool bInBackwards, bool bInIsLooping)
{
    // if (bManualTick /*&& GeometryCache &&*/)
    {
        // probably to do later, updating bounds for rendering
        //// Game thread update:
        //// This mainly just updates the matrix and bounding boxes. All render state (meshes) is done on the render thread
        // bool bUpdatedBoundsOrMatrix = false;
        // for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
        //{
        //	bUpdatedBoundsOrMatrix |= UpdateTrackSection(TrackIndex);
        // }

        // if (bUpdatedBoundsOrMatrix)
        //{
        //	UpdateLocalBounds();
        //	// Mark the transform as dirty, so the bounds are updated and sent to the render thread
        //	MarkRenderTransformDirty();
        // }

        if (Time != CurrentFrame)
        {
            // glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
            CurrentFrame = Time;
            LoadFrameDataAllCrowdFields(false, true); // don't init simu / shader, use local lock

            UpdateSimuFrameData();
        }
    }
}

//-------------------------------------------------------------------------
// Called every frame
void AGolaemCache::Tick(float DeltaTime)
{
    // GLM_DCC_TRACE_WARNING("Golaem Cache Tick");
    if (!bManualTick)
    {
        // glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);

        // in PIE, for sequencer this is not active because bManualTick is false
        Super::Tick(DeltaTime);

        TimeFromBeginPlay += DeltaTime;

        float frameDuration = 1.f; 
        if (FrameRateOverride > 0.f)
        {
            frameDuration = 1.f / FrameRateOverride; 
        }
        float newFrame = TimeFromBeginPlay / frameDuration;

        if (newFrame != CurrentFrame)
        {
            CurrentFrame = newFrame;
            LoadFrameDataAllCrowdFields(false, true); // no init simu, use local lock (on swap)
            UpdateSimuFrameData();
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::GetMeshIndicesInUnrealPerCharacter(
    const USkeletalMesh* InCharacter,
    const glm::GolaemCharacter* glmChar,
    FPerSimData& PerSimData,
    int characterIndex)
{
    // for all characters, check the blendshape indices for this simu data,
    PerSimData.MorphTargetsToBlindDataIndexPerChar[characterIndex].resize(InCharacter->MorphTargets.Num());
    for (int iMorphTarget = 0; iMorphTarget < InCharacter->MorphTargets.Num(); iMorphTarget++)
    {
        FString MorphTargetName = InCharacter->MorphTargets[iMorphTarget]->GetName();
        PerSimData.MorphTargetsToBlindDataIndexPerChar[characterIndex][iMorphTarget] = -1;
        const glm::Array<glm::BlindData>& blindData = glmChar->_converterMapping.getBlindDataInfo();
        for (int iSimuBD = 0; iSimuBD < blindData.size() && PerSimData.MorphTargetsToBlindDataIndexPerChar[characterIndex][iMorphTarget] == -1; iSimuBD++)
        {
            const glm::BlindData& currentBSGroup = blindData[iSimuBD];
            if (currentBSGroup._type == glm::BlindData::Type::BLENDSHAPE)
            {
                int32 outIndex = -1;
                // FName blendshapeName = currentBSGroup._name.c_str();
                FString blendshapeGroupName = currentBSGroup._name.c_str();
                if (blendshapeGroupName == MorphTargetName)
                {
                    PerSimData.MorphTargetsToBlindDataIndexPerChar[characterIndex][iMorphTarget] = iSimuBD;
                }
            }
        }
    }

    // for all character, compute the proper mesh indices when multimaterials are used (need to shift index every time multi materials are found)
    size_t meshIndex(0);
    const glm::Array<glm::MeshAsset>& meshAssets(glmChar->_meshAssets);
    PerSimData.MeshIndicesInUnrealPerCharacter[characterIndex].resize(meshAssets.size());
    for (size_t iMeshAsset = 0; iMeshAsset < meshAssets.size(); ++iMeshAsset)
    {
        for (size_t iSG = 0; iSG < meshAssets[iMeshAsset]._shadingGroups.size(); ++iSG)
        {
            PerSimData.MeshIndicesInUnrealPerCharacter[characterIndex][iMeshAsset].push_back(meshIndex);
            meshIndex++;
        }
    }
}

//-------------------------------------------------------------------------
bool AGolaemCache::FillInCharactersFromFactory(bool defaultToFirst)
{
    if (_factory == NULL)
        return false;

    InCharacters.SetNum(_factory->getGolaemCharacters().sizeInt());

    FAssetRegistryModule& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> CharacterAssetData;
    IAssetRegistry& assetRegistry = assetRegistryModule.Get();
    assetRegistry.GetAssetsByClass(UGolaemSkeletalMesh::StaticClass()->GetClassPathName(), CharacterAssetData);
    assetRegistry.GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), CharacterAssetData);

    for (int iChar = 0; iChar < InCharacters.Num(); ++iChar)
    {
        bool assetFound = InCharacters[iChar] != NULL;
        if (assetFound)
        {
            continue;
        }

        const glm::GolaemCharacter* glmChar = _factory->getGolaemCharacter(iChar);
        if (glmChar != NULL)
        {
            const glm::GlmString& characterFilePath = glmChar->_runtimeFilePath;
            glm::GlmString characterName = glm::FileName(characterFilePath).filenameNoExt();

            for (int iCharAsset = 0, assetCount = CharacterAssetData.Num(); iCharAsset < assetCount; ++iCharAsset)
            {
                FAssetData& charAsset = CharacterAssetData[iCharAsset];
                if (charAsset.AssetName == characterName.c_str())
                {
                    USkeletalMesh* golaemCharacter = Cast<USkeletalMesh>(charAsset.GetAsset());
                    if (golaemCharacter != NULL)
                    {
                        assetFound = true;
                        InCharacters[iChar] = golaemCharacter;
                    }
                    break;
                }
            }
            if (!assetFound)
            {
                GLM_CROWD_TRACE_ERROR("Could not find Unreal asset for Golaem Character file '" << characterFilePath << "'. Please import the character first!");
                ResetSimulation();
                return false;
            }
        }
        else
        {
            if (defaultToFirst && InCharacters[0] != NULL)
            {
                InCharacters[iChar] = InCharacters[0];
            }
            else
            {
                GLM_CROWD_TRACE_ERROR("Empty Golaem Character at index '" << iChar << "'!");
                ResetSimulation();
                return false;
            }
        }
    }

    TemplateCharactersData.SetNum(_factory->getGolaemCharacters().sizeInt());
    for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
    {
        FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];

        thisCrowdFieldData.MeshIndicesInUnrealPerCharacter.resize(InCharacters.Num());
        thisCrowdFieldData.MorphTargetsToBlindDataIndexPerChar.resize(InCharacters.Num());
        for (int iChar = 0; iChar < InCharacters.Num(); iChar++)
        {
            thisCrowdFieldData.MorphTargetsToBlindDataIndexPerChar[iChar].empty();
            const glm::GolaemCharacter* glmChar = _factory->getGolaemCharacter(iChar);
            if (glmChar != NULL)
            {
                GetMeshIndicesInUnrealPerCharacter(InCharacters[iChar], glmChar, thisCrowdFieldData, iChar);
            }
        }
    }

    return true;
}

//-------------------------------------------------------------------------
void AGolaemCache::InitSimulation(bool createCharacters)
{
    // character load
    if (_factory == NULL)
    {
        _factory = new glm::crowdio::SimulationCacheFactory();
        _factory->setMemoryBudget(1024);
    }
    else
    {
        _factory->clear(glm::crowdio::FactoryClearMode::ALL);
    }

    // rebuild a dirmapped string :
    GolaemUtils::dirmapString(CharactersFiles);

    _factory->loadGolaemCharacters(TCHAR_TO_ANSI(*CharactersFiles));

    // proxy matrix
    glm::Matrix4 proxyMatrix(glm::Matrix4::identity);
    proxyMatrix.scale(glm::Vector3(1.f));
    glm::Matrix4 proxyMatrixInverse(glm::Matrix4::identity);
    proxyMatrixInverse.inverse(proxyMatrix);
    _factory->setSimulationProxyMatrix(proxyMatrix, proxyMatrixInverse);

    // terrain
    if (_terrainMeshSource == NULL)
    {
        if (!TerrainFileSource.FilePath.IsEmpty())
            _terrainMeshSource = glm::crowdio::crowdTerrain::loadTerrainAsset(TCHAR_TO_ANSI(*TerrainFileSource.FilePath));
        if (!TerrainFileDest.FilePath.IsEmpty())
            _terrainMeshDestination = glm::crowdio::crowdTerrain::loadTerrainAsset(TCHAR_TO_ANSI(*TerrainFileDest.FilePath));
        if (_terrainMeshDestination == NULL)
            _terrainMeshDestination = _terrainMeshSource;
        _factory->setTerrainMeshes(_terrainMeshSource, _terrainMeshDestination);
    }

    // Note : not applied if bEnableLayout is false. Need to recall this when enabling / disabling the layout !
    if (bEnableLayout)
    {
        GolaemUtils::dirmapString(LayoutFile);

        LayoutFilesParsed.clear();
        LayoutFilesParsed.reserve(50); // without reservation, GlmString allocations occurs in split with another allocator ! With 50 we shoud be safe
        const char* layoutFilesChar = TCHAR_TO_ANSI(*LayoutFile);
        glm::GlmString layoutFilesGlm = layoutFilesChar;
        glm::split(layoutFilesGlm, ";", LayoutFilesParsed);

        // Load history and update terrain mode
        ETerrainAdaptationMode terrainMode = ETerrainAdaptationMode::NONE;
        for (size_t iLayout = 0, layoutCount = LayoutFilesParsed.size(); iLayout < layoutCount; ++iLayout)
        {
            if (!LayoutFilesParsed[iLayout].empty())
            {
                _factory->loadLayoutHistoryFile(iLayout, LayoutFilesParsed[iLayout].c_str());

                // update terrain mode
                const glm::crowdio::GlmHistory* history = _factory->getLayoutHistory(iLayout);
                if (history != NULL)
                {
                    // terrainMode =
                    for (uint32_t iParam = 0; iParam < history->_parametersCount; iParam++)
                    {
                        if (!strcmp(history->_parameters[iParam]._name._values, "defaultGroundAdaptationMode") && history->_parameters[iParam]._type == 3 // GTV_uint32
                            && history->_parameters[iParam]._int32val != NULL && history->_parameters[iParam]._int32val->_count > 0)
                        {
                            terrainMode = (ETerrainAdaptationMode)history->_parameters[iParam]._int32val->_values[0];
                        }
                    }

                    // update terrain adaptation mode with last mode encountered
                    TerrainAdaptationMode = terrainMode;
                }
            }
        }
    }

    // dirmap cache directory :
    GolaemUtils::dirmapString(CacheDirectory);
    CacheDirectoryDirmapped = TCHAR_TO_ANSI(*CacheDirectory);

    // dimension according to input crowdFields
    DataPerCF.SetNum(CrowdFields.Num());
    for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
    {
        FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];

        thisCrowdFieldData._cachedSimulation = &_factory->getCachedSimulation(CacheDirectoryDirmapped.c_str(), TCHAR_TO_ANSI(*CacheName), TCHAR_TO_ANSI(*CrowdFields[crowdFieldIndex]));
        thisCrowdFieldData._cachedSimulation->getFinalSimulationData(); // mandatory to get createEntity new characters loaded in factory before allocating InCharacters from factory
    }

    if (!FillInCharactersFromFactory(false)) // cannot find all characters, reset
        return;

    // create needed / all instancing characters if needed. setGCHABones may not be called if factory not done yet (will be done in InitSim then)
    if (createCharacters)
    {
        RefreshInstancedCharacters();
    }

    // runtime data
    TArray<FMaterialParameterInfo> ScalarParamInfos;
    TArray<FGuid> ScalarParamGuids;
    TArray<FMaterialParameterInfo> VectorParamInfos;
    TArray<FGuid> VectorParamGuids;

    // insure that we will poll a gscf in the correct range
    if (CurrentFrame < StartFrame)
        CurrentFrame = StartFrame;

    if (CurrentFrame > EndFrame)
        CurrentFrame = EndFrame;

    TemplateCharactersData.SetNum(_factory->getGolaemCharacters().sizeInt());
    for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
    {
        FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];

        // will feed CurrentSimuData[CrowdFieldIndex] and its modified counterpart
        const glm::crowdio::GlmSimulationData* simuData = thisCrowdFieldData._cachedSimulation->getFinalSimulationData();

        const glm::crowdio::GlmFrameData* frameData = thisCrowdFieldData._cachedSimulation->getFinalFrameData(CurrentFrame, -1, false);
        if (simuData == NULL)
        {
            FString gscsFileName = CacheName + "." + CrowdFields[crowdFieldIndex] + ".gscs";
            FString gscsFilePath = FPaths::Combine(CacheDirectoryDirmapped.c_str(), gscsFileName);
            GLM_CROWD_TRACE_WARNING("Golaem Cache " << TCHAR_TO_ANSI(*this->GetName()) << " Could not load its .gscs file " << TCHAR_TO_ANSI(*gscsFilePath));
            // USceneComponent* cacheProxyRootComponent = GetRootComponent();       // if we have no sim, probably better to hide ourself   ?
            // cacheProxyRootComponent->SetVisibility(false, true);
            continue;
        }

        if (frameData == NULL)
        {
            FString gscfFileName = CacheName + "." + CrowdFields[crowdFieldIndex] + "." + FString::FromInt(static_cast<int>(CurrentFrame)) + ".gscf";
            FString gscfFilePath = FPaths::Combine(CacheDirectoryDirmapped.c_str(), gscfFileName);
            GLM_CROWD_TRACE_WARNING("Golaem Cache " << TCHAR_TO_ANSI(*this->GetName()) << " Could not load its .gscf file " << TCHAR_TO_ANSI(*gscfFilePath));
            continue;
        }

        // update frameRate according to first simuData :
        if (crowdFieldIndex == 0)
        {
            FrameRate = simuData->_framerate;
            if (FrameRateOverride <= 0.f)
            {
                FrameRateOverride = simuData->_framerate;
            }            
        }
    }

    if (createCharacters)
    {
        CharactersHaveBeenReset = false;
        RefreshCharacters();
    }

    bIsSimuBuilt = true;

    // launch preload thread here
    // PreloadCacheWorker = new FCachePreloadWorker(this);
}

void AGolaemCache::SpawnActor(
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
    TArray<TWeakObjectPtr<AGolaemCharacterInstanced>>& AllInstancedCharacterActors)
{
    TArray<int> meshesToKeep;
    if (ThisEntityCharacterIndex < PerSimData.MeshIndicesInUnrealPerCharacter.size())
    {
        for (size_t iMesh = 0; iMesh < EntityAssets.size(); ++iMesh)
        {
            if (EntityAssets[iMesh] < PerSimData.MeshIndicesInUnrealPerCharacter[ThisEntityCharacterIndex].size())
            {
                for (size_t iMeshIdx = 0; iMeshIdx < PerSimData.MeshIndicesInUnrealPerCharacter[ThisEntityCharacterIndex][EntityAssets[iMesh]].size(); ++iMeshIdx)
                    meshesToKeep.Push((int32)PerSimData.MeshIndicesInUnrealPerCharacter[ThisEntityCharacterIndex][EntityAssets[iMesh]][iMeshIdx]);
            }
        }
    }

    if (InstanceMode == EInstanceMode::FOR_RIGID_MESHES || InstanceMode == EInstanceMode::FORCED)
    {
        if (AllInstancedCharacterActors.IsValidIndex(ThisEntityCharacterIndex) && AllInstancedCharacterActors[ThisEntityCharacterIndex].Get() != NULL)
        {
            // add an instance on this character :
            AllInstancedCharacterActors[ThisEntityCharacterIndex]->AddInstance(SimData, SimCrowdFieldIndex, meshesToKeep, SimDataEntityIdx);
        }
    }

    if (InstanceMode == EInstanceMode::FOR_RIGID_MESHES || InstanceMode == EInstanceMode::DISABLED)
    {
        FTransform spawnTransform;
        // SpawnActorDeferred permet d'executer certaines methodes de GolaemCharacter avant de faire effectivement spawn le personnage

        // entityName = FString::Printf(TEXT("Entity_%d"), entityId);
        // glmCharacterActor->Rename(*entityName); // failing crashing on "glmCharacterActor needs loaded" when changing outer.. which does not change hum

        AGolaemCharacter* glmCharacterActor = World->SpawnActorDeferred<AGolaemCharacter>(AGolaemCharacter::StaticClass(), spawnTransform);

        if (Cast<AGolaemCache>(ParentActor) != NULL)
        {
            GolaemCacheEditorWrapper* golaemCacheEditorWrapper = GetGolaemCacheEditorWrapper();
            if (golaemCacheEditorWrapper->getWrapper() != NULL)
            {
                golaemCacheEditorWrapper->getWrapper()->AddSelectionCallBack(*glmCharacterActor);
            }
        }

        glmCharacterActor->CharacterIndex = ThisEntityCharacterIndex;
        // glmCharacterActor->MasterSkel = InCharacters[ThisEntityCharacterIndex].Get();// ->Skeleton;
        glmCharacterActor->CrowdEntityId = SimData->_entityIds[SimDataEntityIdx]; // for latter reference
        glmCharacterActor->CrowdFieldIndex = SimCrowdFieldIndex;
        glmCharacterActor->CacheIndex = SimDataEntityIdx;

        /* /////////////////////////////////////////////////// */

        // UGameplayStatics::FinishSpawningActor(glmCharacterActor, spawnTransform);

        USkeletalMeshComponent* SkeletalMeshComponent = glmCharacterActor->GetSkeletalMeshComponent();
        FSkeletalMeshRenderData* renderMeshData = SkeletalMeshComponent->GetSkeletalMeshRenderData();
        SkeletalMeshComponent->SetSkeletalMesh(InCharacter /*.Get()*/);

        // do not update RigidSections for Instance disabled, keep all mesh as skinned
        if (InstanceMode == EInstanceMode::FOR_RIGID_MESHES && AllInstancedCharacterActors.IsValidIndex(ThisEntityCharacterIndex) && AllInstancedCharacterActors[ThisEntityCharacterIndex].Get() != NULL)
        {
            glmCharacterActor->RigidSections = AllInstancedCharacterActors[ThisEntityCharacterIndex]->RigidSections;
        }
        glmCharacterActor->MeshSelectionByMaterialSection(meshesToKeep);

        // mesh animation setup
        SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationCustomMode);
        SkeletalMeshComponent->SetAnimInstanceClass(UGolaemAnimInstance::StaticClass());
        SkeletalMeshComponent->SetUpdateAnimationInEditor(true);

        // Attache le GolaemCharacter au GolaemCache
        glmCharacterActor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform);
        SkeletalMeshComponent->InitAnim(true); // init Anim after setting the parent, as we retrieve it in animNode init !
        // SkeletalMeshComponent->TickPose();
        SkeletalMeshComponent->RefreshBoneTransforms(); // hopefully will make actor show ?

        // Genere la bonne hierarchie de bones une fois pour chaque type de personnages du cache
        if (TemplateCharactersData[ThisEntityCharacterIndex]._boneHierarchy.Num() == 0)
        {
            GolaemSimulationInterface->ComputeSkeleton(glmCharacterActor, TemplateCharactersData[ThisEntityCharacterIndex]._boneHierarchy);
        }
        // check that the found Unreal character has the same number of bones
        if (SimData->_boneCount[SimData->_entityTypes[SimDataEntityIdx]] != TemplateCharactersData[ThisEntityCharacterIndex]._boneHierarchy.Num())
        {
            GLM_CROWD_TRACE_WARNING_LIMIT(
                "Mismatched number of bones in Golaem Cache Node for Golaem Entity " << glmCharacterActor->CrowdEntityId
                                                                                     << ". Expected number of bones is " << SimData->_boneCount[SimData->_entityTypes[SimDataEntityIdx]]
                                                                                     << ", Skeletal Mesh " << TCHAR_TO_ANSI(*glmCharacterActor->GetName()) << " has " << TemplateCharactersData[ThisEntityCharacterIndex]._boneHierarchy.Num() << " bones");
            TemplateCharactersData[ThisEntityCharacterIndex]._boneHierarchy.Empty();
            return;
        }

        PerSimData.AllChars[SimDataEntityIdx] = glmCharacterActor;

        glmCharacterActor->bShouldDoAnimNotifies = false;
        glmCharacterActor->SetActorEnableCollision(false);
        glmCharacterActor->SetCanBeDamaged(false);

#if WITH_EDITOR
        FString ActorName = "Entity_" + FString::FromInt(glmCharacterActor->CrowdEntityId);
        glmCharacterActor->SetActorLabel(*ActorName); // Rename modifies unique ID , we don't want to unique rename entity 1001 if we load several times a cache with 1001 (name collision)
#endif

        SkeletalMeshComponent->bDisableClothSimulation = true;
        SkeletalMeshComponent->SetAllowAnimCurveEvaluation(false);

        glmCharacterActor->AddTickPrerequisiteActor(ParentActor);
        glmCharacterActor->FinishSpawning(spawnTransform);

        SkeletalMeshComponent->AddTickPrerequisiteActor(ParentActor);
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::RefreshCharacters()
{
    ResetCharacterActors();
    for (auto& CFData : DataPerCF)
    {
        CFData.AllChars.Empty();
    }

    UWorld* world = GetWorld();
    FTransform spawnTransform = FTransform();
    FString entityName;

    // rebuild instance holders with up to date data
    RefreshInstancedCharacters();

    for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
    {
        FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];
        const glm::Array<glm::PODArray<int>>& allEntityAssets = thisCrowdFieldData._cachedSimulation->getFinalEntityAssets(CurrentFrame);

        const glm::crowdio::GlmSimulationData* simuData = thisCrowdFieldData._cachedSimulation->getFinalSimulationData();
        if (simuData == NULL)
            continue;

        const glm::crowdio::GlmFrameData* frameData = thisCrowdFieldData._cachedSimulation->getFinalFrameData(CurrentFrame, -1, false);
        if (frameData == NULL)
            continue;

        // check simData character bone count compared to bound asset :
        // for each new characteridx, store its boneCount, then compare with bound asset from factory
        glm::PODArray<int> cacheCharacterToBoneCount;
        for (uint32_t iEntity = 0; iEntity < simuData->_entityCount; iEntity++)
        {
            if (!glm::crowdio::isEntityValid(simuData, iEntity))
            {
                continue;
            }

            if (cacheCharacterToBoneCount.size() <= simuData->_characterIdx[iEntity])
            {
                cacheCharacterToBoneCount.resize(simuData->_characterIdx[iEntity] + 1, -1);
            }

            cacheCharacterToBoneCount[simuData->_characterIdx[iEntity]] = simuData->_boneCount[simuData->_entityTypes[iEntity]];
        }

        bool simDataBoneCountMismatch = false;
        int iCharacter = 0;
        for (; iCharacter < InCharacters.Num() && !simDataBoneCountMismatch; iCharacter++)
        {
            simDataBoneCountMismatch = iCharacter < cacheCharacterToBoneCount.size() && cacheCharacterToBoneCount[iCharacter] != -1 && (cacheCharacterToBoneCount[iCharacter] != InCharacters[iCharacter]->GetRefSkeleton().GetNum() || (_factory != NULL && _factory->getGolaemCharacter(iCharacter) != NULL && _factory->getGolaemCharacter(iCharacter)->_converterMapping._skeletonDescription->getBones().size() != InCharacters[iCharacter]->GetRefSkeleton().GetNum()));
        }

        if (simDataBoneCountMismatch)
        {
            iCharacter--;
            FString gscsFileName = CacheName + "." + CrowdFields[crowdFieldIndex] + ".gscs";
            FString gscsFilePath = FPaths::Combine(CacheDirectoryDirmapped.c_str(), gscsFileName);
            size_t factoryCount = (_factory == NULL || _factory->getGolaemCharacter(iCharacter) == NULL) ? 0 : _factory->getGolaemCharacter(iCharacter)->_converterMapping._skeletonDescription->getBones().size();

            GLM_CROWD_TRACE_ERROR("There is a bone count mismatch between asset " << TCHAR_TO_ANSI(*(InCharacters[iCharacter]->GetName())) << " (" << InCharacters[iCharacter]->GetRefSkeleton().GetNum()
                                                                                  << " bones) and cache data from " << TCHAR_TO_ANSI(*gscsFilePath) << " (" << cacheCharacterToBoneCount[iCharacter]
                                                                                  << " bones). GCHA loaded in factory has " << factoryCount
                                                                                  << " bones. Either the imported UAsset character, the cache or the referenced gcha in the library is not up-to-date, this simulation won't be loaded.");
            continue;
        }

        thisCrowdFieldData.AllChars.SetNum(simuData->_entityCount);
        // else it will be detecetd empty and will look for them in LoadFrameData of BeginPlay

        bool collapseCacheInOutliner = false;
        for (uint32_t simDataEntityIdx = 0, entityCount = simuData->_entityCount; simDataEntityIdx < entityCount; ++simDataEntityIdx)
        {
            if (!glm::crowdio::isEntityValid(simuData, simDataEntityIdx))
            {
                continue;
            }

            // we must build all characters, and disable those disabled in current frame to be able to show them when they spawn without having to reinit !
            AGolaemCharacter* glmCharacterActor = NULL;

            // createCharacter
            int32_t thisEntityCharacterIndex = simuData->_characterIdx[simDataEntityIdx];

            // get meshes to keep while keeping right indices in case of multi materials

            // if we have only one section, we probably need to merge stuff
            const glm::PODArray<int>& entityAssets = allEntityAssets[simDataEntityIdx];

            collapseCacheInOutliner = InstanceMode == EInstanceMode::FOR_RIGID_MESHES || InstanceMode == EInstanceMode::DISABLED;

            SpawnActor(
                this,
                this,
                InstanceMode,
                thisEntityCharacterIndex,
                entityAssets,
                simuData,
                frameData,
                simDataEntityIdx,
                crowdFieldIndex,
                InCharacters[thisEntityCharacterIndex],
                _crowdUnitFactor,
                thisCrowdFieldData,
                TemplateCharactersData,
                world,
                AllInstancedCharacterActors);
        }

        if (collapseCacheInOutliner)
        {
            GolaemCacheEditorWrapper* golaemCacheEditorWrapper = GetGolaemCacheEditorWrapper();
            if (golaemCacheEditorWrapper->getWrapper() != NULL)
            {
                golaemCacheEditorWrapper->getWrapper()->Expand(this, false);
            }
        }

        ApplySimuShaderData(crowdFieldIndex);
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::LoadFrameData(int crowdFieldIndex, bool bInitSimuShader, bool localLock)
{
    if (DataPerCF.IsValidIndex(crowdFieldIndex))
    {
        FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];

        // update the shader data per entity
        const glm::crowdio::GlmSimulationData* simuData = thisCrowdFieldData._cachedSimulation->getFinalSimulationData();

        // check if we must find the characters (happens after PostLoad, where children characters are not built yet)
        bool needCharsUpdate = thisCrowdFieldData.AllChars.Num() == 0 && (InstanceMode == EInstanceMode::DISABLED || InstanceMode == EInstanceMode::FOR_RIGID_MESHES);
        bool needInstancesUpdate = AllInstancedCharacterActors.Num() == 0 && (InstanceMode == EInstanceMode::FOR_RIGID_MESHES || InstanceMode == EInstanceMode::FORCED);
        if (simuData != NULL && (needCharsUpdate || needInstancesUpdate))
        {
            TArray<AActor*> allChildActors;
            GetAttachedActors(allChildActors);
            TArray<AGolaemCharacter*> allChildCharacters;
            AllInstancedCharacterActors.Empty();
            AllInstancedCharacterActors.SetNumZeroed(InCharacters.Num(), true);
            for (int iChildActor = 0; iChildActor < allChildActors.Num(); iChildActor++)
            {
                AGolaemCharacter* aGolaemCharacter = Cast<AGolaemCharacter>(allChildActors[iChildActor]);
                if (aGolaemCharacter != NULL)
                {
                    allChildCharacters.Add(aGolaemCharacter);
                }
                else
                {
                    AGolaemCharacterInstanced* aGolaemCharacterInstanced = Cast<AGolaemCharacterInstanced>(allChildActors[iChildActor]);
                    if (aGolaemCharacterInstanced != NULL)
                    {
                        // find back instanced actor after reload
                        bool found = false;
#if WITH_EDITOR
                        for (int iChar = 0; iChar < InCharacters.Num() && !found; iChar++)
                        {
                            // check if label matches
                            FString characterLabel = InCharacters[iChar]->GetName() + FString("_Instances");
                            if (characterLabel == aGolaemCharacterInstanced->GetActorLabel() && AllInstancedCharacterActors[iChar] == NULL)
                            {
                                AllInstancedCharacterActors[iChar] = aGolaemCharacterInstanced;
                                found = true;
                            }
                        }
#endif

                        if (!found)
                        {
#if WITH_EDITOR
                            GLM_CROWD_TRACE_WARNING("The instanced character " << TCHAR_TO_ANSI(*aGolaemCharacterInstanced->GetActorLabel()) << " does not match any GolaemCache input character, it will be destroyed");
#endif
                            aGolaemCharacterInstanced->Destroy();
                            // destroy unfound instance character, should be rebuilt
                        }

                        // if (InCharacters.IsValidIndex(AllInstancedCharacterActors.Num() - 1) && aGolaemCharacterInstanced->InSkeletalMesh == InCharacters[AllInstancedCharacterActors.Num() - 1])
                        //{
                        //     AllInstancedCharacterActors.Add(aGolaemCharacterInstanced);
                        // }
                    }
                }
            }

            // keep track of matching simu index <-> crowdField index
            thisCrowdFieldData.AllChars.SetNum(simuData->_entityCount);

            // not optimized, look for each actor by crowdEntityId
            for (uint32_t simDataEntityIdx = 0, entityCount = simuData->_entityCount; simDataEntityIdx < entityCount; ++simDataEntityIdx)
            {
                if (!glm::crowdio::isEntityValid(simuData, simDataEntityIdx))
                {
                    continue;
                }
                int64_t entityId = simuData->_entityIds[simDataEntityIdx];
                for (int iChildChar = 0; iChildChar < allChildCharacters.Num(); iChildChar++)
                {
                    if (allChildCharacters[iChildChar]->CrowdEntityId == entityId)
                    {
                        thisCrowdFieldData.AllChars[simDataEntityIdx] = allChildCharacters[iChildChar];

                        USkeletalMeshComponent* SkeletalMeshComponent = allChildCharacters[iChildChar]->GetSkeletalMeshComponent();
                        SkeletalMeshComponent->SetUpdateAnimationInEditor(true);
                        SkeletalMeshComponent->InitAnim(true);          // init Anim after setting the parent, as we retrieve it in animNode init !
                        SkeletalMeshComponent->RefreshBoneTransforms(); // hopefully will make actor show ?
                        break;
                    }
                }
            }

            // Permet de remplir la map contenant l'ordre des bones des personnages, qui se vide quand on entre en runtime
            for (int i = 0; i < thisCrowdFieldData.AllChars.Num(); ++i)
            {
                if (thisCrowdFieldData.AllChars[i].IsValid())
                {
                    int iChar = thisCrowdFieldData.AllChars[i].Get()->CharacterIndex;
                    int cacheIdx = thisCrowdFieldData.AllChars[i].Get()->CacheIndex;
                    if (TemplateCharactersData[iChar]._boneHierarchy.Num() == 0)
                    {
                        ComputeSkeleton(thisCrowdFieldData.AllChars[i].Get(), TemplateCharactersData[iChar]._boneHierarchy);
                    }
                    // check that the found Unreal character has the same number of bones
                    if (simuData->_boneCount[simuData->_entityTypes[cacheIdx]] != TemplateCharactersData[iChar]._boneHierarchy.Num())
                    {
                        int64_t entityId = simuData->_entityIds[cacheIdx];
                        GLM_CROWD_TRACE_WARNING_LIMIT(
                            "Mismatched number of bones in Golaem Cache Node "
                            << TCHAR_TO_ANSI(*this->GetName()) << " for Golaem Entity " << entityId
                            << ". Expected number of bones is " << simuData->_boneCount[simuData->_entityTypes[cacheIdx]]
                            << ", Skeletal Mesh" /*<< TCHAR_TO_ANSI(*glmCharacterActor->GetName())*/ << " has " << TemplateCharactersData[iChar]._boneHierarchy.Num() << " bones");
                        TemplateCharactersData[iChar]._boneHierarchy.Empty();
                        continue;
                    }
                }
            }
        }

        if (simuData != NULL)
        {
            // TODO: compute the actual time based on simu data fps
            // const glm::crowdio::GlmFrameData* frameData = GetOrWaitPreloadedFrame(crowdFieldIndex);// , UINT32_MAX, true);
            const glm::crowdio::GlmFrameData* frameData = thisCrowdFieldData._cachedSimulation->getFinalFrameData(CurrentFrame, UINT32_MAX, true);

            if (frameData != NULL)
            {
                for (uint32_t simDataEntityIdx = 0, entityCount = simuData->_entityCount; simDataEntityIdx < entityCount; ++simDataEntityIdx)
                {
                    if (thisCrowdFieldData.AllChars.IsValidIndex(simDataEntityIdx) && thisCrowdFieldData.AllChars[simDataEntityIdx].IsValid())
                    {
                        int64_t entityId = simuData->_entityIds[simDataEntityIdx];
                        thisCrowdFieldData.AllChars[simDataEntityIdx].Get()->bEnabledInCache = entityId >= 0 && frameData->_entityEnabled[simDataEntityIdx] == 1;
                    }
                }
            }
            else
            {
                // only issued if we have a simdata, and with limited dump
                GLM_CROWD_TRACE_WARNING_LIMIT("Could not load frame " << CurrentFrame << " for Golaem cache " << TCHAR_TO_ANSI(*this->GetName()));
            }
        }

        ShowEntityPercent(crowdFieldIndex, !localLock);

        if (bInitSimuShader)
        {
            GLM_DEBUG_ASSERT(!localLock);
            ApplySimuShaderData(crowdFieldIndex);
        }

        // also apply shader data depending on pp attributes
        ApplyFrameShaderData(crowdFieldIndex);
    }
}

//------------------------------------------------------------
// void AGolaemCache::releaseFinalFrameDataBefore(int frame)
//{
//	if (_factory && !PreloadedFrames.empty())
//	{
//		glm::GlmSet<int>::iterator frameIte = PreloadedFrames.begin();
//		while (frameIte != PreloadedFrames.end() && frameIte.getValue() < frame)
//		{
//			int frameToUnlock = frameIte.getValue();
//			glm::GlmSet<int>::iterator frameIteToDelete = frameIte;
//			for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
//			{
//				FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];
//
//				// simu must exist, it is created in initSimulation before the preload thread
//				thisCrowdFieldData._cachedSimulation = &_factory->getCachedSimulation(TCHAR_TO_ANSI(*CacheDirectory), TCHAR_TO_ANSI(*CacheName), TCHAR_TO_ANSI(*CrowdFields[crowdFieldIndex]));
//				thisCrowdFieldData._cachedSimulation->unlockFrame(frameToUnlock, -1);
//			}
//
//			frameIte++;
//			PreloadedFrames.erase(frameIteToDelete);
//		}
//	}
//}

void AGolaemCache::ApplySimuShaderData(
    FPerSimData& PerSimData,
    const glm::crowdio::GlmSimulationData* simuData,
    const glm::ShaderAssetDataContainer* shaderDataContainer,
    const glm::PODArray<glm::GolaemCharacter*>& GolaemCharacters)
{
    if (simuData == NULL || shaderDataContainer == NULL)
    {
        return;
    }

    glm::Array<glm::Vector3> shaderValues;

    for (uint32_t simDataEntityIdx = 0, entityCount = simuData->_entityCount; simDataEntityIdx < entityCount; ++simDataEntityIdx)
    {
        int64_t entityId = simuData->_entityIds[simDataEntityIdx];
        if (entityId < 0)
        {
            // entity was probably killed
            continue;
        }

        int32_t characterIdx = simuData->_characterIdx[simDataEntityIdx];
        const glm::GolaemCharacter* character = characterIdx < GolaemCharacters.size() ? GolaemCharacters[characterIdx] : NULL;
        if (character == NULL)
        {
            continue;
        }

        shaderValues.clear(); // store values in first element if only one float or int, in 3 if vector
        shaderValues.resize(character->_shaderAttributes.size(), glm::Vector3::getNullVector());

        const glm::PODArray<int>& entityIntShaderData = shaderDataContainer->intData[simDataEntityIdx];
        const glm::PODArray<float>& entityFloatShaderData = shaderDataContainer->floatData[simDataEntityIdx];
        const glm::Array<glm::Vector3>& entityVectorShaderData = shaderDataContainer->vectorData[simDataEntityIdx];

        const glm::PODArray<size_t>& globalToSpecificShaderAttrIdx = shaderDataContainer->globalToSpecificShaderAttrIdxPerChar[characterIdx];

        // prepare list of shaderAttributes for this entity
        for (size_t iShaderAttr = 0, shaderAttrCount = character->_shaderAttributes.size(); iShaderAttr < shaderAttrCount; ++iShaderAttr)
        {
            const glm::ShaderAttribute& shaderAttr = character->_shaderAttributes[iShaderAttr];
            size_t specificAttrIdx = globalToSpecificShaderAttrIdx[iShaderAttr];

            switch (shaderAttr._type)
            {
            case glm::ShaderAttributeType::INT:
            {
                shaderValues[iShaderAttr].x = entityIntShaderData[specificAttrIdx];
            }
            break;
            case glm::ShaderAttributeType::FLOAT:
            {
                shaderValues[iShaderAttr].x = entityFloatShaderData[specificAttrIdx];
            }
            break;
            case glm::ShaderAttributeType::VECTOR:
            {
                shaderValues[iShaderAttr] = entityVectorShaderData[specificAttrIdx];
            }
            break;
            default:
                break;
            }

        } // end for each shader attribute

        // check materials, get parameters and if at least some names match, duplicate the material as instance

        if (!PerSimData.AllChars.IsValidIndex(simDataEntityIdx) || !PerSimData.AllChars[simDataEntityIdx].IsValid())
        {
            continue;
        }

        AGolaemCharacter* characterActor = PerSimData.AllChars[simDataEntityIdx].Get();

        TArray<FMaterialParameterInfo> scalarParamInfos;
        TArray<FGuid> scalarParamGuids;
        TArray<FMaterialParameterInfo> vectorParamInfos;
        TArray<FGuid> vectorParamGuids;

        glm::GlmMap<UMaterialInterface*, UMaterialInterface*> alreadyDoneMaterials;
        USkeletalMeshComponent* skeletonMeshComponent = characterActor->GetSkeletalMeshComponent();

        int32 NumMaterials = characterActor->GetSkeletalMeshComponent()->GetNumMaterials();
        for (int iMat = 0; iMat < NumMaterials; iMat++)
        {
            UMaterialInterface* materialInterface = characterActor->GetSkeletalMeshComponent()->GetMaterial(iMat);
            if (materialInterface != NULL)
            {
                glm::GlmMap<UMaterialInterface*, UMaterialInterface*>::const_iterator matIte = alreadyDoneMaterials.find(materialInterface);
                if (matIte == alreadyDoneMaterials.end())
                {
                    scalarParamInfos.Empty();
                    scalarParamGuids.Empty();
                    vectorParamInfos.Empty();
                    vectorParamGuids.Empty();

                    UMaterialInstanceDynamic* dynMaterial = Cast<UMaterialInstanceDynamic>(materialInterface);

                    materialInterface->GetAllScalarParameterInfo(scalarParamInfos, scalarParamGuids);
                    for (int iScalarParam = 0; iScalarParam < scalarParamInfos.Num(); iScalarParam++)
                    {
                        ANSICHAR scalarParamName[NAME_SIZE];
                        scalarParamInfos[iScalarParam].Name.GetPlainANSIString(scalarParamName);
                        for (size_t iShaderAttr = 0, shaderAttrCount = character->_shaderAttributes.size(); iShaderAttr < shaderAttrCount; iShaderAttr++)
                        {
                            const glm::ShaderAttribute& shaderAttr = character->_shaderAttributes[iShaderAttr];
                            if ((shaderAttr._type == glm::ShaderAttributeType::INT || shaderAttr._type == glm::ShaderAttributeType::FLOAT) && shaderAttr._name == scalarParamName)
                            {
                                if (dynMaterial == NULL)
                                {
                                    dynMaterial = skeletonMeshComponent->CreateAndSetMaterialInstanceDynamic(iMat);
                                }
                                int parameterIndex;
                                dynMaterial->InitializeScalarParameterAndGetIndex(scalarParamInfos[iScalarParam].Name, shaderValues[iShaderAttr].x, parameterIndex);
                                if (shaderAttr._valueType == glm::ShaderAttributeValueType::ATTRIBUTE)
                                {
                                    // register to set this param each frame for this material of index "iMat" and scalar parameter "parameterIndex" with shader data at index iShaderAttr
                                    characterActor->AddShaderScalarParameter(iMat, parameterIndex, iShaderAttr);
                                }
                            }
                        }
                    }

                    materialInterface->GetAllVectorParameterInfo(vectorParamInfos, vectorParamGuids);
                    for (int iVectorParam = 0; iVectorParam < vectorParamInfos.Num(); iVectorParam++)
                    {
                        ANSICHAR vectorParamName[NAME_SIZE];
                        vectorParamInfos[iVectorParam].Name.GetPlainANSIString(vectorParamName);
                        for (size_t iShaderAttr = 0, shaderAttrCount = character->_shaderAttributes.size(); iShaderAttr < shaderAttrCount; iShaderAttr++)
                        {
                            const glm::ShaderAttribute& shaderAttr = character->_shaderAttributes[iShaderAttr];
                            if (shaderAttr._type == glm::ShaderAttributeType::VECTOR && shaderAttr._name == vectorParamName)
                            {
                                if (dynMaterial == NULL)
                                {
                                    dynMaterial = skeletonMeshComponent->CreateAndSetMaterialInstanceDynamic(iMat);
                                }
                                int parameterIndex;
                                dynMaterial->InitializeVectorParameterAndGetIndex(
                                    vectorParamInfos[iVectorParam].Name,
                                    FLinearColor(shaderValues[iShaderAttr].x, shaderValues[iShaderAttr].y, shaderValues[iShaderAttr].z), parameterIndex);

                                if (shaderAttr._valueType == glm::ShaderAttributeValueType::ATTRIBUTE)
                                {
                                    // register to set this param each frame for this material of index "iMat" and Vector parameter "parameterIndex" with shader data at index iShaderAttr
                                    characterActor->AddShaderVectorParameter(iMat, parameterIndex, iShaderAttr);
                                }
                            }
                        }
                    }

                    if (dynMaterial != NULL)
                        alreadyDoneMaterials[materialInterface] = dynMaterial;
                    else
                        alreadyDoneMaterials[materialInterface] = materialInterface;
                }
                else
                {
                    characterActor->GetSkeletalMeshComponent()->SetMaterial(iMat, matIte.getValue());
                }
            } // end if materialInterface not null
        }
    }
}

//------------------------------------------------------------
// based on glm::computeCharacterShaderData()
void AGolaemCache::ApplySimuShaderData(int crowdFieldIndex)
{
    FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];
    const glm::crowdio::GlmSimulationData* simuData = thisCrowdFieldData._cachedSimulation->getFinalSimulationData();

    const glm::ShaderAssetDataContainer* shaderDataContainer = thisCrowdFieldData._cachedSimulation->getFinalShaderData(CurrentFrame, UINT32_MAX, true);

    ApplySimuShaderData(thisCrowdFieldData, simuData, shaderDataContainer, _factory->getGolaemCharacters());
}

//------------------------------------------------------------
void AGolaemCache::ApplyFrameShaderData(
    FPerSimData& PerSimData,
    const glm::ShaderAssetDataContainer* shaderDataContainer,
    const glm::PODArray<glm::GolaemCharacter*>& GolaemCharacters)
{
    if (shaderDataContainer == NULL)
    {
        return;
    }

    for (int iChar = 0; iChar < PerSimData.AllChars.Num(); iChar++)
    {
        if (PerSimData.AllChars[iChar].IsValid())
        {
            AGolaemCharacter* thisCharacter = PerSimData.AllChars[iChar].Get();
            const glm::PODArray<int>& entityIntShaderData = shaderDataContainer->intData[thisCharacter->CacheIndex];
            const glm::PODArray<float>& entityFloatShaderData = shaderDataContainer->floatData[thisCharacter->CacheIndex];
            const glm::Array<glm::Vector3>& entityVectorShaderData = shaderDataContainer->vectorData[thisCharacter->CacheIndex];

            const glm::PODArray<size_t>& globalToSpecificShaderAttrIdx = shaderDataContainer->globalToSpecificShaderAttrIdxPerChar[thisCharacter->CharacterIndex];

            const glm::GolaemCharacter* glmCharacter = thisCharacter->CharacterIndex < GolaemCharacters.size() ? GolaemCharacters[thisCharacter->CharacterIndex] : NULL;
            if (glmCharacter == NULL)
            {
                continue;
            }

            USkeletalMeshComponent* skelComp = thisCharacter->GetSkeletalMeshComponent();
            for (int iMatParam = 0; iMatParam < thisCharacter->DynamicMaterials.Num(); iMatParam++)
            {
                FDynamicMaterialInfo& dynMat = thisCharacter->DynamicMaterials[iMatParam];
                const glm::ShaderAttribute& shaderAttr = glmCharacter->_shaderAttributes[dynMat.shaderAttrIndex];
                size_t specificAttrIdx = globalToSpecificShaderAttrIdx[dynMat.shaderAttrIndex];
                if (dynMat.ScalarParameterIndex >= 0)
                {
                    UMaterialInstanceDynamic* matInterface = Cast<UMaterialInstanceDynamic>(skelComp->GetMaterial(dynMat.MaterialIndex));
                    float value = 0;
                    switch (shaderAttr._type)
                    {
                    case glm::ShaderAttributeType::INT:
                    {
                        value = entityIntShaderData[specificAttrIdx];
                    }
                    break;
                    case glm::ShaderAttributeType::FLOAT:
                    {
                        value = entityFloatShaderData[specificAttrIdx];
                    }
                    break;
                    default:
                        break;
                    }
                    matInterface->SetScalarParameterByIndex(dynMat.ScalarParameterIndex, value);
                    // else should randomize ! let the default value
                }
                if (dynMat.VectorParameterIndex >= 0)
                {
                    UMaterialInstanceDynamic* matInterface = Cast<UMaterialInstanceDynamic>(skelComp->GetMaterial(dynMat.MaterialIndex));
                    glm::Vector3 value(0.f);
                    if (shaderAttr._type == glm::ShaderAttributeType::VECTOR)
                    {
                        value = entityVectorShaderData[specificAttrIdx];
                    }
                    matInterface->SetVectorParameterByIndex(dynMat.ScalarParameterIndex, FLinearColor(value[0], value[1], value[2]));
                    // else should randomize ! let the default value
                }
            }
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::ApplyFrameShaderData(int crowdFieldIndex)
{
    FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];

    // update the shader data per entity
    const glm::ShaderAssetDataContainer* shaderDataContainer = thisCrowdFieldData._cachedSimulation->getFinalShaderData(CurrentFrame, UINT32_MAX, true);

    ApplyFrameShaderData(thisCrowdFieldData, shaderDataContainer, _factory->getGolaemCharacters());
}

#if WITH_EDITOR
//-------------------------------------------------------------------------
FString AGolaemCache::ExportTerrain(TArray<AActor*>& landscapeProxies)
{
    FString returnValue;
    void* ParentWindowHandle = nullptr;
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (DesktopPlatform)
    {
        TArray<FString> OutFilesnames;
        FString DialogTitle = TEXT("Choose a save location for the terrain file");
        FString DefaultPath = FPaths::ProjectDir();
        uint32 SelectionFlag = 0;
        DesktopPlatform->SaveFileDialog(ParentWindowHandle, DialogTitle, DefaultPath, FString(""), FString(".gtg"), SelectionFlag, OutFilesnames);

        if (OutFilesnames.Num() == 1)
        {
            FString terrainPath = OutFilesnames[0];
            if (!OutFilesnames[0].EndsWith(".gtg")) // else let user call its file as he wants
            {
                if (!OutFilesnames[0].EndsWith("._terrain"))
                {
                    terrainPath = OutFilesnames[0] + "_terrain.gtg";
                }
                else
                {
                    terrainPath = OutFilesnames[0] + ".gtg";
                }
            }
            char* terrainPathName = TCHAR_TO_ANSI(*terrainPath);

            if (landscapeProxies.Num() > 0)
            {
                float crowdUnitFactor = GetSceneUnitFactor();
                int meshCount = 0;
                for (int i = 0; i < landscapeProxies.Num(); ++i)
                {
                    // check for landscape, static meshes, etc
                    if (landscapeProxies[i] != nullptr)
                    {
                        if (landscapeProxies[i]->IsA<ALandscapeProxy>() || landscapeProxies[i]->IsA<AStaticMeshActor>())
                        {
                            meshCount++;
                        }
                    }
                }

                glm::crowdio::GlmFileMesh* meshesToExport = new glm::crowdio::GlmFileMesh[meshCount];
                glm::crowdio::GlmFileMeshTransform* transformsToExport = new glm::crowdio::GlmFileMeshTransform[meshCount];
                memset(meshesToExport, 0, sizeof(glm::crowdio::GlmFileMesh) * meshCount);
                memset(transformsToExport, 0, sizeof(glm::crowdio::GlmFileMeshTransform) * meshCount);

                int cptMesh = 0;
                for (int iTerrain = 0; iTerrain < landscapeProxies.Num(); ++iTerrain)
                {
                    if (landscapeProxies[iTerrain] != nullptr)
                    {
                        FMeshDescription meshDescription;
                        FMeshDescription* meshDescriptionPtr = nullptr;

                        const FTransform& actorWorldTransform = landscapeProxies[iTerrain]->GetTransform();
                        if (landscapeProxies[iTerrain]->IsA<ALandscapeProxy>())
                        {
                            ALandscapeProxy* landscapeProxy = Cast<ALandscapeProxy>(landscapeProxies[iTerrain]);

                            FStaticMeshAttributes(meshDescription).Register();
                            ALandscapeProxy::FRawMeshExportParams ExportParams;
                            ExportParams.ExportLOD = INDEX_NONE;

                            landscapeProxy->ExportToRawMesh(ExportParams, meshDescription);
                            meshDescriptionPtr = &meshDescription;
                        }
                        else if (landscapeProxies[iTerrain]->IsA<AStaticMeshActor>())
                        {
                            AStaticMeshActor* staticMeshActor = Cast<AStaticMeshActor>(landscapeProxies[iTerrain]);
                            meshDescriptionPtr = staticMeshActor->GetStaticMeshComponent()->GetStaticMesh()->GetMeshDescription(0); // request terrain at LODIndex 0
                        }

                        if (meshDescriptionPtr != nullptr && meshDescriptionPtr->Vertices().Num() > 0)
                        {
                            //-------- Compacting the raw mesh

                            // FMeshDescription MeshDescription;
                            // TMap<int32, FName> InMap1;
                            // FMeshDescriptionOperations::ConvertFromRawMesh(meshDescription, MeshDescription, InMap1);

                            // FElementIDRemappings ElementIDRemap;
                            // MeshDescription.Compact(ElementIDRemap);

                            // TMap<FName, int32> InMap2;
                            // FMeshDescriptionOperations::ConvertToRawMesh(MeshDescription, meshDescription, InMap2);

                            //------------
                            glm::crowdio::GlmFileMesh& meshToExport = meshesToExport[cptMesh];
                            glm::crowdio::GlmFileMeshTransform& transformToExport = transformsToExport[cptMesh];

                            glm::crowdio::glmFileSetString(meshToExport._meshName, TCHAR_TO_ANSI(*landscapeProxies[iTerrain]->GetName()));
                            meshToExport._uvSetCount = 0;
                            meshToExport._uvCoordCount = 0;
                            meshToExport._normalMode = glm::crowdio::GLM_NORMAL_PER_POLYGON_VERTEX;
                            meshToExport._uvSetNames = NULL;
                            meshToExport._triangleCount = 0;
                            meshToExport._polygonCount = 0;
                            meshToExport._vertexCount = 0;
                            meshToExport._vertexCacheFrameCount = 0;
                            meshToExport._blendShapeCount = 0;
                            meshToExport._skinningType = 0;
                            meshToExport._perPolyTriangleTotalCount = 0;

                            transformToExport._rigidSkinningBoneId = 0;
                            transformToExport._meshIndex = cptMesh;
                            glm::crowdio::glmFileSetString(transformToExport._referenceName, TCHAR_TO_ANSI(*landscapeProxies[iTerrain]->GetName()));
                            transformToExport._animTransformCount = 0;

                            FMatrix actorMX = actorWorldTransform.ToMatrixWithScale();

                            FMatrix crowdUnitScaleMx;
                            crowdUnitScaleMx.SetIdentity();
                            crowdUnitScaleMx = crowdUnitScaleMx.ApplyScale(1 / crowdUnitFactor);

                            FMatrix finalMx = crowdUnitScaleMx * actorMX;

                            // set transform matrix
                            FMatrix finalMxAssetReferential = MatUEToGda * finalMx * MatUEToGda; // note that MatUEToGda == MatUEToGda.inverse()
                            memcpy(&transformToExport._defaultLocalToWorldMatrix[0], &finalMxAssetReferential.M[0][0], sizeof(float) * 16);

                            // just to be sure we are triangulated
                            meshDescriptionPtr->TriangulateMesh();
                            const TVertexAttributesConstRef<FVector3f>& VertexPositions = meshDescriptionPtr->VertexAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Vertex::Position);
                            const TVertexInstanceAttributesConstRef<FVector3f>& Normals = meshDescriptionPtr->VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Normal);
                            const TVertexInstanceAttributesConstRef<FVector3f>& Tangents = meshDescriptionPtr->VertexInstanceAttributes().GetAttributesRef<FVector3f>(MeshAttribute::VertexInstance::Tangent);

                            meshToExport._triangleCount = meshDescriptionPtr->Polygons().Num();
                            meshToExport._vertexCount = VertexPositions.GetNumElements();
                            meshToExport._normalCount = Normals.GetNumElements();

                            meshToExport._vertices = (glm::crowdio::GlmFileMeshVertex*)GLM_ALLOC_NOPROFILING(sizeof(glm::crowdio::GlmFileMeshVertex) * meshToExport._vertexCount, glm::crowdio::MemoryStatIds::GolaemCrowdIO);
                            memset(meshToExport._vertices, 0, sizeof(glm::crowdio::GlmFileMeshVertex) * meshToExport._vertexCount);

                            meshToExport._normals = (float(*)[3])GLM_ALLOC_NOPROFILING(meshToExport._normalCount * sizeof(float[3]), glm::crowdio::MemoryStatIds::GolaemCrowdIO);
                            memset(meshToExport._normals, 0, sizeof(float[3]) * meshToExport._normalCount);

                            // things untouched :  we don't export polys other than triangles, normals will be indexed, by poly
                            meshToExport._uvSetCount = 0;
                            meshToExport._blendShapeCount = 0;
                            meshToExport._polygonCount = 0;
                            meshToExport._vertexCacheFrameCount = 0;
                            meshToExport._normalMode = glm::crowdio::GLM_NORMAL_PER_POLYGON_VERTEX_INDEXED;

                            meshToExport._trianglesVertexIndices = (uint32_t(*)[3])GLM_ALLOC_NOPROFILING(meshToExport._triangleCount * 3 * sizeof(uint32_t), glm::crowdio::MemoryStatIds::GolaemCrowdIO);
                            memset(meshToExport._trianglesVertexIndices, 0, meshToExport._triangleCount * 3 * sizeof(uint32_t));

                            meshToExport._trianglesNormalIndices = (uint32_t(*)[3])GLM_ALLOC_NOPROFILING(meshToExport._triangleCount * 3 * sizeof(uint32_t), glm::crowdio::MemoryStatIds::GolaemCrowdIO);
                            memset(meshToExport._trianglesNormalIndices, 0, meshToExport._triangleCount * 3 * sizeof(uint32_t));

                            // meshToExport._trianglesUVIndices = (uint32_t(*)[3])GLM_ALLOC_NOPROFILING(meshToExport._triangleCount * 3 * sizeof(uint32_t), glm::crowdio::MemoryStatIds::GolaemCrowdIO);
                            // memset(meshToExport._trianglesUVIndices, 0, meshToExport._triangleCount * 3 * sizeof(uint32_t));

                            FVertexID VertexID;
                            for (int iVertex = 0; iVertex < static_cast<int>(meshToExport._vertexCount); ++iVertex)
                            {
                                VertexID = FVertexID(iVertex);
                                meshToExport._vertices[iVertex]._position[0] = VertexPositions[VertexID].X;
                                meshToExport._vertices[iVertex]._position[2] = VertexPositions[VertexID].Y;
                                meshToExport._vertices[iVertex]._position[1] = VertexPositions[VertexID].Z;
                            }

                            FVertexInstanceID VertexInstanceID;
                            for (int iVertex = 0; iVertex < static_cast<int>(meshToExport._normalCount); ++iVertex)
                            {
                                VertexInstanceID = FVertexInstanceID(iVertex);
                                meshToExport._normals[iVertex][0] = Normals[VertexInstanceID].X;
                                meshToExport._normals[iVertex][2] = Normals[VertexInstanceID].Y;
                                meshToExport._normals[iVertex][1] = Normals[VertexInstanceID].Z;
                            }

                            int32 TriangleIndex = 0;
                            const FPolygonArray& polygonsArray = meshDescriptionPtr->Polygons();
                            FPolygonArray::TElementIDs elementIds = polygonsArray.GetElementIDs();
                            for (const FPolygonID& PolygonID : elementIds)
                            {
                                const TArray<FTriangleID>& TriangleIds = meshDescriptionPtr->GetPolygonTriangleIDs(PolygonID);
                                for (const FTriangleID TriangleID : TriangleIds)
                                {
                                    for (int32 Corner = 0; Corner < 3; ++Corner)
                                    {
                                        FVertexInstanceID vertexInstanceID = meshDescriptionPtr->GetTriangleVertexInstance(TriangleID, Corner);
                                        meshToExport._trianglesVertexIndices[TriangleIndex][Corner] = meshDescriptionPtr->GetVertexInstanceVertex(vertexInstanceID).GetValue();
                                        meshToExport._trianglesNormalIndices[TriangleIndex][Corner] = vertexInstanceID.GetValue();
                                        // iTriangleID++;
                                    }
                                    TriangleIndex++;
                                }
                            }
                            // meshDescription.Empty();

                            cptMesh++;
                        }
                    }
                }
                glm::crowdio::crowdTerrain::glmWriteTerrainFile(terrainPathName, meshCount, meshesToExport, meshCount, transformsToExport);
                returnValue = terrainPath;

                for (int iMesh = 0; iMesh < meshCount; ++iMesh)
                {
                    GLM_FREE_NOPROFILING(meshesToExport[iMesh]._vertices, glm::crowdio::MemoryStatIds::GolaemCrowdIO);
                    GLM_FREE_NOPROFILING(meshesToExport[iMesh]._normals, glm::crowdio::MemoryStatIds::GolaemCrowdIO);
                    GLM_FREE_NOPROFILING(meshesToExport[iMesh]._trianglesVertexIndices, glm::crowdio::MemoryStatIds::GolaemCrowdIO);
                    GLM_FREE_NOPROFILING(meshesToExport[iMesh]._trianglesNormalIndices, glm::crowdio::MemoryStatIds::GolaemCrowdIO);
                    // GLM_FREE_NOPROFILING( meshesToExport[iMesh]._trianglesUVIndices, glm::crowdio::MemoryStatIds::GolaemCrowdIO);
                }
                delete[] meshesToExport;
                meshesToExport = NULL;

                delete[] transformsToExport;
                transformsToExport = NULL;
            }
        }
    }

    return returnValue;
}

#endif // editor

//-------------------------------------------------------------------------
bool AGolaemCache::GetBoneSpaceTransforms_AnyThread(int simulationIndex, int entityIndex, TArray<FTransform>& boneSpaceTransforms, TArray<float>& morphTargetWeights, float& entityCurrentTime)
{
    glm::ScopedLockRW<glm::RWSpinLock, false> lockForRead(CacheRWLock);

    if (simulationIndex < DataPerCF.Num())
    {
        FCrowdFieldData& thisCrowdFieldData = DataPerCF[simulationIndex];
        const glm::crowdio::GlmSimulationData* simuData = thisCrowdFieldData._cachedSimulation->getFinalSimulationData();
        const glm::crowdio::GlmFrameData* frameData = thisCrowdFieldData._cachedSimulation->getFinalFrameData(CurrentFrame, UINT32_MAX, true);

        if (simuData != NULL &&
            frameData != NULL &&
            (uint32_t)entityIndex < simuData->_entityCount &&
            thisCrowdFieldData.AllChars.IsValidIndex(entityIndex) &&
            thisCrowdFieldData.AllChars[entityIndex].IsValid())
        {
            AGolaemCharacter* currentChar = thisCrowdFieldData.AllChars[entityIndex].Get();

            USceneComponent* cacheProxyRootComponent = GetRootComponent();
            const FTransform& cacheProxyTransform = cacheProxyRootComponent->GetComponentTransform();

            USkeletalMeshComponent* SkelMeshComp = currentChar->GetSkeletalMeshComponent();
            USkeletalMesh* skelMesh = SkelMeshComp->SkeletalMesh;

            TArray<FTransform> ComponentSpaceTransforms;

            if (!glm::crowdio::isEntityValid(simuData, entityIndex))
            {
                return false;
            }

            if (frameData->_entityEnabled[entityIndex] == 1 && !currentChar->IsHidden())
            {
                unsigned int entityType = simuData->_entityTypes[entityIndex];

                boneSpaceTransforms.SetNum(simuData->_boneCount[entityType]);
                ComponentSpaceTransforms.SetNum(boneSpaceTransforms.Num()); // to be able to compute relative

                FVector BonePosition;
                FQuat BoneOrientation;

                const TArray<int32>& entityBoneHierarchy(TemplateCharactersData[currentChar->CharacterIndex]._boneHierarchy);
                if (entityBoneHierarchy.Num() == ComponentSpaceTransforms.Num()) // else we have an internal error : mismatch instanced character with simData
                {
                    const FTransform& skelMeshCompTransform = SkelMeshComp->GetComponentTransform();
                    for (unsigned int iBone = 0; iBone < simuData->_boneCount[entityType]; ++iBone)
                    {
                        unsigned int index = iBone +
                                             simuData->_iBoneOffsetPerEntityType[entityType] +
                                             simuData->_indexInEntityType[entityIndex] * simuData->_boneCount[entityType];

                        BonePosition.X = _crowdUnitFactor * frameData->_bonePositions[index][0] + cacheProxyTransform.GetLocation().X;
                        // Y and Z positions are interverted because default up-axis of Maya is Y while Unreal's up-axis is Z
                        BonePosition.Y = _crowdUnitFactor * frameData->_bonePositions[index][2] + cacheProxyTransform.GetLocation().Y;
                        BonePosition.Z = _crowdUnitFactor * frameData->_bonePositions[index][1] + cacheProxyTransform.GetLocation().Z;

                        BoneOrientation.X = frameData->_boneOrientations[index][0];
                        BoneOrientation.Y = frameData->_boneOrientations[index][2];
                        BoneOrientation.Z = frameData->_boneOrientations[index][1];
                        BoneOrientation.W = -frameData->_boneOrientations[index][3];

                        BoneOrientation = BoneOrientation * QuatCacheToUE4;

                        FTransform& currentComponentSpaceTransform = ComponentSpaceTransforms[entityBoneHierarchy[iBone]];
                        FTransform& currentBonesSpaceTransform = boneSpaceTransforms[entityBoneHierarchy[iBone]];

                        currentComponentSpaceTransform.SetLocation(BonePosition);
                        currentComponentSpaceTransform.SetRotation(BoneOrientation);
                        float characterScale = simuData->_scales[entityIndex];
                        currentComponentSpaceTransform.SetScale3D(FVector(_crowdUnitFactor * characterScale, _crowdUnitFactor * characterScale, _crowdUnitFactor * characterScale));

                        int parentIndex = skelMesh->RefSkeleton.GetParentIndex(entityBoneHierarchy[iBone]);

                        currentComponentSpaceTransform.SetToRelativeTransform(skelMeshCompTransform);

                        currentBonesSpaceTransform = currentComponentSpaceTransform;

                        if (parentIndex >= 0 && parentIndex < ComponentSpaceTransforms.Num())
                        {
                            currentBonesSpaceTransform.SetToRelativeTransform(ComponentSpaceTransforms[parentIndex]);
                        }
                    }
                }
                // else // this led to a log storm (logging each frame a tremendous amount of lines) leading to ram saturation
                //{
                //     GLM_CROWD_TRACE_WARNING(
                //         "Mismatched number of bones in Golaem Cache Node "
                //         << TCHAR_TO_ANSI(*this->GetName()) << " for Golaem Entity "
                //         << simuData->_entityIds[entityIndex] << ". Expected number of bones is "
                //         << simuData->_boneCount[entityType] << ", Skeletal Mesh "
                //         << TCHAR_TO_ANSI(*currentChar->GetName()) << " has " << boneSpaceTransforms.Num() << " bones");
                // }

                if (currentChar->bMorphTargetFromAnim && currentChar->CharacterIndex >= 0 && currentChar->CharacterIndex < thisCrowdFieldData.MorphTargetsToBlindDataIndexPerChar.size())
                {
                    glm::PODArray<int32>& AllMorphTargetsToBlindDataIndexPerChar = thisCrowdFieldData.MorphTargetsToBlindDataIndexPerChar[currentChar->CharacterIndex];
                    unsigned int thisEntityBlindDataCount = simuData->_blindDataCount[entityType];
                    unsigned int thisEntityFirstBlindDataIndex = simuData->_iBlindDataOffsetPerEntityType[entityType] + thisEntityBlindDataCount * simuData->_indexInEntityType[entityIndex];

                    morphTargetWeights.Empty();
                    for (size_t iBS = 0; iBS < AllMorphTargetsToBlindDataIndexPerChar.size(); iBS++)
                    {
                        float currentMorphTargetWeight = 0.f;
                        if (AllMorphTargetsToBlindDataIndexPerChar[iBS] >= 0 && AllMorphTargetsToBlindDataIndexPerChar[iBS] < static_cast<int32>(thisEntityBlindDataCount))
                        {
                            currentMorphTargetWeight = frameData->_blindData[thisEntityFirstBlindDataIndex + AllMorphTargetsToBlindDataIndexPerChar[iBS]];
                        }
                        morphTargetWeights.Add(currentMorphTargetWeight);
                    }
                }

                // GLM_DCC_TRACE_WARNING("Getting GolaemCharacter Transform for current frame  : " << CurrentFrame);

                entityCurrentTime = CurrentFrame;
                return true;
            }
        }
    }
    return false;
}

//-------------------------------------------------------------------------
void AGolaemCache::ResetCharacterActors()
{
    TArray<AActor*> allChildActors;
    GetAttachedActors(allChildActors);
    for (int iChildActor = 0; iChildActor < allChildActors.Num(); iChildActor++)
    {
        AActor* attachedActor = allChildActors[iChildActor];
        AGolaemCharacter* aGolaemCharacter = Cast<AGolaemCharacter>(attachedActor);
        if (aGolaemCharacter != NULL)
        {
            aGolaemCharacter->Destroy();
        }
    }

    CharactersHaveBeenReset = true;
}

//-------------------------------------------------------------------------
void AGolaemCache::Destroyed()
{
    ResetCharacterActors();
    ResetCharacterActorsInstanced();

    Super::Destroyed();
}

//-------------------------------------------------------------------------
void AGolaemCache::ResetSimulation()
{
    // note : we do NOT reset UE characters or path here, we must be able to relaunch a simu just after

    // if (PreloadCacheWorker != nullptr)
    //{
    //	PreloadCacheWorker->Stop();
    //	delete PreloadCacheWorker;
    // }
    // PreloadedFrames.clear();

    // we don't keep any frame data or simu data
    DataPerCF.Empty();

    TemplateCharactersData.Empty();

    if (_factory)
    {
        _factory->setTerrainMeshes(NULL, NULL);
        if (_terrainMeshSource && _terrainMeshSource != _terrainMeshDestination)
            closeTerrainAsset(_terrainMeshSource);
        if (_terrainMeshDestination)
            closeTerrainAsset(_terrainMeshDestination);
        _terrainMeshSource = NULL;
        _terrainMeshDestination = NULL;

        _factory->clear(glm::crowdio::FactoryClearMode::ALL);
        GLM_SAFE_DELETE(_factory);
    }

    bIsSimuBuilt = false;
}

//-------------------------------------------------------------------------
void AGolaemCache::SetSelectEntitySet(glm::GlmSet<int64>& crowdEntityIdSet)
{
    // GLM_CROWD_TRACE_INFO("setting " << crowdEntityIdSet << " To Selection");
    SelectedCrowdIds = crowdEntityIdSet;
}

//-------------------------------------------------------------------------
void AGolaemCache::AppendSelectEntitySet(glm::GlmSet<int64>& crowdEntityIdSet)
{
    // GLM_CROWD_TRACE_INFO("appending" << crowdEntityIdSet << " To Selection");
    SelectedCrowdIds.insert(crowdEntityIdSet.begin(), crowdEntityIdSet.end());
}

//-------------------------------------------------------------------------
const glm::GlmSet<int64>& AGolaemCache::GetSelectedEntitiesInt() const
{
    return SelectedCrowdIds;
}

//-------------------------------------------------------------------------
void AGolaemCache::SelectEntity(int64 crowdEntityId)
{
    GLM_CROWD_TRACE_INFO("Adding " << crowdEntityId << " To Selection");
    // UE_LOG(GlmLog, Display, TEXT("Adding %d To Selection"), crowdEntityId);
    SelectedCrowdIds.insert(crowdEntityId);
}

//-------------------------------------------------------------------------
void AGolaemCache::UnselectEntity(int64 crowdEntityId)
{
    GLM_CROWD_TRACE_INFO("Removing " << crowdEntityId << " From Selection");
    // UE_LOG(GlmLog, Display, TEXT("Removing %d From Selection"), crowdEntityId);
    SelectedCrowdIds.erase(crowdEntityId);
}

//-------------------------------------------------------------------------
void AGolaemCache::TranslateSelection(glm::Vector3& translate)
{
    if (!SelectedCrowdIds.empty())
    {
        GLM_CROWD_TRACE_INFO("Translation " << SelectedCrowdIds.size() << " entities of " << translate);

        CommandIDs.clear();
        CommandIDs.insert(CommandIDs.begin(), SelectedCrowdIds.begin(), SelectedCrowdIds.end());

#if WITH_EDITOR
        OpenLayoutFile(); // be sure that the layout file is opened
        // CheckLayoutEditor();
        addOrEditLayoutTransformation(
            CommandIDs, // SelectedCrowdIds
            "Translate",
            "translate",
            (1.f / _crowdUnitFactor) * translate, //
            CurrentFrame,
            1); // mode 1: add

        refreshLayoutFromEditor();
#endif
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::RotateSelection(glm::Vector3& glmEulerRotation)
{
    if (!SelectedCrowdIds.empty())
    {
        // GLM_CROWD_TRACE_INFO("Rotation " << SelectedCrowdIds.size() <<" entities of " << Angle);

        CommandIDs.clear();
        CommandIDs.insert(CommandIDs.begin(), SelectedCrowdIds.begin(), SelectedCrowdIds.end());

#if WITH_EDITOR
        OpenLayoutFile(); // be sure that the layout file is opened

        GolaemCacheEditorWrapper* golaemCacheEditorWrapper = GetGolaemCacheEditorWrapper();
        FVector EditorWorldPivotLocation(0, 0, 0);
        if (golaemCacheEditorWrapper->getWrapper() != NULL)
        {
            EditorWorldPivotLocation = golaemCacheEditorWrapper->getWrapper()->GetPivotLocation();
        }
        glm::Vector3 pivotLocation(EditorWorldPivotLocation.X, EditorWorldPivotLocation.Z, EditorWorldPivotLocation.Y);
        pivotLocation *= (1 / _crowdUnitFactor);

        addOrEditLayoutTransformation(
            CommandIDs, // SelectedCrowdIds
            "Rotate",
            "usePivot",
            true, //
            CurrentFrame,
            0); // mode 0: set

        addOrEditLayoutTransformation(
            CommandIDs, // SelectedCrowdIds
            "Rotate",
            "pivot",
            pivotLocation, //
            CurrentFrame,
            0); // mode 0: set

        // convert to degrees
        // glm::Vector3 eulerRotationInDegrees((float)(GLM_RAD_2_DEG * glmEulerRotation.x), (float)(GLM_RAD_2_DEG * glmEulerRotation.y), (float)(GLM_RAD_2_DEG * glmEulerRotation.z));

        addOrEditLayoutTransformation(
            CommandIDs, // SelectedCrowdIds
            "Rotate",
            "rotate",
            glmEulerRotation, //
            CurrentFrame,
            1); // mode 1: add

        refreshLayoutFromEditor();
#endif
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::ScaleSelection(float scale, glm::Vector3& pivot)
{
    if (!SelectedCrowdIds.empty())
    {
        // GLM_CROWD_TRACE_INFO("Scaling " << SelectedCrowdIds.size() << " entities of " << scale << " Around " << pivot);

        CommandIDs.clear();
        CommandIDs.insert(CommandIDs.begin(), SelectedCrowdIds.begin(), SelectedCrowdIds.end());

#if WITH_EDITOR

        OpenLayoutFile(); // be sure that the layout file is opened

        pivot *= (1 / _crowdUnitFactor);

        addOrEditLayoutTransformation(
            CommandIDs, // SelectedCrowdIds
            "Expand",
            "pivot",
            pivot, //
            CurrentFrame,
            0); // mode 0: set

        addOrEditLayoutTransformation(
            CommandIDs, // SelectedCrowdIds
            "Expand",
            "referenceFrameIndex",
            CurrentFrame, //
            CurrentFrame,
            0); // mode 0: set

        addOrEditLayoutTransformation(
            CommandIDs, // SelectedCrowdIds
            "Expand",
            "scale",
            scale, //
            CurrentFrame,
            2); // mode 2: mult

        refreshLayoutFromEditor();
#endif
    }
}

//-------------------------------------------------------------------------
void AGolaemCache::ScaleSelection(float scale)
{
    // if (!(SelectedCrowdIds.empty()) && crowdEntityId == *SelectedCrowdIds.begin())
    {
        GLM_CROWD_TRACE_INFO("Scaling " << SelectedCrowdIds.size() << " entities of " << scale);

        CommandIDs.clear();
        CommandIDs.insert(CommandIDs.begin(), SelectedCrowdIds.begin(), SelectedCrowdIds.end());

#if WITH_EDITOR
        OpenLayoutFile(); // be sure that the layout file is opened

        if (SelectedCrowdIds.size() > 1)
        {
            GLM_CROWD_TRACE_WARNING("Scaling is only handled on one entity from the viewport (scaling several entities in viewport gives weird results, scaling all from last selected entity pivot, but each in its local scale)");
        }
        else
        {
            GolaemCacheEditorWrapper* golaemCacheEditorWrapper = GetGolaemCacheEditorWrapper();
            FVector EditorWorldPivotLocation(0, 0, 0);
            if (golaemCacheEditorWrapper->getWrapper() != NULL)
            {
                EditorWorldPivotLocation = golaemCacheEditorWrapper->getWrapper()->GetPivotLocation();
            }

            glm::Vector3 pivotLocation(EditorWorldPivotLocation.X, EditorWorldPivotLocation.Z, EditorWorldPivotLocation.Y);

            addOrEditLayoutTransformation(
                CommandIDs, // SelectedCrowdIds
                "Scale",
                "pivot",
                pivotLocation, //
                CurrentFrame,
                0); // mode 0: set

            addOrEditLayoutTransformation(
                CommandIDs, // SelectedCrowdIds
                "Scale",
                "scale",
                scale, //
                CurrentFrame,
                2); // mode 2: mult
        }

        refreshLayoutFromEditor();
#endif
    }
}

#if WITH_EDITOR

//-----------------------------------------------------------------------------
void AGolaemCache::addOrEditLayoutTransformation(const glm::PODArray<int64_t>& entitiesToTransform, const char* transformationNodeTypeName, const char* parameterName, float parameterValue, float frame, int operationMode)
{
    // force enable layout to display transformation or we will stack "invisible" transfos
    bEnableLayout = true;

    FString layoutIdName = TCHAR_TO_ANSI(*getEditedLayoutName());

    GEngine->Exec(NULL, TEXT("py import glm.layout.layoutEditorWrapper as layoutWrapper\n"));
    GEngine->Exec(NULL, TEXT("py wrapper = layoutWrapper.getTheLayoutEditorWrapperInstance(True)\n"));

    std::stringstream addOrEditFloatCmd;
    addOrEditFloatCmd << "py wrapper.addOrEditLayoutTransformation(\"";
    addOrEditFloatCmd << TCHAR_TO_ANSI(*layoutIdName);
    addOrEditFloatCmd << "\"";
    {
        addOrEditFloatCmd << ",\"";
        for (size_t entityIndex = 0; entityIndex < entitiesToTransform.size(); ++entityIndex)
        {
            if (entityIndex > 0)
                addOrEditFloatCmd << ",";
            addOrEditFloatCmd << entitiesToTransform[entityIndex];
        }
        addOrEditFloatCmd << "\"";
    }
    addOrEditFloatCmd << ", \"" << transformationNodeTypeName << "\"";
    addOrEditFloatCmd << ", \"" << parameterName << "\"";
    {
        addOrEditFloatCmd << ",[" << parameterValue << "]";
    }
    addOrEditFloatCmd << ", " << frame;
    addOrEditFloatCmd << ", " << operationMode;
    addOrEditFloatCmd << ")\n";

    FString addOrEditFloatFStringCmd = addOrEditFloatCmd.str().c_str();
    GEngine->Exec(NULL, *addOrEditFloatFStringCmd);
}

//-----------------------------------------------------------------------------
void AGolaemCache::addOrEditLayoutTransformation(const glm::PODArray<int64_t>& entitiesToTransform, const char* transformationNodeTypeName, const char* parameterName, const glm::Vector3& parameterValue, float frame, int operationMode)
{
    // force enable layout to display transformation or we will stack "invisible" transfos
    bEnableLayout = true;

    FString layoutIdName = TCHAR_TO_ANSI(*getEditedLayoutName());

    GEngine->Exec(NULL, TEXT("py import glm.layout.layoutEditorWrapper as layoutWrapper\n"));
    GEngine->Exec(NULL, TEXT("py wrapper = layoutWrapper.getTheLayoutEditorWrapperInstance(True)\n"));

    std::stringstream addOrEditVecCmd;
    addOrEditVecCmd << "py wrapper.addOrEditLayoutTransformation(\"";
    addOrEditVecCmd << TCHAR_TO_ANSI(*layoutIdName);
    addOrEditVecCmd << "\"";
    {
        addOrEditVecCmd << ",\"";
        for (size_t entityIndex = 0; entityIndex < entitiesToTransform.size(); ++entityIndex)
        {
            if (entityIndex > 0)
                addOrEditVecCmd << ",";
            addOrEditVecCmd << entitiesToTransform[entityIndex];
        }
        addOrEditVecCmd << "\"";
    }
    addOrEditVecCmd << ", \"" << transformationNodeTypeName << "\"";
    addOrEditVecCmd << ", \"" << parameterName << "\"";
    {
        addOrEditVecCmd << ",[[" << parameterValue.x << "," << parameterValue.y << "," << parameterValue.z << "]]";
    }
    addOrEditVecCmd << ", " << frame;
    addOrEditVecCmd << ", " << operationMode;
    addOrEditVecCmd << ")\n";

    FString addOrEditVecFStringCmd = addOrEditVecCmd.str().c_str();
    GEngine->Exec(NULL, *addOrEditVecFStringCmd);
}

//-----------------------------------------------------------------------------
bool AGolaemCache::TryGetTransformLock()
{
    return TransformLock.trylock();
}

//-----------------------------------------------------------------------------
void AGolaemCache::ReleaseTransformLock()
{
    TransformLock.unlock();
    refreshLayoutFromEditor(); // at edition end, refresh the layout from editor

    // Animation will be updated while dragging, update instances to follow
    for (int iInstanceCharacter = 0; iInstanceCharacter < AllInstancedCharacterActors.Num(); iInstanceCharacter++)
    {
        if (AllInstancedCharacterActors[iInstanceCharacter].Get() != NULL)
        {
            AllInstancedCharacterActors[iInstanceCharacter].Get()->setTransformDirty();
        }
    }
}

//-----------------------------------------------------------------------------
void AGolaemCache::refreshLayoutFromEditor()
{
    FString layoutIdName = getEditedLayoutName();

    GEngine->Exec(NULL, TEXT("py import glm.layout.layoutEditorWrapper as layoutWrapper\n"));
    GEngine->Exec(NULL, TEXT("py wrapper = layoutWrapper.getTheLayoutEditorWrapperInstance(True)\n"));

    std::stringstream refreshLayoutCmd;
    refreshLayoutCmd << "py wrapper.sendLayout(\"";
    refreshLayoutCmd << TCHAR_TO_ANSI(*layoutIdName);
    refreshLayoutCmd << "\", False);"; // false : do not refresh assets
    FString refreshLayoutFStringCmd = refreshLayoutCmd.str().c_str();
    GEngine->Exec(NULL, *refreshLayoutFStringCmd);

    // refreshLayoutCmd << "py wrapper.saveCacheProxyTab(\"";
    // refreshLayoutCmd << TCHAR_TO_ANSI(*layoutIdName);
    // refreshLayoutCmd << "\")";

    // FString refreshLayoutFStringCmd = refreshLayoutCmd.str().c_str();
    // GEngine->Exec(NULL, *refreshLayoutFStringCmd);

    //// then force reload the layout file :
    // if (_factory && layoutIndex < LayoutFilesParsed.sizeInt() && !LayoutFilesParsed[layoutIndex].empty())
    //{
    //	_factory->loadLayoutHistoryFile(layoutIndex, LayoutFilesParsed[layoutIndex].c_str());
    // }
}

//-----------------------------------------------------------------------------
FString AGolaemCache::GetSelectedEntities() const
{
    FString returnValue;
    GolaemCacheEditorWrapper* golaemCacheEditorWrapper = GetGolaemCacheEditorWrapper();
    if (golaemCacheEditorWrapper->getWrapper() != NULL)
    {
        returnValue = golaemCacheEditorWrapper->getWrapper()->GetSelectedEntities(this->GetName());
    }

    return returnValue;
}

//-----------------------------------------------------------------------------
void AGolaemCache::ReloadLayout(int layoutIndex, const FString& layoutJSON, const TArray<int>& dirtyLayoutNodeID, bool doRefreshAssets)
{
    // if (doRefreshAssets)
    //	_factory->clear(glm::crowdio::FactoryClearMode::CHARACTERS);

    if (_factory && layoutJSON.Len() > 0)
    {
        // clear cache parts that needs to be cleared
        if (layoutIndex != UINT32_MAX && bEnableLayout) // else the cacheProxy has not been computed yet (no draw occured between the creation of the history and this (python) call)
        {
            // glm::Array<uint32_t> dirtyNodesIDs;
            // for (int iDirtyNode = 0; iDirtyNode < dirtyLayoutNodeID.Num(); iDirtyNode++)
            //	dirtyNodesIDs.push_back(dirtyLayoutNodeID[iDirtyNode]);

            _factory->clear(glm::crowdio::FactoryClearMode::ALL_MODIFIED);
            // ToDo : doClearCachePropagation(_factory, layoutIndex, TCHAR_TO_ANSI(*layoutJSON), dirtyNodesIDs);

            // if (_layoutRootNodeID < 0 && _dirtyLayoutNodeID.length() == 0 && !_dirtyAssetRepartitionSet)
            //{
            //	GLM_CROWD_TRACE_WARNING("Setting a new layout content without dirtying any node or changing the root node ID. The cache factory won't be refreshed automatically, needs to be manually refreshed from the cache proxy.");
            // }

            if (_factory->loadLayoutHistoryContent(layoutIndex, TCHAR_TO_ANSI(*layoutJSON), layoutJSON.Len()) == glm::crowdio::GSC_SUCCESS)
            {
                GLM_CROWD_TRACE_INFO("Successfully set new layout for " << TCHAR_TO_ANSI(*GetName()));
            }
            else
            {
                GLM_CROWD_TRACE_INFO("Could not set new layout for " << TCHAR_TO_ANSI(*GetName()));
            }

            glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
            if (bIsSimuBuilt)
            {
                bool refreshCharacters = doRefreshAssets;
                if (!refreshCharacters)
                {
                    // keep Current frame as is, but force reload of the frame
                    uint32_t cacheEntityCount = 0;
                    unsigned int unrealCharacterCount = 0;
                    for (int crowdFieldIndex = 0; crowdFieldIndex < CrowdFields.Num(); ++crowdFieldIndex)
                    {
                        if (DataPerCF.IsValidIndex(crowdFieldIndex))
                        {
                            FCrowdFieldData& thisCrowdFieldData = DataPerCF[crowdFieldIndex];

                            // update the shader data per entity
                            const glm::crowdio::GlmSimulationData* simuData = thisCrowdFieldData._cachedSimulation->getFinalSimulationData();
                            refreshCharacters = simuData->_entityCount != (uint32_t)thisCrowdFieldData.AllChars.Num();
                            if (refreshCharacters)
                            {
                                break;
                            }
                        }
                    }
                }

                if (static_cast<int32>(_factory->getGolaemCharacters().size()) != InCharacters.Num())
                {
                    // characters have changed, we must rebuild the character "models" and instances, which is done in InitSimulation
                    /*souci l'init des instances se fait avant le InitSimulation, mais on a besoin des infos du layout pour avoir le bon nombre de characters.

                    on a besoin de resize :
                    InCharacters (from factory)
                    TemplateCharactersData (from InCharacters)
                    AllInstancedCharacterActors(from InCharacters)*/

                    FillInCharactersFromFactory(true);

                    refreshCharacters = true; // regenerate characters and instances
                }

                if (refreshCharacters)
                {
                    RefreshCharacters(); // in case the number of characters has changed
                }
                LoadFrameDataAllCrowdFields();
                UpdateSimuFrameData();
                InvalidateCharactersFrame();
            }

            //_cacheProxy->dirtyEntities(); // just a flag
            //_cacheProxy->refreshCacheStatus(); // update memory budget, needs a LayoutEditorUtils::setCacheStatus
            //_cacheProxy->invalidateFrame(); // force a -INF frame to reload current one
        }
    }
}

//-------------------------------------------------------------------------
// check that layout editor exists and have a proper configuration (icons dir)
void AGolaemCache::CheckLayoutEditor()
{
    TSharedPtr<IPlugin> golaemPlugin = IPluginManager::Get().FindPlugin("GolaemPlugin");
    FString golaemUEDir = FPaths::ConvertRelativePathToFull(golaemPlugin->GetBaseDir());
    GEngine->Exec(NULL, TEXT("py import glm.layout.layoutEditorUnrealWrapper as unrealWrapper"));
    FString pyCommand = FString("py unrealWrapper.checkLayoutEditor(\"" + golaemUEDir + "\")");
    GEngine->Exec(NULL, *pyCommand);
}

//-------------------------------------------------------------------------
FString AGolaemCache::getEditedLayoutName(int layoutIndex) const
{
    if (layoutIndex == -1)
        layoutIndex = getEditedLayoutIndex();

    FString layoutIdName = GetName();
    layoutIdName += " ";
    layoutIdName += FString::FromInt(layoutIndex);

    return layoutIdName;
}

//-------------------------------------------------------------------------
int AGolaemCache::getEditedLayoutIndex() const
{
    return LayoutFilesParsed.empty() ? 0 : (LayoutFilesParsed.size() - 1);
}

//-------------------------------------------------------------------------
void AGolaemCache::OpenNewLayoutFile(int layoutIndex)
{
    CheckLayoutEditor();

    std::stringstream openLayoutFileCmd;

    FString layoutIdName = getEditedLayoutName(layoutIndex);

    GEngine->Exec(NULL, TEXT("py import glm.layout.layoutEditorWrapper as layoutWrapper\n"));
    GEngine->Exec(NULL, TEXT("py wrapper = layoutWrapper.getTheLayoutEditorWrapperInstance(True)\n"));

    openLayoutFileCmd << "py wrapper.openCacheProxy(\"";
    openLayoutFileCmd << TCHAR_TO_ANSI(*layoutIdName);
    openLayoutFileCmd << "\")\n";
    FString openLayoutFStringCmd = openLayoutFileCmd.str().c_str();
    GEngine->Exec(NULL, *openLayoutFStringCmd);
}

//-------------------------------------------------------------------------
void AGolaemCache::OpenLayoutFile(int layoutIndex, bool forceFocus)
{
    CheckLayoutEditor();

    std::stringstream openLayoutFileCmd;
    FString layoutIdName = getEditedLayoutName(layoutIndex);

    GEngine->Exec(NULL, TEXT("py import glm.layout.layoutEditorUnrealWrapper as unrealWrapper\n"));

    openLayoutFileCmd << "py unrealWrapper.openLayoutFileFromPath(\"";
    if (layoutIndex == -1)
        layoutIndex = LayoutFilesParsed.sizeInt() - 1;

    if (layoutIndex >= 0 && layoutIndex < LayoutFilesParsed.sizeInt())
    {
        openLayoutFileCmd << LayoutFilesParsed[layoutIndex].c_str();
    }
    openLayoutFileCmd << "\", \"";
    openLayoutFileCmd << TCHAR_TO_ANSI(*layoutIdName);
    openLayoutFileCmd << "\", ";
    openLayoutFileCmd << forceFocus ? "True" : "False";
    openLayoutFileCmd << ")\n";
    FString openLayoutFStringCmd = openLayoutFileCmd.str().c_str();
    GEngine->Exec(NULL, *openLayoutFStringCmd);
}

//-------------------------------------------------------------------------
void AGolaemCache::ReloadLayoutFile(int layoutIndex, bool forceFocus)
{
    if (layoutIndex == -1)
        layoutIndex = LayoutFilesParsed.sizeInt() - 1;

    if (layoutIndex < 0 || layoutIndex >= LayoutFilesParsed.sizeInt())
    {
        return;
    }

    // init the layout Editor if not done yet
    CheckLayoutEditor();

    FString layoutIdName = getEditedLayoutName(layoutIndex);

    // close the tab before reopening it (reload cache does not worl for some reason)
    GEngine->Exec(NULL, TEXT("py import glm.layout.layoutEditorWrapper as layoutWrapper\n"));
    GEngine->Exec(NULL, TEXT("py wrapper = layoutWrapper.getTheLayoutEditorWrapperInstance(True)\n"));

    std::stringstream closeLayoutFileCmd;
    closeLayoutFileCmd << "py wrapper.closeCacheProxyTab(\"";
    closeLayoutFileCmd << TCHAR_TO_ANSI(*layoutIdName);
    closeLayoutFileCmd << "\", False)\n"; // save = False, default
    FString closeLayoutFileFStringCmd = closeLayoutFileCmd.str().c_str();
    GEngine->Exec(NULL, *closeLayoutFileFStringCmd);

    // reopens the tab to load the file from disk
    OpenLayoutFile(layoutIndex, forceFocus);
}

#endif WITH_EDITOR