/***************************************************************************
 *                                                                          *
 *  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
 *                                                                          *
 ***************************************************************************/

#include "GolaemSimulation.h" //needs to be included first

#include "Engine/SkeletalMesh.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/MorphTarget.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/PlatformFilemanager.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"
#include "AssetRegistry/AssetData.h"

#include "GolaemCharacter.h"
#include "GolaemCharacterInstanced.h"
#include "GolaemUtils.h"
#include "GolaemAnimInstance.h"
#include "GolaemSkeletalMesh.h"

#include "glmUnrealObjectsWrapper.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
// #include <glmUtils.h>
#include <glmLog.h>
#include <glmScopedLock.h>
#include <glmEngine.h>
#include <glmSimulationCacheFactory.h>
#include <glmGolaemCharacter.h>
#include <glmSimulationData.h>
#include <glmFileName.h>
#include <glmGDAAttribute.h>
#include <glmGDAConnection.h>
#include <glmVector4.h>
#include <glmGDATypes.h>
#include <glmCrowdIOUtils.h>
#include <glmSimulation.h>
#include <glmAssetManagementUtils.h>
#include <glmUtils.h>
#include <glmStringOperators.h>
#include "glmADP.h"
THIRD_PARTY_INCLUDES_END

//---------------------------------------------------------------------------------------------------------
int exportLibraryItem(const char* filePath, const char* libraryName, const char* cacheDir, const char* cacheName, const glm::Array<glm::GlmString>& simulationNames, const char* characterFiles, int entityCount, int startFrame, int endFrame)
{
    // export a library item
    glm::GlmString fullFilePath = filePath;
    fullFilePath += "/";
    fullFilePath += libraryName;
    fullFilePath += ".gscb";
    std::fstream fileOutput(fullFilePath.c_str(), std::ios_base::out);
    if (!fileOutput.good())
        return -1;

    // build json item
    fileOutput << "{";
    fileOutput << "\n	\"libFile\": \"\",";
    fileOutput << "\n	\"libFileDirty\": true,";
    fileOutput << "\n	\"libFileVersion\": 2,";
    fileOutput << "\n	\"type\": \"SimCacheLib\",";
    fileOutput << "\n	\"items\":";
    fileOutput << "\n	[";
    fileOutput << "\n		{";
    fileOutput << "\n		\"type\": \"SimCacheLibItem\",";
    fileOutput << "\n		\"itemName\": \"" << libraryName << "\",";
    fileOutput << "\n		\"cacheDir\": \"" << cacheDir << "\",";
    fileOutput << "\n		\"cacheName\": \"" << cacheName << "\",";
    fileOutput << "\n		\"crowdFields\":";
    fileOutput << "\n			[";
    for (size_t iSim = 0, simCount = simulationNames.size(); iSim < simCount; ++iSim)
    {
        if (iSim > 0)
        {
            fileOutput << ",";
        }
        fileOutput << "\n				\"" << simulationNames[iSim] << "\"";
    }

    fileOutput << "\n			],";
    fileOutput << "\n		\"nbEntities\": " << entityCount << ",";
    fileOutput << "\n		\"characterFiles\": \"" << characterFiles << "\",";
    fileOutput << "\n		\"startFrame\": " << startFrame << ",";
    fileOutput << "\n		\"endFrame\": " << endFrame << ",";
    fileOutput << "\n		\"image\": \"\",";
    fileOutput << "\n		\"enableLayout\": false,";
    fileOutput << "\n		\"layoutFile\": \"\",";
    fileOutput << "\n		\"sourceTerrain\": \"\",";
    fileOutput << "\n		\"destTerrain\": \"\",";
    fileOutput << "\n		\"tags\": [\"Engine\", \"EngineExport\"]";
    fileOutput << "\n		}";
    fileOutput << "\n	]";
    fileOutput << "\n}";

    fileOutput.close();

    return 0;
}

//-------------------------------------------------------------------------
AGolaemSimulation::AGolaemSimulation(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    // PrimaryActorTick.bTickEvenWhenPaused = false;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    // Bounds.Init();
    RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("RootComponent"));
    QuatCacheToUE4 = FQuat(-GLM_SQRT_2_DIV_2, 0.f, 0.f, GLM_SQRT_2_DIV_2);

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
}

//-------------------------------------------------------------------------
AGolaemSimulation::~AGolaemSimulation()
{
    ResetSimulation();
}

//-------------------------------------------------------------------------
// https://docs.unrealengine.com/en-US/ProgrammingAndScripting/ProgrammingWithCPP/UnrealArchitecture/Actors/ActorLifecycle/index.html
// PostLoad est appelee lors du runtime et au startup du moteur si un GolaemSimulation a ete sauvegarde dans la scene
void AGolaemSimulation::PostLoad()
{
    Super::PostLoad();
    if (bRefreshGDAOnLoad)
        UpdateGdaPublicAttributes();

    bSimuDirty = true;
}

//-------------------------------------------------------------------------
void AGolaemSimulation::PostActorCreated()
{
    Super::PostActorCreated();

    // consider that we defined the cache properties, they have been unserialized
    glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
    ResetCharacterActors();
    ResetSimulation();

    glm::crowdio::ADPTrackEvent("CREATE_GOLAEMSIMULATION");
}

//-------------------------------------------------------------------------
// https://docs.unrealengine.com/portals/0/images/Programming/UnrealArchitecture/Actors/ActorLifecycle/ActorLifeCycle1.png
// OnConstruction est appelee quand on drag&drop le GolaemSimulation dans la scene et au startup du moteur si un GolaemSimulation a ete sauvegarde dans la scene
void AGolaemSimulation::OnConstruction(const FTransform& transform)
{
    Super::OnConstruction(transform);
}

//-------------------------------------------------------------------------
void AGolaemSimulation::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();
    // SetupSimulationPreview();
}

