/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemUELogger.h"
THIRD_PARTY_INCLUDES_START

THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY(GlmLog);

//-----------------------------------------------------------------------------
FGolaemUELogger::~FGolaemUELogger()
{
}

//-----------------------------------------------------------------------------
void FGolaemUELogger::trace(glm::Log::Module module, glm::Log::Severity severity, const char* msg, const char*, int, const char*)
{
    //if (module == glm::Log::CROWD)
    {
        FString logstring = msg;
        switch (severity)
        {
            // warning: AiMsgError & AiMsgFatal stop Arnold rendering
        case glm::Log::LOG_ERROR:
            UE_LOG(GlmLog, Error, TEXT("[Golaem::ERROR] %s"), *logstring);
            break;
        case glm::Log::LOG_WARNING:
            UE_LOG(GlmLog, Warning, TEXT("[Golaem::WARNING] %s"), *logstring);
            break;
        case glm::Log::LOG_INFO:
            UE_LOG(GlmLog, Display, TEXT("[Golaem::INFO] %s"), *logstring);
            break;
        case glm::Log::LOG_DEBUG:
            UE_LOG(GlmLog, Verbose, TEXT("[Golaem::DEBUG] %s"), *logstring);
            break;
        default:
            break;
        }
    }
}