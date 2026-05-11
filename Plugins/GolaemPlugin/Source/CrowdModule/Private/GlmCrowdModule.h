/***************************************************************************
 *                                                                          *
 *  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
 *                                                                          *
 ***************************************************************************/

#pragma once

#include "Modules/ModuleInterface.h"
#include "GolaemCustomSettings.h"
#include "Interfaces/IPluginManager.h"

THIRD_PARTY_INCLUDES_START
#include "glmCore.h"
#include "glmMinidump.h"
THIRD_PARTY_INCLUDES_END

const FName CrowdWindowTabName(TEXT("CrowdWindowTab"));

class FGolaemCrowdModule : public IModuleInterface
{
public:
    FGolaemCrowdModule();
    virtual ~FGolaemCrowdModule();

    CROWDMODULE_API const FString& getPluginVersionName();
    CROWDMODULE_API int32 getPluginVersionNumber();

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    TSharedPtr<IPlugin> _golaemPlugin;

    glm::Minidump _minidump;

#if WITH_EDITOR
    // Callback for when the settings were saved.
    bool HandleSettingsSaved()
    {
        UGolaemCustomSettings* Settings = GetMutableDefault<UGolaemCustomSettings>();
        bool ResaveSettings = false;

        // You can put any validation code in here and resave the settings in case an invalid
        // value has been entered

        if (ResaveSettings)
        {
            Settings->SaveConfig();
        }

        return true;
    }
#endif
};