//-------------------------------------------------------------------------
void AGolaemSimulation::LoadFrameDataAllSimulations(bool localLock)
{
    // Bounds.Init();
    for (int iSimu = 0, simuCount = DataPerSimulation.Num(); iSimu < simuCount; ++iSimu)
    {
        LoadFrameData(iSimu, localLock);
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::SetSceneUnit() // sets the unit for the Golaem node if a GLMCROWD_UNIT env variable is set
{
    _crowdUnitFactor = GolaemUtils::getCrowdUnitFactor();
}

#if WITH_EDITOR
//-------------------------------------------------------------------------
void AGolaemSimulation::SetIsTemporarilyHiddenInEditor(bool bIsHidden)
{
    // hide all children
    Super::SetIsTemporarilyHiddenInEditor(bIsHidden);

    // update show percent per simulation, will take into account if bEnableSimulation or not
    for (int iSimu = 0, simuCount = DataPerSimulation.Num(); iSimu < simuCount; ++iSimu)
    {
        ShowEntityPercent(iSimu);
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
    if (e.PropertyChain.GetHead()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(AGolaemSimulation, GDAFile))
    {
        RefreshGdaFile();
    }

    if (e.PropertyChain.GetHead()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(AGolaemSimulation, bSimulationPreview))
    {
        SetupSimulationPreview();
    }
    if (e.GetPropertyName() == FName(TEXT("InstanceMode")))
    {
        SetupSimulationPreview();
    }
    if (e.GetPropertyName() == FName(TEXT("DrawEntityPercent")))
    {
        glm::ScopedLockRW<glm::RWSpinLock, true> lockForWrite(CacheRWLock);
        for (int CrowdFieldIndex = 0; CrowdFieldIndex < DataPerSimulation.Num(); ++CrowdFieldIndex)
        {
            ShowEntityPercent(CrowdFieldIndex);
        }
    }

    Super::PostEditChangeChainProperty(e);
}

//-------------------------------------------------------------------------
void AGolaemSimulation::PostEditChangeProperty(FPropertyChangedEvent& e)
{
    Super::PostEditChangeProperty(e);
}
#endif

//-------------------------------------------------------------------------
void AGolaemSimulation::SetupSimulationPreview()
{
    if (bSimulationPreview)
    {
        ResetSimulation();
        InitSimulation();
        if (_glmEngine != NULL)
        {
            _objectsWrapper->update(0);
            _glmEngine->update(0);
            LoadFrameDataAllSimulations(true); // use local lock (on swap)
            bForceTick = true;                 // force a tick
        }
    }
    else
    {
        ResetSimulation();
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::ReloadSimuFromFile()
{
    bSimuDirty = true;
}

//-------------------------------------------------------------------------
void AGolaemSimulation::ShowEntityPercent(int simulationIndex, bool locked)
{
    if (simulationIndex < DataPerSimulation.Num())
    {
        FSimulationData& thisSimuData = DataPerSimulation[simulationIndex];
        TArray<TWeakObjectPtr<AGolaemCharacter>>& entitiesToProcess = thisSimuData.AllChars;

        bool isCacheHiddenInEditor = false;
#if WITH_EDITOR
        isCacheHiddenInEditor = IsTemporarilyHiddenInEditor(true); // true : include parent hidden
#endif
        const glm::engine::EngineSimulation* engineSimulation = _glmEngine->_engineSimulations[simulationIndex];
        const glm::crowdio::GlmSimulationData* simulData = engineSimulation->getSimData();

        if (engineSimulation->getFrameData() == NULL || !bEnableSimulation || isCacheHiddenInEditor)
        {
            simulData = NULL;
        }

        AGolaemCache::ShowEntityPercentInternal(DrawEntityPercent, simulData, thisSimuData, entitiesToProcess, AllInstancedCharacterActors);
    }
}

//-------------------------------------------------------------------------
// Called when the game starts or when spawned
void AGolaemSimulation::BeginPlay()
{
    Super::BeginPlay();

    // ResetSimulation();
}

//-------------------------------------------------------------------------
void AGolaemSimulation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (_glmEngine != NULL)
    {
        if (bExportSimulationCache && !SimulationCacheDir.Path.IsEmpty())
        {
            glm::GlmString charactersPath = replaceChar(_glmEngine->getCharacterFiles(), '\\', '/'); // character path needs to be absolute in a gscb file
            int allEntitiesCount = 0;
            glm::Array<glm::GlmString> simulationNames;
            for (size_t iSim = 0; iSim < _glmEngine->_engineSimulations.size(); ++iSim)
            {
                const glm::crowdio::GlmSimulationData* simulationData = _glmEngine->getSimData(iSim);

                if (simulationData != NULL)
                {
                    allEntitiesCount += simulationData->_entityCount;
                    simulationNames.push_back(_glmEngine->_engineSimulations[iSim]->_simulationNode->getName());
                }
            }

            // create the cache library file with the correct end frame
            exportLibraryItem(TCHAR_TO_ANSI(*SimulationCacheDir.Path), "EngineExport", TCHAR_TO_ANSI(*SimulationCacheDir.Path), _glmEngine->getDigitalAssetNode().getName(), simulationNames, charactersPath.c_str(), allEntitiesCount, 0, CurrentFrame - 1);
        }
    }
    Super::EndPlay(EndPlayReason);
}

//-------------------------------------------------------------------------
// Called every frame
void AGolaemSimulation::Tick(float DeltaTime)
{
    if (bForceTick || bSimuDirty)
    {
        DeltaTime = 0.f;
        bForceTick = false;
    }

    // in PIE, for sequencer this is not active because bManualTick is false
    Super::Tick(DeltaTime);

    if (bSimuDirty || _glmEngine == NULL)
    {
        SetupSimulationPreview();
        bSimuDirty = false;

        if (_glmEngine == NULL)
        {
            return;
        }
    }
    else
    {
        float newTime = CurrentTime + DeltaTime;

        if (newTime != CurrentTime)
        {
            if (bExportSimulationCache && !SimulationCacheDir.Path.IsEmpty())
            {
                glm::GlmString frameDataExportPath;
                // refresh the simData for each update (might be emitted entities), but will output to disk only once after simulation is done
                for (size_t iSim = 0; iSim < _glmEngine->_engineSimulations.size(); ++iSim)
                {
                    // refresh the simData for each update (there might be emitted entities in the simulation), but will output to disk only once after simulation is done
                    const glm::engine::EngineSimulation* engineSimulation = _glmEngine->_engineSimulations[iSim];

                    // get the frameData, output to disk at each frame
                    glm::crowdio::getExportedFilePath(frameDataExportPath, TCHAR_TO_ANSI(*SimulationCacheDir.Path), _glmEngine->getDigitalAssetNode().getName(), _glmEngine->_engineSimulations[iSim]->_simulationNode->getName(), CurrentFrame, glm::crowdio::getGolaemFrameExtension());
                    glm::crowdio::GlmSimulationCacheStatus writeFrameStatus(glm::crowdio::GSC_FILE_OPEN_FAILED);

                    const glm::crowdio::GlmFrameData* frameData = engineSimulation->getFrameData();
                    if (frameData != NULL)
                        writeFrameStatus = glm::crowdio::writeFrameData(frameDataExportPath.c_str(), frameData, engineSimulation->getSimData());
                    if (writeFrameStatus != glm::crowdio::GSC_SUCCESS)
                    {
                        GLM_SDK_TRACE_ERROR_LIMIT("Failed to create the Golaem Frame cache gscf file " << frameDataExportPath.c_str() << ": " << glmConvertSimulationCacheStatus(writeFrameStatus));
                    }
                }
            }

            _objectsWrapper->update(DeltaTime);
            _glmEngine->update(DeltaTime);
            CurrentTime = newTime;
            LoadFrameDataAllSimulations(true); // use local lock (on swap)
            ++CurrentFrame;
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::InitSimulation()
{
    if (!bEnableSimulation)
    {
        return;
    }

    SetSceneUnit();

    FAssetRegistryModule& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> CharacterAssetData;
    IAssetRegistry& assetRegistry = assetRegistryModule.Get();

    assetRegistry.GetAssetsByClass(UGolaemSkeletalMesh::StaticClass()->GetClassPathName(), CharacterAssetData);
    assetRegistry.GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), CharacterAssetData);

    _rng.setSeed(54321);

    _glmEngine = new glm::engine::Engine();

    _objectsWrapper = new glm::UnrealObjectsWrapper(_glmEngine, this);
    _glmEngine->_objectsWrapper = _objectsWrapper;

    bool bUseFixedFrameRate = false;
    GConfig->GetBool(
        TEXT("/Script/Engine.Engine"),
        TEXT("bUseFixedFrameRate"),
        bUseFixedFrameRate,
        GEngineIni);

    if (bExportSimulationCache && !bUseFixedFrameRate)
    {
        GLM_CROWD_TRACE_ERROR("Could not export simulation from GDA if the simulation has no fixed frame rate, open project settings and see Engine - general Settings - FrameRate for more information !");
    }

    float FixedFrameRate;
    GConfig->GetFloat(
        TEXT("/Script/Engine.Engine"),
        TEXT("FixedFrameRate"),
        FixedFrameRate,
        GEngineIni);

    bool loadGdaOK = LoadGDA();
    if (!loadGdaOK || (bExportSimulationCache && !bUseFixedFrameRate))
    {
        ResetSimulation();
        return;
    }
    OverrideGDAAttributes();

    // force the framerate of Unreal instead of GDA
    // Engine - general Settings - FrameRate - use Fixed Frame Rate (bool) et Fixed Frame Rate(float)

    // inform the GDA of the expected framerate (for simulation export and geometry behavior update time per frame)
    _glmEngine->touchDigitalAssetNode().setUpdateFramerate(FixedFrameRate);

    _glmEngine->init();
    CurrentFrame = 0;

    DataPerSimulation.SetNum(_glmEngine->_engineSimulations.sizeInt());

    glm::GlmString dirmapStr = glm::getEnvironmentVariable("GLM_DIRMAP");
    glm::Array<glm::GlmString> dirmaps;
    glm::GlmString delim(";");
    dirmaps = glm::stringToStringArray(dirmapStr, delim);

    // push env var dirmaps to engine too to match correctly
    _glmEngine->_dirmaps.insert(_glmEngine->_dirmaps.end(), dirmaps.begin(), dirmaps.end());

    glm::GlmMap<glm::GlmString, glm::GlmString> usedCharacters;
    glm::Array<glm::GlmString> engineCharacterFiles;
    glm::GlmMap<glm::GlmString, int> charFileToIndex;
    {
        glm::split(_glmEngine->getCharacterFiles(), ";", engineCharacterFiles);
        for (size_t iFile = 0, fileCount = engineCharacterFiles.size(); iFile < fileCount; ++iFile)
        {
            glm::GlmString dirmappedFileName;
            glm::findDirmappedFile(dirmappedFileName, engineCharacterFiles[iFile], dirmaps);
            engineCharacterFiles[iFile] = dirmappedFileName; // update with dirmapped file path for later iteration

            charFileToIndex[dirmappedFileName] = iFile;

            if (usedCharacters.find(dirmappedFileName) == usedCharacters.end())
            {
                glm::GlmString characterName = glm::FileName(dirmappedFileName).filenameNoExt();
                usedCharacters[dirmappedFileName] = characterName;
            }
        }
    }

    TemplateCharactersData.SetNum(engineCharacterFiles.sizeInt(), NULL);

    glm::GlmMap<glm::GlmString, USkeletalMesh*> foundCharacterAssets;

    for (glm::GlmMap<glm::GlmString, glm::GlmString>::const_iterator itChar = usedCharacters.begin(); itChar != usedCharacters.end(); ++itChar)
    {
        const glm::GlmString& charFilePath = itChar.getKey();
        const glm::GlmString& charName = itChar.getValue();
        bool assetFound = false;
        for (int iCharAsset = 0, assetCount = CharacterAssetData.Num(); iCharAsset < assetCount; ++iCharAsset)
        {
            FAssetData& charAsset = CharacterAssetData[iCharAsset];
            if (charAsset.AssetName == charName.c_str())
            {
                USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(charAsset.GetAsset());
                if (SkelMesh != NULL)
                {
                    assetFound = true;
                    foundCharacterAssets[charFilePath] = SkelMesh;
                }
                break;
            }
        }
        if (!assetFound)
        {
            GLM_CROWD_TRACE_ERROR("Could not find Unreal asset for Golaem Character file '" << charFilePath << "'. Please import the character first!");
            ResetSimulation();
            return;
        }
    }

    UWorld* world = GetWorld();

    AllInstancedCharacterActors.Empty();
    if (InstanceMode == EInstanceMode::FORCED || InstanceMode == EInstanceMode::FOR_RIGID_MESHES)
    {
        for (size_t iFile = 0, fileCount = engineCharacterFiles.size(); iFile < fileCount; ++iFile)
        {
            auto charAssetIte = foundCharacterAssets.find(engineCharacterFiles[iFile]);
            if (charAssetIte != foundCharacterAssets.end())
            {
                FTransform spawnTransform = FTransform();
                AGolaemCharacterInstanced* thisCharacterInstanced = world->SpawnActorDeferred<AGolaemCharacterInstanced>(AGolaemCharacterInstanced::StaticClass(), spawnTransform);
                thisCharacterInstanced->InSkeletalMesh = charAssetIte.getValue();
#if WITH_EDITOR
                thisCharacterInstanced->UpdateSkeletalMesh(InstanceMode);
#endif
                thisCharacterInstanced->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

                // Golaem characters are loadedin engine, after loadGDA / createEntityTypes & configureBehaviorGraphs

                // dummy values won't be used, character is already loaded
                glm::GenericSpecificConverter::SpecificPostureBlindData::Value specificAnimationBlindDataModDummy = glm::GenericSpecificConverter::SpecificPostureBlindData::NONE;
                float rollExtractionBaseOffsetDummy = 0.f;
                const glm::GolaemCharacter* glmCharacter = _glmEngine->getCharacter(charAssetIte.getKey().c_str(), specificAnimationBlindDataModDummy, rollExtractionBaseOffsetDummy);
                thisCharacterInstanced->SetCharacterToCacheBones(glmCharacter);
#if WITH_EDITOR
                thisCharacterInstanced->SetActorLabel(thisCharacterInstanced->InSkeletalMesh->GetName() + FString("_Instances"));
#endif
                AllInstancedCharacterActors.Add(thisCharacterInstanced);

                thisCharacterInstanced->AddTickPrerequisiteActor(this);
                thisCharacterInstanced->FinishSpawning(spawnTransform);
            }
            else
            {
                // keep characters alignment
                GLM_CROWD_TRACE_ERROR("Could not find Unreal asset for Golaem Character file '" << engineCharacterFiles[iFile] << "'. Please import the character first!");
                ResetSimulation();
                return;
            }
        }
    }

    for (size_t iSimu = 0, simuCount = _glmEngine->_engineSimulations.sizeInt(); iSimu < simuCount; ++iSimu)
    {
        FSimulationData& thisSimuData = DataPerSimulation[iSimu];

        const glm::engine::EngineSimulation* engineSimulation = _glmEngine->_engineSimulations[iSimu];

        const glm::Array<glm::PODArray<int>>& allEntityAssets = engineSimulation->getSrcEntityAssets();

        thisSimuData.MeshIndicesInUnrealPerCharacter.resize(TemplateCharactersData.Num());
        thisSimuData.MorphTargetsToBlindDataIndexPerChar.resize(TemplateCharactersData.Num());

        const glm::crowdio::GlmSimulationData* simulationData = engineSimulation->getSimData();

        if (simulationData != NULL)
        {
            for (int iSimuEt = 0; iSimuEt < simulationData->_entityTypeCount; ++iSimuEt)
            {
                glm::EntityType* entityType = engineSimulation->_simulation->getEntityType(iSimuEt);
                int characterIndex = charFileToIndex[entityType->_characterFile]; // characterFile has been dirmapped in EngineSimulation::createEntityType
                thisSimuData.MorphTargetsToBlindDataIndexPerChar[characterIndex].clear();
                thisSimuData.MeshIndicesInUnrealPerCharacter[characterIndex].clear(); // insure that we don't stack several times the information

                // FString resolvedFilePath = entityType->_characterFile.c_str();
                // GolaemUtils::dirmapString(resolvedFilePath);
                // glm::GlmString resolvedPathGlm = TCHAR_TO_ANSI(*resolvedFilePath);

                // dummy values won't be used, character is already loaded
                glm::GenericSpecificConverter::SpecificPostureBlindData::Value specificAnimationBlindDataModDummy = glm::GenericSpecificConverter::SpecificPostureBlindData::NONE;
                float rollExtractionBaseOffsetDummy = 0.f;
                const glm::GolaemCharacter* glmCharacter = _glmEngine->getCharacter(entityType->_characterFile.c_str(), specificAnimationBlindDataModDummy, rollExtractionBaseOffsetDummy);

                // should not fail, we passed character init simu
                {
                    glm::GlmMap<glm::GlmString, USkeletalMesh*>::const_iterator itCharacterAsset = foundCharacterAssets.find(entityType->_characterFile);

                    if (itCharacterAsset != foundCharacterAssets.end())
                    {
                        USkeletalMesh* characterSkelMesh = itCharacterAsset.getValue();
                        thisSimuData.EtSkeletalMeshes[iSimuEt] = characterSkelMesh;

                        AGolaemCache::GetMeshIndicesInUnrealPerCharacter(characterSkelMesh, glmCharacter, thisSimuData, characterIndex);
                    }
                }
            }

            FTransform spawnTransform = FTransform();
            FString entityName;

            thisSimuData.AllChars.SetNum(simulationData->_entityCount);
            // else it will be detecetd empty and will look for them in LoadFrameData of BeginPlay

            for (uint32_t simDataEntityIdx = 0, entityCount = simulationData->_entityCount; simDataEntityIdx < entityCount; ++simDataEntityIdx)
            {
                if (!glm::crowdio::isEntityValid(simulationData, simDataEntityIdx))
                {
                    continue;
                }

                // we must build all characters, and disable those disabled in current frame to be able to show them when they spawn without having to reinit !

                // createCharacter

                // SpawnActorDeferred permet d'executer certaines methodes de GolaemCharacter avant de faire effectivement spawn le personnage

                // entityName = FString::Printf(TEXT("Entity_%d"), entityId);
                // glmCharacterActor->Rename(*entityName); // failing crashing on "glmCharacterActor needs loaded" when changing outer.. which does not change hum

                // get meshes to keep while keeping right indices in case of multi materials
                // if we have only one section, we probably need to merge stuff
                const glm::PODArray<int>& entityAssets = allEntityAssets[simDataEntityIdx];
                int characterIndex = simulationData->_characterIdx[simDataEntityIdx];

                // collapseCacheInOutliner = InstanceMode == EInstanceMode::FOR_RIGID_MESHES || InstanceMode == EInstanceMode::DISABLED;
                const glm::crowdio::GlmFrameData* frameData = engineSimulation->getFrameData();

                AGolaemCache::SpawnActor(
                    this, // as golaem simu interface for compute skeleton
                    this, // as parent actor
                    InstanceMode,
                    characterIndex,
                    entityAssets,
                    simulationData,
                    frameData,
                    simDataEntityIdx,
                    iSimu,
                    thisSimuData.EtSkeletalMeshes[simulationData->_entityTypes[simDataEntityIdx]],
                    _crowdUnitFactor,
                    thisSimuData,
                    TemplateCharactersData,
                    world,
                    AllInstancedCharacterActors);
            }
        }
        ApplySimuShaderData(iSimu);
    }
    CurrentTime = world->TimeSeconds;

    // dirmap SimulationCacheDir
    GolaemUtils::dirmapString(SimulationCacheDir.Path);

    // exporting library item
    if (bExportSimulationCache && !SimulationCacheDir.Path.IsEmpty())
    {
        // refresh the simData, output to disk
        glm::GlmString simDataExportPath;
        glm::GlmString charactersPath = replaceChar(_glmEngine->getCharacterFiles(), '\\', '/'); // character path needs to be absolute in a gscb file
        int allEntitiesCount = 0;
        glm::Array<glm::GlmString> simulationNames;
        for (size_t iSim = 0; iSim < _glmEngine->_engineSimulations.size(); ++iSim)
        {
            const glm::crowdio::GlmSimulationData* simulationData = _glmEngine->getSimData(iSim);

            glm::crowdio::getExportedFilePath(simDataExportPath, TCHAR_TO_ANSI(*SimulationCacheDir.Path), _glmEngine->getDigitalAssetNode().getName(), _glmEngine->_engineSimulations[iSim]->_simulationNode->getName(), glm::crowdio::getGolaemSimulationExtension());

            if (simulationData != NULL)
            {
                allEntitiesCount += simulationData->_entityCount;

                glm::crowdio::GlmSimulationCacheStatus writeStatus = glm::crowdio::writeSimulationData(simDataExportPath.c_str(), simulationData, simulationData->_licenseHashKey);
                if (writeStatus != glm::crowdio::GSC_SUCCESS)
                {
                    GLM_SDK_TRACE_ERROR_LIMIT("Failed to create the Golaem Simulation cache gscs file " << simDataExportPath.c_str() << ": " << glmConvertSimulationCacheStatus(writeStatus));
                }
                simulationNames.push_back(_glmEngine->_engineSimulations[iSim]->_simulationNode->getName());
            }
        }

        // create the cache library file
        exportLibraryItem(TCHAR_TO_ANSI(*SimulationCacheDir.Path), "EngineExport", TCHAR_TO_ANSI(*SimulationCacheDir.Path), _glmEngine->getDigitalAssetNode().getName(), simulationNames, charactersPath.c_str(), allEntitiesCount, CurrentFrame, CurrentFrame + 1000 - 1);
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::LoadFrameData(int simulationIndex, bool localLock)
{
    if (DataPerSimulation.IsValidIndex(simulationIndex))
    {
        FSimulationData& thisSimuData = DataPerSimulation[simulationIndex];

        const glm::engine::EngineSimulation* engineSimulation = _glmEngine->_engineSimulations[simulationIndex];

        const glm::crowdio::GlmSimulationData* simulationData = engineSimulation->getSimData();
        const glm::crowdio::GlmFrameData* frameData = engineSimulation->getFrameData();

        if (simulationData != NULL && frameData != NULL)
        {
            for (uint32_t simDataEntityIdx = 0, entityCount = simulationData->_entityCount; simDataEntityIdx < entityCount; ++simDataEntityIdx)
            {
                int64_t entityId = simulationData->_entityIds[simDataEntityIdx];
                if (thisSimuData.AllChars[simDataEntityIdx].IsValid())
                {
                    AGolaemCharacter* currentChar = thisSimuData.AllChars[simDataEntityIdx].Get();
                    currentChar->bEnabledInCache = entityId >= 0 && frameData->_entityEnabled[simDataEntityIdx] == 1;
                }
            }
        }

        ShowEntityPercent(simulationIndex, !localLock);

        // apply shader data depending on pp attributes
        ApplyFrameShaderData(simulationIndex);
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::UpdateTransforms_AnyThread(AGolaemCharacterInstanced* characterInstanced)
{
    glm::ScopedLockRW<glm::RWSpinLock, false> lockForRead(CacheRWLock);

    for (int simulationIndex = 0; simulationIndex < DataPerSimulation.Num(); simulationIndex++)
    {
        FSimulationData& thisSimuData = DataPerSimulation[simulationIndex];

        const glm::engine::EngineSimulation* engineSimulation = _glmEngine->_engineSimulations[simulationIndex];

        const glm::crowdio::GlmSimulationData* simulationData = engineSimulation->getSimData();
        const glm::crowdio::GlmFrameData* frameData = engineSimulation->getFrameData();

        if (simulationData != NULL && frameData != NULL)
        {
            // GLM_DCC_TRACE_WARNING("CharacterInstanced->UpdateTransforms updated to current frame  : " << CurrentFrame);
            characterInstanced->UpdateTransforms(*simulationData, *frameData, simulationIndex, /* _factory,*/ CurrentFrame, _crowdUnitFactor);
        }
    }
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::GetBoneSpaceTransforms_AnyThread(int simulationIndex, int entityCacheIndex, TArray<FTransform>& boneSpaceTransforms, TArray<float>& morphTargetWeights, float& entityCurrentTime)
{
    glm::ScopedLockRW<glm::RWSpinLock, false> lockForRead(CacheRWLock);

    if (simulationIndex < DataPerSimulation.Num())
    {
        FSimulationData& thisSimuData = DataPerSimulation[simulationIndex];

        const glm::engine::EngineSimulation* engineSimulation = _glmEngine->_engineSimulations[simulationIndex];

        const glm::crowdio::GlmSimulationData* simulationData = engineSimulation->getSimData();
        const glm::crowdio::GlmFrameData* frameData = engineSimulation->getFrameData();

        if (simulationData != NULL &&
            frameData != NULL &&
            thisSimuData.AllChars.IsValidIndex(entityCacheIndex) &&
            thisSimuData.AllChars[entityCacheIndex].IsValid())
        {
            AGolaemCharacter* currentChar = thisSimuData.AllChars[entityCacheIndex].Get();

            USceneComponent* cacheProxyRootComponent = GetRootComponent();
            const FTransform& cacheProxyTransform = cacheProxyRootComponent->GetComponentTransform();

            USkeletalMeshComponent* SkelMeshComp = currentChar->GetSkeletalMeshComponent();
            USkeletalMesh* skelMesh = SkelMeshComp->SkeletalMesh;

            TArray<FTransform> ComponentSpaceTransforms;

            if (!glm::crowdio::isEntityValid(simulationData, entityCacheIndex))
            {
                return false;
            }

            if (frameData->_entityEnabled[entityCacheIndex] == 1)
            {
                unsigned int entityType = simulationData->_entityTypes[entityCacheIndex];

                boneSpaceTransforms.SetNum(simulationData->_boneCount[entityType]);
                ComponentSpaceTransforms.SetNum(boneSpaceTransforms.Num()); // to be able to compute relative

                FVector BonePosition;
                FQuat BoneOrientation;

                const TArray<int32>& entityBoneHierarchy(TemplateCharactersData[currentChar->CharacterIndex]._boneHierarchy);
                if (entityBoneHierarchy.Num() == ComponentSpaceTransforms.Num()) // else we have an internal error : mismatch instanced character with simData
                {
                    uint16_t boneCount = simulationData->_boneCount[entityType];
                    uint32_t bonePositionOffset = simulationData->_iBoneOffsetPerEntityType[entityType] + simulationData->_indexInEntityType[entityCacheIndex] * boneCount;
                    for (unsigned int iBone = 0; iBone < boneCount; ++iBone)
                    {
                        unsigned int index = iBone + bonePositionOffset;

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
                        float characterScale = simulationData->_scales[entityCacheIndex];
                        currentComponentSpaceTransform.SetScale3D(FVector(_crowdUnitFactor * characterScale, _crowdUnitFactor * characterScale, _crowdUnitFactor * characterScale));

                        int parentIndex = skelMesh->RefSkeleton.GetParentIndex(entityBoneHierarchy[iBone]);

                        currentComponentSpaceTransform.SetToRelativeTransform(SkelMeshComp->GetComponentTransform());

                        currentBonesSpaceTransform = currentComponentSpaceTransform;

                        if (parentIndex >= 0 && parentIndex < ComponentSpaceTransforms.Num())
                        {
                            currentBonesSpaceTransform.SetToRelativeTransform(ComponentSpaceTransforms[parentIndex]);
                        }
                    }
                }

                if (currentChar->bMorphTargetFromAnim && currentChar->CharacterIndex >= 0 && currentChar->CharacterIndex < thisSimuData.MorphTargetsToBlindDataIndexPerChar.size())
                {
                    glm::PODArray<int32>& AllMorphTargetsToBlindDataIndexPerChar = thisSimuData.MorphTargetsToBlindDataIndexPerChar[currentChar->CharacterIndex];
                    unsigned int thisEntityBlindDataCount = simulationData->_blindDataCount[entityType];
                    unsigned int thisEntityFirstBlindDataIndex = simulationData->_iBlindDataOffsetPerEntityType[entityType] + thisEntityBlindDataCount * simulationData->_indexInEntityType[entityCacheIndex];

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
                entityCurrentTime = CurrentTime;
                return true;
            }
        }
    }

    return false;
}

//-------------------------------------------------------------------------
void AGolaemSimulation::ResetCharacterActors()
{
    UWorld* world = GetWorld();
    if (world != NULL)
    {
        TArray<AActor*> childrenToDestroy;
        GetAttachedActors(childrenToDestroy);
        for (int iChild = childrenToDestroy.Num() - 1; iChild >= 0; --iChild)
        {
            AActor* child = childrenToDestroy[iChild];
            world->DestroyActor(child);
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::Destroyed()
{
    ResetCharacterActors();

    Super::Destroyed();
}

//-------------------------------------------------------------------------
void AGolaemSimulation::ResetSimulation()
{
    if (_glmEngine != NULL)
    {
        _glmEngine->finish();
    }

    GLM_SAFE_DELETE(_glmEngine);
    GLM_SAFE_DELETE(_objectsWrapper);

    // we don't keep any frame data or simu data
    DataPerSimulation.Empty();

    TemplateCharactersData.Empty();

    // remove all characters
    ResetCharacterActors();
}

//-------------------------------------------------------------------------
void GetGDAAttributeSingleValue(UGDAProperty* gdaProp, const glm::engine::GDAAttribute* attribute, const size_t* indexes, float crowdUnitFactor)
{
    switch (attribute->getTypeId())
    {
    case glm::engine::GDAAttributeType::INT32:
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProp);

        gdaSpecificProp->Value.Add(attribute->getInt32Value(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::INT64:
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProp);

        gdaSpecificProp->Value.Add(attribute->getInt64Value(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::UINT32:
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProp);

        gdaSpecificProp->Value.Add(attribute->getUint32Value(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::UINT64:
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProp);

        gdaSpecificProp->Value.Add(attribute->getUint64Value(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::FLOAT:
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProp);
        const float& gdaValue = attribute->getFloatValue(indexes);
        float unrealValue;
        AGolaemSimulation::gdaToUnreal(gdaValue, unrealValue, crowdUnitFactor, (GDAPropertyDataType::Type)attribute->getDataTypeId());
        gdaSpecificProp->Value.Add(unrealValue);
    }
    break;
    case glm::engine::GDAAttributeType::VEC3:
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProp);
        const float* gdaValue = attribute->getVec3Value(indexes);
        FVector unrealValue;
        AGolaemSimulation::gdaToUnreal(glm::Vector3(gdaValue), unrealValue, crowdUnitFactor, (GDAPropertyDataType::Type)attribute->getDataTypeId());
        gdaSpecificProp->Value.Add(unrealValue);
    }
    break;
    case glm::engine::GDAAttributeType::VEC4:
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProp);
        const float* gdaValue = attribute->getVec4Value(indexes);
        FVector4 unrealValue;
        AGolaemSimulation::gdaToUnreal(glm::Vector4(gdaValue), unrealValue, crowdUnitFactor, (GDAPropertyDataType::Type)attribute->getDataTypeId());
        gdaSpecificProp->Value.Add(unrealValue);
    }
    break;
    case glm::engine::GDAAttributeType::STRING:
    {
        UGDAStringProperty* gdaSpecificProp = static_cast<UGDAStringProperty*>(gdaProp);

        gdaSpecificProp->Value.Add(attribute->getStringValue(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::MESH:
    {
        UGDAMeshProperty* gdaSpecificProp = static_cast<UGDAMeshProperty*>(gdaProp);

        gdaSpecificProp->Value.Add(NULL);
    }
    break;
    case glm::engine::GDAAttributeType::INT32ARRAY:
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProp);
        const int32_t* gdaArrayValue = attribute->getInt32ArrayValue(indexes);
        for (size_t iElem = 0, elemCount = attribute->getArraySize(indexes); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value.Add(gdaArrayValue[iElem]);
        }
        gdaSpecificProp->ArraySizes.Add(attribute->getArraySize(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::INT64ARRAY:
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProp);
        const int64_t* gdaArrayValue = attribute->getInt64ArrayValue(indexes);
        for (size_t iElem = 0, elemCount = attribute->getArraySize(indexes); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value.Add(gdaArrayValue[iElem]);
        }
        gdaSpecificProp->ArraySizes.Add(attribute->getArraySize(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::UINT32ARRAY:
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProp);
        const uint32_t* gdaArrayValue = attribute->getUint32ArrayValue(indexes);
        for (size_t iElem = 0, elemCount = attribute->getArraySize(indexes); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value.Add(gdaArrayValue[iElem]);
        }
        gdaSpecificProp->ArraySizes.Add(attribute->getArraySize(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::UINT64ARRAY:
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProp);
        const uint64_t* gdaArrayValue = attribute->getUint64ArrayValue(indexes);
        for (size_t iElem = 0, elemCount = attribute->getArraySize(indexes); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value.Add(gdaArrayValue[iElem]);
        }
        gdaSpecificProp->ArraySizes.Add(attribute->getArraySize(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::FLOATARRAY:
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProp);
        const float* gdaArrayValue = attribute->getFloatArrayValue(indexes);
        float unrealValue;
        for (size_t iElem = 0, elemCount = attribute->getArraySize(indexes); iElem < elemCount; ++iElem)
        {
            const float& gdaValue = gdaArrayValue[iElem];
            AGolaemSimulation::gdaToUnreal(gdaValue, unrealValue, crowdUnitFactor, (GDAPropertyDataType::Type)attribute->getDataTypeId());
            gdaSpecificProp->Value.Add(unrealValue);
        }
        gdaSpecificProp->ArraySizes.Add(attribute->getArraySize(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::VEC3ARRAY:
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProp);
        const float* gdaArrayValue = attribute->getVec3ArrayValue(indexes);
        FVector unrealValue;
        for (size_t iElem = 0, elemCount = attribute->getArraySize(indexes); iElem < elemCount; ++iElem)
        {
            const float* gdaValue = &gdaArrayValue[3 * iElem];
            AGolaemSimulation::gdaToUnreal(glm::Vector3(gdaValue), unrealValue, crowdUnitFactor, (GDAPropertyDataType::Type)attribute->getDataTypeId());
            gdaSpecificProp->Value.Add(unrealValue);
        }
        gdaSpecificProp->ArraySizes.Add(attribute->getArraySize(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::VEC4ARRAY:
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProp);
        const float* gdaArrayValue = attribute->getVec4ArrayValue(indexes);
        FVector4 unrealValue;
        for (size_t iElem = 0, elemCount = attribute->getArraySize(indexes); iElem < elemCount; ++iElem)
        {
            const float* gdaValue = &gdaArrayValue[4 * iElem];
            AGolaemSimulation::gdaToUnreal(glm::Vector4(gdaValue), unrealValue, crowdUnitFactor, (GDAPropertyDataType::Type)attribute->getDataTypeId());
            gdaSpecificProp->Value.Add(unrealValue);
        }
        gdaSpecificProp->ArraySizes.Add(attribute->getArraySize(indexes));
    }
    break;
    case glm::engine::GDAAttributeType::MESHARRAY:
    {
        UGDAMeshProperty* gdaSpecificProp = static_cast<UGDAMeshProperty*>(gdaProp);
        for (size_t iElem = 0, elemCount = attribute->getArraySize(indexes); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value.Add(NULL);
        }
        gdaSpecificProp->ArraySizes.Add(attribute->getArraySize(indexes));
    }
    break;

    default:
        break;
    }
}

//-------------------------------------------------------------------------
void GetGDAAttributeValueRecursive(UGDAProperty* gdaProp, const glm::engine::GDAAttribute* attribute, size_t currentDimension, const size_t* indexes, float crowdUnitFactor)
{
    if (currentDimension > 0)
    {
        --currentDimension;
        size_t currentSize = attribute->getSizes()[currentDimension];
        glm::PODArray<size_t> newIndexes(attribute->getDimension(), indexes);
        for (size_t iElem = 0; iElem < currentSize; ++iElem)
        {
            newIndexes[currentDimension] = iElem;
            GetGDAAttributeValueRecursive(gdaProp, attribute, currentDimension, newIndexes.begin(), crowdUnitFactor);
        }
    }
    else // currentDimension == 0
    {
        // single value
        GetGDAAttributeSingleValue(gdaProp, attribute, indexes, crowdUnitFactor);
    }
}

//-------------------------------------------------------------------------
UGDAProperty* AGolaemSimulation::CreateGdaPropertyFromAttribute(const glm::engine::GDAAttribute* attribute, const glm::engine::GDAContainer& container)
{
    UGDAProperty* gdaProp = NULL;
    try
    {
        glm::GlmString propName = attribute->getShortestUniqueName();
        glm::UnrealObjectsWrapper::sanitizeName(propName);
        UObject* parent = GetOuter();
        switch (attribute->getTypeId())
        {
        case glm::engine::GDAAttributeType::INT32:
        case glm::engine::GDAAttributeType::INT32ARRAY:
        case glm::engine::GDAAttributeType::UINT32:
        case glm::engine::GDAAttributeType::UINT32ARRAY:
        {
            UGDAInt32Property* gdaSpecificProp = NewObject<UGDAInt32Property>(parent, UGDAInt32Property::StaticClass(), propName.c_str(), RF_Public);
            gdaProp = gdaSpecificProp;
        }
        break;
        case glm::engine::GDAAttributeType::INT64:
        case glm::engine::GDAAttributeType::INT64ARRAY:
        case glm::engine::GDAAttributeType::UINT64:
        case glm::engine::GDAAttributeType::UINT64ARRAY:
        {
            UGDAInt64Property* gdaSpecificProp = NewObject<UGDAInt64Property>(parent, UGDAInt64Property::StaticClass(), propName.c_str(), RF_Public);
            gdaProp = gdaSpecificProp;
        }
        break;
        case glm::engine::GDAAttributeType::FLOAT:
        case glm::engine::GDAAttributeType::FLOATARRAY:
        {
            UGDAFloatProperty* gdaSpecificProp = NewObject<UGDAFloatProperty>(parent, UGDAFloatProperty::StaticClass(), propName.c_str(), RF_Public);
            gdaProp = gdaSpecificProp;
        }
        break;
        case glm::engine::GDAAttributeType::VEC3:
        case glm::engine::GDAAttributeType::VEC3ARRAY:
        {
            UGDAVector3Property* gdaSpecificProp = NewObject<UGDAVector3Property>(parent, UGDAVector3Property::StaticClass(), propName.c_str(), RF_Public);
            gdaProp = gdaSpecificProp;
        }
        break;
        case glm::engine::GDAAttributeType::VEC4:
        case glm::engine::GDAAttributeType::VEC4ARRAY:
        {
            UGDAVector4Property* gdaSpecificProp = NewObject<UGDAVector4Property>(parent, UGDAVector4Property::StaticClass(), propName.c_str(), RF_Public);
            gdaProp = gdaSpecificProp;
        }
        break;
        case glm::engine::GDAAttributeType::STRING:
        {
            UGDAStringProperty* gdaSpecificProp = NewObject<UGDAStringProperty>(parent, UGDAStringProperty::StaticClass(), propName.c_str(), RF_Public);
            gdaProp = gdaSpecificProp;
        }
        break;
        case glm::engine::GDAAttributeType::MESH:
        case glm::engine::GDAAttributeType::MESHARRAY:
        {
            UGDAMeshProperty* gdaSpecificProp = NewObject<UGDAMeshProperty>(parent, UGDAMeshProperty::StaticClass(), propName.c_str(), RF_Public);
            gdaProp = gdaSpecificProp;
        }
        break;
        default:
            break;
        }

        if (gdaProp != NULL)
        {
            gdaProp->GDAAttributeShortName = attribute->getShortestUniqueName();
            glm::GlmString fullName;
            attribute->toString(fullName, &container);
            gdaProp->GDAAttributeFullName = fullName.c_str();
            glm::PODArray<size_t> indexes(attribute->getDimension(), size_t(0));
            GetGDAAttributeValueRecursive(gdaProp, attribute, attribute->getDimension(), indexes.begin(), _crowdUnitFactor);
            gdaProp->Dimension = attribute->getDimension();
            gdaProp->Sizes.SetNum(gdaProp->Dimension);
            const size_t* sizes = attribute->getSizes();
            for (size_t iDim = 0, dimCount = attribute->getDimension(); iDim < dimCount; ++iDim)
            {
                gdaProp->Sizes[iDim] = sizes[iDim];
            }
            gdaProp->DataTypeId = (GDAPropertyDataType::Type)attribute->getDataTypeId();
            gdaProp->GolaemSimulation = this;
        }
    }
    catch (std::exception)
    {
        GLM_SDK_TRACE_ERROR("Error while trying to create the GDA property from a GDA attribute.");
    }
    return gdaProp;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::LoadGDA()
{
    GolaemUtils::dirmapString(GDAFile.FilePath);
    bool loadGdaOK = true;
    if (!GDAFile.FilePath.IsEmpty())
    {
        if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*GDAFile.FilePath))
        {
            loadGdaOK = false;
            GLM_CROWD_TRACE_ERROR("Could not find GDA file '" << TCHAR_TO_ANSI(*GDAFile.FilePath) << "'!");
        }
    }
    else
    {
        loadGdaOK = false;
        GLM_CROWD_TRACE_INFO("No gda file loaded!");
    }
    if (loadGdaOK)
    {
        loadGdaOK = _glmEngine->loadGDA(TCHAR_TO_ANSI(*GDAFile.FilePath));
    }
    return loadGdaOK;
}

//-------------------------------------------------------------------------
void AGolaemSimulation::UpdateGdaPublicAttributes()
{
    if (GDAProperties.Num() > 0)
    {
        TArray<UGDAProperty*> GDAPropsWithOverride;
        for (int32 iProp = 0, propCount = GDAProperties.Num(); iProp < propCount; ++iProp)
        {
            UGDAProperty* gdaProp = GDAProperties[iProp];
            if (gdaProp != NULL && gdaProp->Override)
            {
                GDAPropsWithOverride.Add(gdaProp);
            }
        }
        GDAProperties = GDAPropsWithOverride;
    }
    for (int32 iProp = 0, propCount = GDAProperties.Num(); iProp < propCount; ++iProp)
    {
        UGDAProperty* gdaProp = GDAProperties[iProp];
        GLM_CROWD_TRACE_WARNING("GDA Attribute '" << TCHAR_TO_ANSI(*gdaProp->GDAAttributeShortName) << "' has an override. Keeping the override. Please remove it manually from GDAProperties to reset the attribute.");
    }
    bool loadGdaOK = true;

    GolaemUtils::dirmapString(GDAFile.FilePath);

    if (!GDAFile.FilePath.IsEmpty())
    {
        if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*GDAFile.FilePath))
        {
            loadGdaOK = false;
        }
    }
    else
    {
        loadGdaOK = false;
    }
    if (loadGdaOK)
    {
        glm::engine::GDAContainer container("UnrealContainer", "Container");
        container.loadGdaFile(TCHAR_TO_ANSI(*GDAFile.FilePath));
        container.computeAttributesShortestUniqueNames();

        TArray<UGDAProperty*> NewGDAProperties;

        glm::PODArray<glm::engine::GDAAttribute*> attributes;
        container.findAttributesRecursive(attributes);
        glm::GlmString fullName;

        for (size_t iAttr = 0, attrCount = attributes.size(); iAttr < attrCount; ++iAttr)
        {
            const glm::engine::GDAAttribute* attribute = attributes[iAttr];
            if (attribute->isPublic())
            {
                attribute->toString(fullName, &container);
                UGDAProperty* gdaProp = FindGdaPropertyByFullName(fullName.c_str());
                if (gdaProp == NULL)
                {
                    // do not add property if an override exists
                    gdaProp = CreateGdaPropertyFromAttribute(attribute, container);
                    if (gdaProp != NULL)
                    {
                        NewGDAProperties.Add(gdaProp);
                    }
                }
            }
        }
        GDAProperties.Append(NewGDAProperties);
    }
}

//-------------------------------------------------------------------------
UGDAProperty* AGolaemSimulation::FindGdaPropertyByFullName(const FString& fullName)
{
    for (int32 iProp = 0, propCount = GDAProperties.Num(); iProp < propCount; ++iProp)
    {
        UGDAProperty* gdaProp = GDAProperties[iProp];
        if (gdaProp != NULL && gdaProp->GDAAttributeFullName == fullName)
        {
            return gdaProp;
        }
    }
    return NULL;
}

//-------------------------------------------------------------------------
UGDAProperty* AGolaemSimulation::FindGdaPropertyByShortName(const FString& shortName)
{
    for (int32 iProp = 0, propCount = GDAProperties.Num(); iProp < propCount; ++iProp)
    {
        UGDAProperty* gdaProp = GDAProperties[iProp];
        if (gdaProp != NULL && gdaProp->GDAAttributeShortName == shortName)
        {
            return gdaProp;
        }
    }
    return NULL;
}

//-------------------------------------------------------------------------
void SetGDAAttributeSizes(const glm::engine::GDAAttribute* attribute, const UGDAProperty* gdaProp, size_t* sizes)
{
    bool sizesSet = false;
    if (attribute->getDimension() == 1)
    {
        switch (attribute->getTypeId())
        {
        case glm::engine::GDAAttributeType::INT32:
        {
            const UGDAInt32Property* gdaSpecificProp = static_cast<const UGDAInt32Property*>(gdaProp);
            sizes[0] = gdaSpecificProp->Value.Num();
            sizesSet = true;
        }
        break;
        case glm::engine::GDAAttributeType::INT64:
        {
            const UGDAInt64Property* gdaSpecificProp = static_cast<const UGDAInt64Property*>(gdaProp);
            sizes[0] = gdaSpecificProp->Value.Num();
            sizesSet = true;
        }
        break;
        case glm::engine::GDAAttributeType::UINT32:
        {
            const UGDAInt32Property* gdaSpecificProp = static_cast<const UGDAInt32Property*>(gdaProp);
            sizes[0] = gdaSpecificProp->Value.Num();
            sizesSet = true;
        }
        break;
        case glm::engine::GDAAttributeType::UINT64:
        {
            const UGDAInt64Property* gdaSpecificProp = static_cast<const UGDAInt64Property*>(gdaProp);
            sizes[0] = gdaSpecificProp->Value.Num();
            sizesSet = true;
        }
        break;
        case glm::engine::GDAAttributeType::FLOAT:
        {
            const UGDAFloatProperty* gdaSpecificProp = static_cast<const UGDAFloatProperty*>(gdaProp);
            sizes[0] = gdaSpecificProp->Value.Num();
            sizesSet = true;
        }
        break;
        case glm::engine::GDAAttributeType::VEC3:
        {
            const UGDAVector3Property* gdaSpecificProp = static_cast<const UGDAVector3Property*>(gdaProp);
            sizes[0] = gdaSpecificProp->Value.Num();
            sizesSet = true;
        }
        break;
        case glm::engine::GDAAttributeType::VEC4:
        {
            const UGDAVector4Property* gdaSpecificProp = static_cast<const UGDAVector4Property*>(gdaProp);
            sizes[0] = gdaSpecificProp->Value.Num();
            sizesSet = true;
        }
        break;
        case glm::engine::GDAAttributeType::STRING:
        {
            const UGDAStringProperty* gdaSpecificProp = static_cast<const UGDAStringProperty*>(gdaProp);
            sizes[0] = gdaSpecificProp->Value.Num();
            sizesSet = true;
        }
        break;
        case glm::engine::GDAAttributeType::MESH:
        {
            const UGDAMeshProperty* gdaSpecificProp = static_cast<const UGDAMeshProperty*>(gdaProp);
            sizes[0] = gdaSpecificProp->Value.Num();
            sizesSet = true;
        }
        break;
        default:
            break;
        }
    }
    if (!sizesSet)
    {
        for (size_t iDim = 0, dimCount = attribute->getDimension(); iDim < dimCount && iDim < gdaProp->Sizes.Num(); ++iDim)
        {
            sizes[iDim] = gdaProp->Sizes[iDim];
        }
    }
}

//-------------------------------------------------------------------------
void SetGDAAttributeSingleValue(glm::engine::GDAAttribute* attribute, const UGDAProperty* gdaProp, const size_t* indexes, float crowdUnitFactor)
{
    size_t valueIdx = attribute->getValueIndex(indexes);
    switch (attribute->getTypeId())
    {
    case glm::engine::GDAAttributeType::INT32:
    {
        const UGDAInt32Property* gdaSpecificProp = static_cast<const UGDAInt32Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->Value.Num())
        {
            attribute->setInt32Value(gdaSpecificProp->Value[valueIdx], indexes);
        }
    }
    break;
    case glm::engine::GDAAttributeType::INT64:
    {
        const UGDAInt64Property* gdaSpecificProp = static_cast<const UGDAInt64Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->Value.Num())
        {
            attribute->setInt64Value(gdaSpecificProp->Value[valueIdx], indexes);
        }
    }
    break;
    case glm::engine::GDAAttributeType::UINT32:
    {
        const UGDAInt32Property* gdaSpecificProp = static_cast<const UGDAInt32Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->Value.Num())
        {
            attribute->setUint32Value(gdaSpecificProp->Value[valueIdx], indexes);
        }
    }
    break;
    case glm::engine::GDAAttributeType::UINT64:
    {
        const UGDAInt64Property* gdaSpecificProp = static_cast<const UGDAInt64Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->Value.Num())
        {
            attribute->setUint64Value(gdaSpecificProp->Value[valueIdx], indexes);
        }
    }
    break;
    case glm::engine::GDAAttributeType::FLOAT:
    {
        const UGDAFloatProperty* gdaSpecificProp = static_cast<const UGDAFloatProperty*>(gdaProp);
        if (valueIdx < gdaSpecificProp->Value.Num())
        {
            const float& uValue = gdaSpecificProp->Value[valueIdx];
            float gdaValue;
            AGolaemSimulation::unrealToGda(
                uValue,
                gdaValue,
                crowdUnitFactor,
                (GDAPropertyDataType::Type)attribute->getDataTypeId());
            attribute->setFloatValue(gdaValue, indexes);
        }
    }
    break;
    case glm::engine::GDAAttributeType::VEC3:
    {
        const UGDAVector3Property* gdaSpecificProp = static_cast<const UGDAVector3Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->Value.Num())
        {
            const FVector& uValue = gdaSpecificProp->Value[valueIdx];
            glm::Vector3 gdaValue;
            AGolaemSimulation::unrealToGda(
                uValue,
                gdaValue,
                crowdUnitFactor,
                (GDAPropertyDataType::Type)attribute->getDataTypeId());
            attribute->setVec3Value(gdaValue.getFloatValues(), indexes);
        }
    }
    break;
    case glm::engine::GDAAttributeType::VEC4:
    {
        const UGDAVector4Property* gdaSpecificProp = static_cast<const UGDAVector4Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->Value.Num())
        {
            const FVector4& uValue = gdaSpecificProp->Value[valueIdx];
            glm::Vector4 gdaValue;
            AGolaemSimulation::unrealToGda(
                uValue,
                gdaValue,
                crowdUnitFactor,
                (GDAPropertyDataType::Type)attribute->getDataTypeId());
            attribute->setVec4Value(gdaValue.getFloatValues(), indexes);
        }
    }
    break;
    case glm::engine::GDAAttributeType::STRING:
    {
        const UGDAStringProperty* gdaSpecificProp = static_cast<const UGDAStringProperty*>(gdaProp);
        if (valueIdx < gdaSpecificProp->Value.Num())
        {
            const FString& uValue = gdaSpecificProp->Value[valueIdx];
            attribute->setStringValue(TCHAR_TO_ANSI(*uValue), indexes);
        }
    }
    break;
    case glm::engine::GDAAttributeType::INT32ARRAY:
    {
        const UGDAInt32Property* gdaSpecificProp = static_cast<const UGDAInt32Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->ArraySizes.Num())
        {
            size_t startIndex = 0;
            for (size_t iElem = 0; iElem < valueIdx; ++iElem)
            {
                startIndex += gdaSpecificProp->ArraySizes[iElem];
            }
            size_t count = gdaSpecificProp->ArraySizes[valueIdx];
            if (attribute->getDimension() == 0)
            {
                // startIndex should be 0 here, take all the elements (do not rely on ArraySize)
                count = gdaSpecificProp->Value.Num();
            }
            if (startIndex < gdaSpecificProp->Value.Num())
            {
                attribute->setInt32ArrayValue(&gdaSpecificProp->Value[startIndex], count, indexes);
            }
        }
    }
    break;
    case glm::engine::GDAAttributeType::INT64ARRAY:
    {
        const UGDAInt64Property* gdaSpecificProp = static_cast<const UGDAInt64Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->ArraySizes.Num())
        {
            size_t startIndex = 0;
            for (size_t iElem = 0; iElem < valueIdx; ++iElem)
            {
                startIndex += gdaSpecificProp->ArraySizes[iElem];
            }
            size_t count = gdaSpecificProp->ArraySizes[valueIdx];
            if (attribute->getDimension() == 0)
            {
                // startIndex should be 0 here, take all the elements (do not rely on ArraySize)
                count = gdaSpecificProp->Value.Num();
            }
            if (startIndex < gdaSpecificProp->Value.Num())
            {
                attribute->setInt64ArrayValue(&gdaSpecificProp->Value[startIndex], count, indexes);
            }
        }
    }
    break;
    case glm::engine::GDAAttributeType::UINT32ARRAY:
    {
        const UGDAInt32Property* gdaSpecificProp = static_cast<const UGDAInt32Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->ArraySizes.Num())
        {
            size_t startIndex = 0;
            for (size_t iElem = 0; iElem < valueIdx; ++iElem)
            {
                startIndex += gdaSpecificProp->ArraySizes[iElem];
            }
            size_t count = gdaSpecificProp->ArraySizes[valueIdx];
            if (attribute->getDimension() == 0)
            {
                // startIndex should be 0 here, take all the elements (do not rely on ArraySize)
                count = gdaSpecificProp->Value.Num();
            }
            if (startIndex < gdaSpecificProp->Value.Num())
            {
                attribute->setUint32ArrayValue((uint32*)&gdaSpecificProp->Value[startIndex], count, indexes);
            }
        }
    }
    break;
    case glm::engine::GDAAttributeType::UINT64ARRAY:
    {
        const UGDAInt64Property* gdaSpecificProp = static_cast<const UGDAInt64Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->ArraySizes.Num())
        {
            size_t startIndex = 0;
            for (size_t iElem = 0; iElem < valueIdx; ++iElem)
            {
                startIndex += gdaSpecificProp->ArraySizes[iElem];
            }
            size_t count = gdaSpecificProp->ArraySizes[valueIdx];
            if (attribute->getDimension() == 0)
            {
                // startIndex should be 0 here, take all the elements (do not rely on ArraySize)
                count = gdaSpecificProp->Value.Num();
            }
            if (startIndex < gdaSpecificProp->Value.Num())
            {
                attribute->setUint64ArrayValue((uint64*)&gdaSpecificProp->Value[startIndex], count, indexes);
            }
        }
    }
    break;
    case glm::engine::GDAAttributeType::FLOATARRAY:
    {
        const UGDAFloatProperty* gdaSpecificProp = static_cast<const UGDAFloatProperty*>(gdaProp);
        if (valueIdx < gdaSpecificProp->ArraySizes.Num())
        {
            size_t startIndex = 0;
            for (size_t iElem = 0; iElem < valueIdx; ++iElem)
            {
                startIndex += gdaSpecificProp->ArraySizes[iElem];
            }
            size_t count = gdaSpecificProp->ArraySizes[valueIdx];
            if (attribute->getDimension() == 0)
            {
                // startIndex should be 0 here, take all the elements (do not rely on ArraySize)
                count = gdaSpecificProp->Value.Num();
            }
            if (startIndex < gdaSpecificProp->Value.Num())
            {
                glm::PODArray<float> gdaValues(count);
                for (size_t iElem = 0; iElem < count; ++iElem)
                {
                    AGolaemSimulation::unrealToGda(
                        gdaSpecificProp->Value[startIndex + iElem],
                        gdaValues[iElem],
                        crowdUnitFactor,
                        (GDAPropertyDataType::Type)attribute->getDataTypeId());
                }
                attribute->setFloatArrayValue(gdaValues.begin(), count, indexes);
            }
        }
    }
    break;
    case glm::engine::GDAAttributeType::VEC3ARRAY:
    {
        const UGDAVector3Property* gdaSpecificProp = static_cast<const UGDAVector3Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->ArraySizes.Num())
        {
            size_t startIndex = 0;
            for (size_t iElem = 0; iElem < valueIdx; ++iElem)
            {
                startIndex += gdaSpecificProp->ArraySizes[iElem];
            }
            size_t count = gdaSpecificProp->ArraySizes[valueIdx];
            if (attribute->getDimension() == 0)
            {
                // startIndex should be 0 here, take all the elements (do not rely on ArraySize)
                count = gdaSpecificProp->Value.Num();
            }
            if (startIndex < gdaSpecificProp->Value.Num())
            {
                glm::PODArray<float> gdaValues(count * 3);
                glm::Vector3 gdaValue;
                for (size_t iElem = 0, gdaIdx = 0; iElem < count; ++iElem, gdaIdx += 3)
                {
                    AGolaemSimulation::unrealToGda(
                        gdaSpecificProp->Value[startIndex + iElem],
                        gdaValue,
                        crowdUnitFactor,
                        (GDAPropertyDataType::Type)attribute->getDataTypeId());
                    gdaValues[gdaIdx] = gdaValue.x;
                    gdaValues[gdaIdx + 1] = gdaValue.y;
                    gdaValues[gdaIdx + 2] = gdaValue.z;
                }
                attribute->setVec3ArrayValue(gdaValues.begin(), count, indexes);
            }
        }
    }
    break;
    case glm::engine::GDAAttributeType::VEC4ARRAY:
    {
        const UGDAVector4Property* gdaSpecificProp = static_cast<const UGDAVector4Property*>(gdaProp);
        if (valueIdx < gdaSpecificProp->ArraySizes.Num())
        {
            size_t startIndex = 0;
            for (size_t iElem = 0; iElem < valueIdx; ++iElem)
            {
                startIndex += gdaSpecificProp->ArraySizes[iElem];
            }
            size_t count = gdaSpecificProp->ArraySizes[valueIdx];
            if (attribute->getDimension() == 0)
            {
                // startIndex should be 0 here, take all the elements (do not rely on ArraySize)
                count = gdaSpecificProp->Value.Num();
            }
            if (startIndex < gdaSpecificProp->Value.Num())
            {
                glm::PODArray<float> gdaValues(count * 4);
                glm::Vector4 gdaValue;
                for (size_t iElem = 0, gdaIdx = 0; iElem < count; ++iElem, gdaIdx += 4)
                {
                    AGolaemSimulation::unrealToGda(
                        gdaSpecificProp->Value[startIndex + iElem],
                        gdaValue,
                        crowdUnitFactor,
                        (GDAPropertyDataType::Type)attribute->getDataTypeId());
                    gdaValues[gdaIdx] = gdaValue.x;
                    gdaValues[gdaIdx + 1] = gdaValue.y;
                    gdaValues[gdaIdx + 2] = gdaValue.z;
                    gdaValues[gdaIdx + 3] = gdaValue.w;
                }
                attribute->setVec4ArrayValue(gdaValues.begin(), count, indexes);
            }
        }
    }
    break;
    case glm::engine::GDAAttributeType::MESH:
    case glm::engine::GDAAttributeType::MESHARRAY:
    {
        // nothing to set
    }
    break;
    default:
        break;
    }
}

//-------------------------------------------------------------------------
void SetGDAAttributeValueRecursive(glm::engine::GDAAttribute* attribute, const UGDAProperty* gdaProp, size_t currentDimension, size_t* indexes, float crowdUnitFactor)
{
    if (currentDimension == 0)
    {
        SetGDAAttributeSingleValue(attribute, gdaProp, indexes, crowdUnitFactor);
    }
    else
    {
        for (size_t iTopIdx = 0, dimSize = attribute->getSizes()[currentDimension - 1]; iTopIdx < dimSize; ++iTopIdx)
        {
            indexes[currentDimension - 1] = iTopIdx;
            SetGDAAttributeValueRecursive(attribute, gdaProp, currentDimension - 1, indexes, crowdUnitFactor);
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::OverrideGDAAttributes()
{
    glm::engine::GDAContainer& container = _glmEngine->touchDigitalAssetNode();
    for (int32 iProp = 0, propCount = GDAProperties.Num(); iProp < propCount; ++iProp)
    {
        UGDAProperty* gdaProp = GDAProperties[iProp];
        if (gdaProp != NULL && gdaProp->Override)
        {
            glm::engine::GDAAttribute* attribute = container.findAttribute(TCHAR_TO_ANSI(*gdaProp->GDAAttributeFullName));
            if (attribute == NULL)
            {
                GLM_CROWD_TRACE_WARNING("Could not find GDA Attribute '" << TCHAR_TO_ANSI(*gdaProp->GDAAttributeShortName) << "' to override in GDA file '" << TCHAR_TO_ANSI(*GDAFile.FilePath) << "'");
                continue;
            }
            if (!attribute->isPublic())
            {
                GLM_CROWD_TRACE_WARNING("GDA Attribute '" << TCHAR_TO_ANSI(*gdaProp->GDAAttributeShortName) << "' in GDA file '" << TCHAR_TO_ANSI(*GDAFile.FilePath) << "' is not public, no override will be applied.");
                continue;
            }
            size_t dimension = gdaProp->Dimension;
            glm::PODArray<size_t> sizes(dimension, size_t(0));
            SetGDAAttributeSizes(attribute, gdaProp, sizes.begin());
            attribute->resize(dimension, sizes.begin());
            glm::PODArray<size_t> indexes(dimension, size_t(0));
            SetGDAAttributeValueRecursive(attribute, gdaProp, dimension, indexes.begin(), _crowdUnitFactor);
        }
    }
}

//-------------------------------------------------------------------------
float AGolaemSimulation::gdaToUnrealFloat(const float& gdaValue, GDAPropertyDataType::Type dataType)
{
    float unrealValue;
    AGolaemSimulation::gdaToUnreal(gdaValue, unrealValue, _crowdUnitFactor, dataType);
    return unrealValue;
}

//-------------------------------------------------------------------------
FVector AGolaemSimulation::gdaToUnrealVector3(const FVector& gdaValue, GDAPropertyDataType::Type dataType)
{
    FVector unrealValue;
    AGolaemSimulation::gdaToUnreal(glm::Vector3(static_cast<float>(gdaValue.X), static_cast<float>(gdaValue.Y), static_cast<float>(gdaValue.Z)), unrealValue, _crowdUnitFactor, dataType);
    return unrealValue;
}

//-------------------------------------------------------------------------
FVector4 AGolaemSimulation::gdaToUnrealVector4(const FVector4& gdaValue, GDAPropertyDataType::Type dataType)
{
    FVector4 unrealValue;
    AGolaemSimulation::gdaToUnreal(glm::Vector4(static_cast<float>(gdaValue.X), static_cast<float>(gdaValue.Y), static_cast<float>(gdaValue.Z), static_cast<float>(gdaValue.W)), unrealValue, _crowdUnitFactor, dataType);
    return unrealValue;
}

//-------------------------------------------------------------------------
void AGolaemSimulation::gdaToUnreal(
    const float& gdaValue,
    float& unrealValue,
    float crowdUnitFactor,
    GDAPropertyDataType::Type dataType)
{
    unrealValue = gdaValue;
    switch (dataType)
    {
    case glm::engine::GDAAttributeDataType::DISTANCE:
    case glm::engine::GDAAttributeDataType::SPEED:
    case glm::engine::GDAAttributeDataType::ACCELERATION:
    case glm::engine::GDAAttributeDataType::FORCE:
    {
        // apply scale
        unrealValue *= crowdUnitFactor;
    }
    break;
    default:
        break;
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::gdaToUnreal(
    const glm::Vector3& gdaValue,
    FVector& unrealValue,
    float crowdUnitFactor,
    GDAPropertyDataType::Type dataType)
{
    switch (dataType)
    {
    case glm::engine::GDAAttributeDataType::DISTANCE:
    case glm::engine::GDAAttributeDataType::SPEED:
    case glm::engine::GDAAttributeDataType::ACCELERATION:
    case glm::engine::GDAAttributeDataType::FORCE:
    {
        // convert to Z-up and apply scale
        unrealValue.X = gdaValue.x;
        unrealValue.Y = gdaValue.z;
        unrealValue.Z = gdaValue.y;
        unrealValue *= crowdUnitFactor;
    }
    break;
    case glm::engine::GDAAttributeDataType::ANGLE:
    {
        FQuat unrealQuat = FQuat::MakeFromEuler(FVector(gdaValue.x, gdaValue.y, gdaValue.z));
        Swap(unrealQuat.Y, unrealQuat.Z);
        unrealValue = unrealQuat.Euler();
    }
    break;
    case glm::engine::GDAAttributeDataType::DIRECTION:
    {
        // convert to Z-up without scale
        unrealValue.X = gdaValue.x;
        unrealValue.Y = gdaValue.z;
        unrealValue.Z = gdaValue.y;
    }
    break;
    default:
    {
        // do not change axes
        unrealValue.X = gdaValue.x;
        unrealValue.Y = gdaValue.y;
        unrealValue.Z = gdaValue.z;
    }
    break;
    }
}
//-------------------------------------------------------------------------
void AGolaemSimulation::gdaToUnreal(
    const glm::Vector4& gdaValue,
    FVector4& unrealValue,
    float crowdUnitFactor,
    GDAPropertyDataType::Type dataType)
{
    switch (dataType)
    {
    case glm::engine::GDAAttributeDataType::DISTANCE:
    case glm::engine::GDAAttributeDataType::SPEED:
    case glm::engine::GDAAttributeDataType::ACCELERATION:
    case glm::engine::GDAAttributeDataType::FORCE:
    {
        // convert to Z-up and apply scale
        unrealValue.X = gdaValue.x;
        unrealValue.Y = gdaValue.z;
        unrealValue.Z = gdaValue.y;
        unrealValue.W = gdaValue.w;
        unrealValue *= crowdUnitFactor;
    }
    break;
    case glm::engine::GDAAttributeDataType::ANGLE:
    {
        FQuat unrealQuat = FQuat::MakeFromEuler(FVector(gdaValue.x, gdaValue.y, gdaValue.z));
        Swap(unrealQuat.Y, unrealQuat.Z);
        unrealValue = unrealQuat.Euler();
        unrealValue.W = gdaValue.w;
    }
    break;
    case glm::engine::GDAAttributeDataType::DIRECTION:
    {
        // convert to Z-up without scale
        unrealValue.X = gdaValue.x;
        unrealValue.Y = gdaValue.z;
        unrealValue.Z = gdaValue.y;
        unrealValue.W = gdaValue.w;
    }
    break;
    default:
    {
        // do not change axes
        unrealValue.X = gdaValue.x;
        unrealValue.Y = gdaValue.y;
        unrealValue.Z = gdaValue.z;
        unrealValue.W = gdaValue.w;
    }
    break;
    }
}

//-------------------------------------------------------------------------
float AGolaemSimulation::unrealToGdaFloat(const float& unrealValue, GDAPropertyDataType::Type dataType)
{
    float gdaValue;
    unrealToGda(unrealValue, gdaValue, _crowdUnitFactor, dataType);
    return gdaValue;
}

//-------------------------------------------------------------------------
FVector AGolaemSimulation::unrealToGdaVector3(const FVector& unrealValue, GDAPropertyDataType::Type dataType)
{
    glm::Vector3 gdaValue;
    unrealToGda(unrealValue, gdaValue, _crowdUnitFactor, dataType);
    return FVector(gdaValue.x, gdaValue.y, gdaValue.z);
}

//-------------------------------------------------------------------------
FVector4 AGolaemSimulation::unrealToGdaVector4(const FVector4& unrealValue, GDAPropertyDataType::Type dataType)
{
    glm::Vector4 gdaValue;
    unrealToGda(unrealValue, gdaValue, _crowdUnitFactor, dataType);
    return FVector4(gdaValue.x, gdaValue.y, gdaValue.z, gdaValue.w);
}

//-------------------------------------------------------------------------
void AGolaemSimulation::unrealToGda(
    const float& unrealValue,
    float& gdaValue,
    float crowdUnitFactor,
    GDAPropertyDataType::Type dataType)
{
    gdaValue = unrealValue;
    switch (dataType)
    {
    case glm::engine::GDAAttributeDataType::DISTANCE:
    case glm::engine::GDAAttributeDataType::SPEED:
    case glm::engine::GDAAttributeDataType::ACCELERATION:
    case glm::engine::GDAAttributeDataType::FORCE:
    {
        // apply scale
        gdaValue /= crowdUnitFactor;
    }
    default:
        break;
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::unrealToGda(
    const float& unrealValue,
    double& gdaValue,
    float crowdUnitFactor,
    GDAPropertyDataType::Type dataType)
{
    float gdaValueFloat;
    unrealToGda(unrealValue, gdaValueFloat, crowdUnitFactor, dataType);
    gdaValue = gdaValueFloat;
}

//-------------------------------------------------------------------------
void AGolaemSimulation::unrealToGda(
    const FVector& unrealValue,
    glm::Vector3& gdaValue,
    float crowdUnitFactor,
    GDAPropertyDataType::Type dataType)
{
    switch (dataType)
    {
    case glm::engine::GDAAttributeDataType::DISTANCE:
    case glm::engine::GDAAttributeDataType::SPEED:
    case glm::engine::GDAAttributeDataType::ACCELERATION:
    case glm::engine::GDAAttributeDataType::FORCE:
    {
        // convert to Y-up and apply scale
        gdaValue.setValues(unrealValue.X, unrealValue.Z, unrealValue.Y);
        gdaValue /= crowdUnitFactor;
    }
    break;
    case glm::engine::GDAAttributeDataType::ANGLE:
    {
        FQuat gdaQuat = FQuat::MakeFromEuler(unrealValue);
        Swap(gdaQuat.Y, gdaQuat.Z);
        FVector gdaEuler = gdaQuat.Euler();
        gdaValue.setValues(static_cast<float>(gdaEuler.X), static_cast<float>(gdaEuler.Y), static_cast<float>(gdaEuler.Z));
    }
    break;
    case glm::engine::GDAAttributeDataType::DIRECTION:
    {
        // convert to Y-up without scale
        gdaValue.setValues(unrealValue.X, unrealValue.Z, unrealValue.Y);
    }
    break;
    default:
    {
        // do not change axes
        gdaValue.setValues(unrealValue.X, unrealValue.Y, unrealValue.Z);
    }
    break;
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::unrealToGda(
    const FVector4& unrealValue,
    glm::Vector4& gdaValue,
    float crowdUnitFactor,
    GDAPropertyDataType::Type dataType)
{
    switch (dataType)
    {
    case glm::engine::GDAAttributeDataType::DISTANCE:
    case glm::engine::GDAAttributeDataType::SPEED:
    case glm::engine::GDAAttributeDataType::ACCELERATION:
    case glm::engine::GDAAttributeDataType::FORCE:
    {
        // convert to Y-up and apply scale
        gdaValue.setValues(unrealValue.X, unrealValue.Z, unrealValue.Y, unrealValue.W);
        gdaValue /= crowdUnitFactor;
    }
    break;
    case glm::engine::GDAAttributeDataType::ANGLE:
    {
        FQuat gdaQuat = FQuat::MakeFromEuler(unrealValue);
        Swap(gdaQuat.Y, gdaQuat.Z);
        FVector4 gdaEuler = gdaQuat.Euler();
        gdaEuler.W = unrealValue.W;
        gdaValue.setValues(static_cast<float>(gdaEuler.X), static_cast<float>(gdaEuler.Y), static_cast<float>(gdaEuler.Z), static_cast<float>(gdaEuler.W));
    }
    break;
    case glm::engine::GDAAttributeDataType::DIRECTION:
    {
        // convert to Y-up without scale
        gdaValue.setValues(unrealValue.X, unrealValue.Z, unrealValue.Y, unrealValue.W);
    }
    break;
    default:
    {
        // do not change axes
        gdaValue.setValues(unrealValue.X, unrealValue.Y, unrealValue.Z, unrealValue.W);
    }
    break;
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::OverridePoptoolFromMesh(const FString& poptoolName, AStaticMeshActor* meshActor)
{
    bool canContinue = true;
    UGDAProperty* poptoolPosProp = NULL;
    UGDAProperty* poptoolOriProp = NULL;
    if (meshActor == NULL)
    {
        GLM_CROWD_TRACE_ERROR("OverridePoptoolFromMesh: No mesh connected!");
        canContinue = false;
    }
    if (poptoolName.IsEmpty())
    {
        GLM_CROWD_TRACE_ERROR("OverridePoptoolFromMesh: Population tool name empty!");
        canContinue = false;
    }
    else
    {
        FString poptoolPosAttrName = poptoolName + ".positions";
        poptoolPosProp = FindGdaPropertyByShortName(poptoolPosAttrName);
        if (poptoolPosProp == NULL)
        {
            GLM_CROWD_TRACE_ERROR("OverridePoptoolFromMesh: GDA Property not found: " << TCHAR_TO_ANSI(*poptoolPosAttrName) << ". The GDA attribute must exist and be public.");
            canContinue = false;
        }
        else if (!poptoolPosProp->IsA(UGDAVector3Property::StaticClass()))
        {
            GLM_CROWD_TRACE_ERROR("OverridePoptoolFromMesh: GDA Property " << TCHAR_TO_ANSI(*poptoolPosProp->GDAAttributeShortName) << " is not a Vector3 GDA Property!");
            canContinue = false;
        }
        FString poptoolOriAttrName = poptoolName + ".orientations";
        poptoolOriProp = FindGdaPropertyByShortName(poptoolOriAttrName);
        if (poptoolOriProp == NULL)
        {
            GLM_CROWD_TRACE_ERROR("OverridePoptoolFromMesh: GDA Property not found: " << TCHAR_TO_ANSI(*poptoolOriAttrName) << ". The GDA attribute must exist and be public.");
            canContinue = false;
        }
        else if (!poptoolOriProp->IsA(UGDAVector3Property::StaticClass()))
        {
            GLM_CROWD_TRACE_ERROR("OverridePoptoolFromMesh: GDA Property " << TCHAR_TO_ANSI(*poptoolOriProp->GDAAttributeShortName) << " is not a Vector3 GDA Property!");
            canContinue = false;
        }
    }

    if (!canContinue)
    {
        return;
    }
    poptoolPosProp->Override = true;
    poptoolOriProp->Override = true;
    UGDAVector3Property* poptoolPosSpecificProp = static_cast<UGDAVector3Property*>(poptoolPosProp);
    UGDAVector3Property* poptoolOriSpecificProp = static_cast<UGDAVector3Property*>(poptoolOriProp);

    if (meshActor->GetStaticMeshComponent() != NULL && meshActor->GetStaticMeshComponent()->GetStaticMesh() != NULL)
    {
        UStaticMesh* staticMesh = meshActor->GetStaticMeshComponent()->GetStaticMesh();
        if (staticMesh->HasValidRenderData())
        {
            int vertexCount = staticMesh->GetNumVertices(0);
            poptoolPosSpecificProp->Value.SetNum(vertexCount);
            poptoolOriSpecificProp->Value.SetNum(vertexCount);

            FStaticMeshVertexBuffers& vertexBuffers = staticMesh->GetRenderData()->LODResources[0].VertexBuffers;

            FPositionVertexBuffer& positionBuffer = vertexBuffers.PositionVertexBuffer;

            FTransform meshTransform = meshActor->GetStaticMeshComponent()->GetComponentTransform();
            FVector meshEulerAngles = meshTransform.GetRotation().Euler();
            for (int iVertex = 0; iVertex < vertexCount; ++iVertex)
            {
                poptoolPosSpecificProp->Value[iVertex] = meshTransform.TransformPosition(FVector(positionBuffer.VertexPosition(iVertex)));
                poptoolOriSpecificProp->Value[iVertex] = meshEulerAngles;
            }
        }
    }
}

//-------------------------------------------------------------------------
void AGolaemSimulation::ApplySimuShaderData(int simulationIndex)
{
    FSimulationData& thisSimuData = DataPerSimulation[simulationIndex];

    const glm::engine::EngineSimulation* engineSimulation = _glmEngine->_engineSimulations[simulationIndex];

    AGolaemCache::ApplySimuShaderData(thisSimuData, engineSimulation->getSimData(), &engineSimulation->getSrcShaderDataContainer(), engineSimulation->getGolaemCharacters());
}

//-------------------------------------------------------------------------
void AGolaemSimulation::ApplyFrameShaderData(int simulationIndex)
{
    FSimulationData& thisSimuData = DataPerSimulation[simulationIndex];

    const glm::engine::EngineSimulation* engineSimulation = _glmEngine->_engineSimulations[simulationIndex];

    const glm::crowdio::GlmSimulationData* simulationData = engineSimulation->getSimData();
    const glm::ShaderAssetDataContainer& shaderDataContainer = engineSimulation->getSrcShaderDataContainer();

    AGolaemCache::ApplyFrameShaderData(thisSimuData, &shaderDataContainer, engineSimulation->getGolaemCharacters());
}

//-------------------------------------------------------------------------
void AGolaemSimulation::RefreshGdaFile()
{
    UpdateGdaPublicAttributes();
    SetupSimulationPreview();
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyInt32(const FString& shortName, int32 value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAInt32Property::StaticClass()))
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAInt64Property::StaticClass()))
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAFloatProperty::StaticClass()))
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector3Property::StaticClass()))
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = FVector(value);
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector4Property::StaticClass()))
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = FVector4(value, value, value, value);
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyInt64(const FString& shortName, int64 value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAInt32Property::StaticClass()))
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = (int32)value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAInt64Property::StaticClass()))
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAFloatProperty::StaticClass()))
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector3Property::StaticClass()))
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = FVector(value);
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector4Property::StaticClass()))
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = FVector4(value);
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyFloat(const FString& shortName, float value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAInt32Property::StaticClass()))
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = (int32)value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAInt64Property::StaticClass()))
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = (int64)value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAFloatProperty::StaticClass()))
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector3Property::StaticClass()))
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = FVector(value);
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector4Property::StaticClass()))
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = FVector4(value);
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyVector3(const FString& shortName, const FVector& value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAInt32Property::StaticClass()))
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() < 3)
        {
            gdaSpecificProp->Value.SetNum(3);
        }
        for (size_t i = 0; i < 3; ++i)
        {
            gdaSpecificProp->Value[i] = (int32)value[i];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAInt64Property::StaticClass()))
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() < 3)
        {
            gdaSpecificProp->Value.SetNum(3);
        }
        for (size_t i = 0; i < 3; ++i)
        {
            gdaSpecificProp->Value[i] = (int64)value[i];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAFloatProperty::StaticClass()))
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() < 3)
        {
            gdaSpecificProp->Value.SetNum(3);
        }
        for (size_t i = 0; i < 3; ++i)
        {
            gdaSpecificProp->Value[i] = value[i];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector3Property::StaticClass()))
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector4Property::StaticClass()))
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyVector4(const FString& shortName, const FVector4& value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAInt32Property::StaticClass()))
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() < 4)
        {
            gdaSpecificProp->Value.SetNum(4);
        }
        for (size_t i = 0; i < 4; ++i)
        {
            gdaSpecificProp->Value[i] = (int32)value[i];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAInt64Property::StaticClass()))
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() < 4)
        {
            gdaSpecificProp->Value.SetNum(4);
        }
        for (size_t i = 0; i < 4; ++i)
        {
            gdaSpecificProp->Value[i] = (int64)value[i];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAFloatProperty::StaticClass()))
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() < 4)
        {
            gdaSpecificProp->Value.SetNum(4);
        }
        for (size_t i = 0; i < 4; ++i)
        {
            gdaSpecificProp->Value[i] = value[i];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector3Property::StaticClass()))
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector4Property::StaticClass()))
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyString(const FString& shortName, const FString& value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAStringProperty::StaticClass()))
    {
        UGDAStringProperty* gdaSpecificProp = static_cast<UGDAStringProperty*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyMesh(const FString& shortName, AStaticMeshActor* value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAMeshProperty::StaticClass()))
    {
        UGDAMeshProperty* gdaSpecificProp = static_cast<UGDAMeshProperty*>(gdaProperty);
        if (gdaSpecificProp->Value.Num() == 0)
        {
            gdaSpecificProp->Value.SetNum(1);
        }
        gdaSpecificProp->Value[0] = value;
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyInt32Array(const FString& shortName, const TArray<int32>& value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAInt32Property::StaticClass()))
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProperty);
        gdaSpecificProp->Value = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAInt64Property::StaticClass()))
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = value[iElem];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAFloatProperty::StaticClass()))
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = value[iElem];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector3Property::StaticClass()))
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = FVector(value[iElem]);
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector4Property::StaticClass()))
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = FVector4(value[iElem]);
        }
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyInt64Array(const FString& shortName, const TArray<int64>& value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAInt32Property::StaticClass()))
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = value[iElem];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAInt64Property::StaticClass()))
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProperty);
        gdaSpecificProp->Value = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAFloatProperty::StaticClass()))
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = value[iElem];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector3Property::StaticClass()))
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = FVector(value[iElem]);
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector4Property::StaticClass()))
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = FVector4(value[iElem]);
        }
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyFloatArray(const FString& shortName, const TArray<float>& value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAInt32Property::StaticClass()))
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = value[iElem];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAInt64Property::StaticClass()))
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = value[iElem];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAFloatProperty::StaticClass()))
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProperty);
        gdaSpecificProp->Value = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector3Property::StaticClass()))
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = FVector(value[iElem]);
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector4Property::StaticClass()))
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = FVector4(value[iElem]);
        }
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyVector3Array(const FString& shortName, const TArray<FVector>& value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAInt32Property::StaticClass()))
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num() * 3);
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[3 * iElem] = value[iElem][0];
            gdaSpecificProp->Value[3 * iElem + 1] = value[iElem][1];
            gdaSpecificProp->Value[3 * iElem + 2] = value[iElem][2];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAInt64Property::StaticClass()))
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num() * 3);
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[3 * iElem] = value[iElem][0];
            gdaSpecificProp->Value[3 * iElem + 1] = value[iElem][1];
            gdaSpecificProp->Value[3 * iElem + 2] = value[iElem][2];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAFloatProperty::StaticClass()))
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num() * 3);
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[3 * iElem] = value[iElem][0];
            gdaSpecificProp->Value[3 * iElem + 1] = value[iElem][1];
            gdaSpecificProp->Value[3 * iElem + 2] = value[iElem][2];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector3Property::StaticClass()))
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProperty);
        gdaSpecificProp->Value = value;
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector4Property::StaticClass()))
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = value[iElem];
        }
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyVector4Array(const FString& shortName, const TArray<FVector4>& value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAInt32Property::StaticClass()))
    {
        UGDAInt32Property* gdaSpecificProp = static_cast<UGDAInt32Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num() * 4);
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[4 * iElem] = value[iElem][0];
            gdaSpecificProp->Value[4 * iElem + 1] = value[iElem][1];
            gdaSpecificProp->Value[4 * iElem + 2] = value[iElem][2];
            gdaSpecificProp->Value[4 * iElem + 3] = value[iElem][3];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAInt64Property::StaticClass()))
    {
        UGDAInt64Property* gdaSpecificProp = static_cast<UGDAInt64Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num() * 4);
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[4 * iElem] = value[iElem][0];
            gdaSpecificProp->Value[4 * iElem + 1] = value[iElem][1];
            gdaSpecificProp->Value[4 * iElem + 2] = value[iElem][2];
            gdaSpecificProp->Value[4 * iElem + 3] = value[iElem][3];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAFloatProperty::StaticClass()))
    {
        UGDAFloatProperty* gdaSpecificProp = static_cast<UGDAFloatProperty*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num() * 4);
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[4 * iElem] = value[iElem][0];
            gdaSpecificProp->Value[4 * iElem + 1] = value[iElem][1];
            gdaSpecificProp->Value[4 * iElem + 2] = value[iElem][2];
            gdaSpecificProp->Value[4 * iElem + 3] = value[iElem][3];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector3Property::StaticClass()))
    {
        UGDAVector3Property* gdaSpecificProp = static_cast<UGDAVector3Property*>(gdaProperty);
        gdaSpecificProp->Value.SetNum(value.Num());
        for (int32 iElem = 0, elemCount = value.Num(); iElem < elemCount; ++iElem)
        {
            gdaSpecificProp->Value[iElem] = value[iElem];
        }
        valueSet = true;
    }
    else if (gdaProperty->IsA(UGDAVector4Property::StaticClass()))
    {
        UGDAVector4Property* gdaSpecificProp = static_cast<UGDAVector4Property*>(gdaProperty);
        gdaSpecificProp->Value = value;
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyStringArray(const FString& shortName, const TArray<FString>& value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAStringProperty::StaticClass()))
    {
        UGDAStringProperty* gdaSpecificProp = static_cast<UGDAStringProperty*>(gdaProperty);
        gdaSpecificProp->Value = value;
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}

//-------------------------------------------------------------------------
bool AGolaemSimulation::SetGDAPropertyMeshArray(const FString& shortName, const TArray<AStaticMeshActor*>& value)
{
    UGDAProperty* gdaProperty = FindGdaPropertyByShortName(shortName);
    if (gdaProperty == NULL)
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not find property '" << TCHAR_TO_ANSI(*shortName) << "'");
        return false;
    }

    bool valueSet = false;
    if (gdaProperty->IsA(UGDAMeshProperty::StaticClass()))
    {
        UGDAMeshProperty* gdaSpecificProp = static_cast<UGDAMeshProperty*>(gdaProperty);
        gdaSpecificProp->Value = value;
        valueSet = true;
    }
    else
    {
        GLM_CROWD_TRACE_ERROR(__FUNCTION__ << ": could not set property '" << TCHAR_TO_ANSI(*shortName) << "' value, incompatible type!");
    }

    if (valueSet)
    {
        // show that the property is overridden
        gdaProperty->Override = true;
    }

    return valueSet;
}