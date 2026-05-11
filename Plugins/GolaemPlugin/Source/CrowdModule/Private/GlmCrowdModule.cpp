/***************************************************************************
 *                                                                          *
 *  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
 *                                                                          *
 ***************************************************************************/

#include "GlmCrowdModule.h"

#include "Modules/ModuleManager.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsContainer.h"
#include "ISettingsSection.h"
#endif
#include "EngineGlobals.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "CoreMinimal.h"
#include "GlmCacheTypes.h"
#include "GolaemCache.h"
#include "Engine/SkeletalMesh.h"
#include "Modules/ModuleManager.h"

#include "GolaemUtils.h"
#include "GolaemSimulation.h"
#include "GolaemUELogger.h"

#include <EngineGlobals.h>

THIRD_PARTY_INCLUDES_START
#include "glmCrowdIO.h"
#include "glmEngineLibrary.h"
#include "glmUtils.h"
#include "glmADP.h"
THIRD_PARTY_INCLUDES_END

#define LOCTEXT_NAMESPACE "FGolaemCrowdModule"

//-----------------------------------------------------------------------------
FGolaemCrowdModule::FGolaemCrowdModule()
    : _golaemPlugin(NULL)
{
    ////////////////////////////////////////////////////
    // Get Golaem Plugin
    ////////////////////////////////////////////////////
    _golaemPlugin = IPluginManager::Get().FindPlugin("GolaemPlugin");
    if (!_golaemPlugin.IsValid())
    {
        GLM_CROWD_TRACE_ERROR("Error while fetching the Golaem Plugin for Unreal. Only rendering features are available");
    }
}

//-----------------------------------------------------------------------------
FGolaemCrowdModule::~FGolaemCrowdModule()
{
}

//-----------------------------------------------------------------------------
const FString& FGolaemCrowdModule::getPluginVersionName()
{
    return _golaemPlugin->GetDescriptor().VersionName;
}

//-----------------------------------------------------------------------------
int32 FGolaemCrowdModule::getPluginVersionNumber()
{
    return _golaemPlugin->GetDescriptor().Version;
}

//-----------------------------------------------------------------------------
void FGolaemCrowdModule::StartupModule()
{
    glm::initCore(); // inits logs
    glm::getLog()->_logSeverity[glm::Log::SDK] = glm::Log::LOG_ERROR;
    glm::getLog()->_logSeverity[glm::Log::CROWD] = glm::Log::LOG_WARNING;
    glm::getLog()->_logSeverity[glm::Log::DCC] = glm::Log::LOG_WARNING;

    theGolaemLogger::create();

    ////////////////////////////////////////////////////
    glm::GlmString releaseLabel = TCHAR_TO_ANSI(*getPluginVersionName());
    const FString UEMinidumpDir = FPlatformMisc::GetEnvironmentVariable(TEXT("GLM_CRASH_REPORT_DIR"));
    glm::GlmString minidumpDir = TCHAR_TO_ANSI(*UEMinidumpDir);
    if (!minidumpDir.empty())
    {
        minidumpDir = replaceChar(minidumpDir, '\\', '/');
        minidumpDir += "/golaemPlugin_";
        minidumpDir += releaseLabel;
        minidumpDir += ".dmp";

        glm::strcpySafe(_minidump._directory, glm::Minidump::_directoryPathLengthMax, minidumpDir.c_str());
        _minidump.start(glm::Minidump::Reporting::MESSAGE_BOX);
    }

    glm::GlmString engineIdentifier;
#if UE_BUILD_SHIPPING != 1
    // desktopPlatform is only available in development
    IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
    if (desktopPlatform != NULL)
    {
        engineIdentifier = TCHAR_TO_ANSI(*desktopPlatform->GetCurrentEngineIdentifier());
    }
#endif
    glm::crowdio::setupGolaemProduct("GolaemForUnreal", engineIdentifier.c_str());
    glm::crowdio::init();
    glm::crowdio::displayADPDialog("en", true, NULL);
    glm::engine::initEngine();

    glm::Singleton<GolaemCacheEditorWrapper>::create();

#if WITH_EDITOR
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        // Create the new category
        ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer("Project");

        // SettingsContainer->DescribeCategory("CustomSettings",
        //	LOCTEXT("RuntimeWDCategoryName", "CustomSettings"),
        //	LOCTEXT("RuntimeWDCategoryDescription", "Configuration for the Golaem Plugin"));

        // Register the settings
        ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "Golaem",
                                                                               LOCTEXT("RuntimeGeneralSettingsName", "Golaem"),
                                                                               LOCTEXT("RuntimeGeneralSettingsDescription", "Configure the Golaem Plugin"),
                                                                               GetMutableDefault<UGolaemCustomSettings>());

        // Register the save handler to your settings, you might want to use it to
        // validate those or just act to settings changes.
        if (SettingsSection.IsValid())
        {
            SettingsSection->OnModified().BindRaw(this, &FGolaemCrowdModule::HandleSettingsSaved);
        }
    }
#endif
}

//-----------------------------------------------------------------------------
void FGolaemCrowdModule::ShutdownModule()
{
    // Ensure to unregister all of your registered settings here, hot-reload would
    // otherwise yield unexpected results.
#if WITH_EDITOR
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->UnregisterSettings("Project", "Plugins", "Golaem");
    }
#endif

    glm::Singleton<GolaemCacheEditorWrapper>::destroy();

    glm::engine::finishEngine();
    glm::crowdio::finish();

    _minidump.stop();
    theGolaemLogger::destroy();
    glm::finishCore();
}

#undef LOCTEXT_NAMESPACE //"CrowdModule"

IMPLEMENT_MODULE(FGolaemCrowdModule, CrowdModule)