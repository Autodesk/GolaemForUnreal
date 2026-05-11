/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <glmLog.h>
#include <glmSingleton.h>
THIRD_PARTY_INCLUDES_END

DECLARE_LOG_CATEGORY_EXTERN(GlmLog, Log, All);

class CROWDMODULE_API FGolaemUELogger : public glm::ILogger
{
public:
    virtual ~FGolaemUELogger();
    virtual void trace(glm::Log::Module module, glm::Log::Severity severity, const char* msg, const char* file, int line, const char* operation);
};

typedef glm::Singleton<FGolaemUELogger> theGolaemLogger;
