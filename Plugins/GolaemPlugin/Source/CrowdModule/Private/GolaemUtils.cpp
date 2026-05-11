/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemUtils.h" //needs to be included first

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "GolaemUtils"

THIRD_PARTY_INCLUDES_START
#include "glmWindows.h" // takes care of no min max define
#include "glmAssetManagementUtils.h"
#include "glmString.h"
#include "glmStringOperators.h"
#include "glmArray.h"
#include "glmUtils.h"
THIRD_PARTY_INCLUDES_END

//-------------------------------------------------------------------------
void GolaemUtils::dirmapString(FString& aFilePathsString)
{
    glm::GlmString dirmapStr = glm::getEnvironmentVariable("GLM_DIRMAP");
    glm::Array<glm::GlmString> dirmaps;
    glm::GlmString delim(";");
    dirmaps = glm::stringToStringArray(dirmapStr, delim);
    if (dirmaps.size() > 1)
    {
        TArray<FString> aFilePathList;
        aFilePathsString.ParseIntoArray(aFilePathList, TEXT(";"), true);

        glm::GlmString dirmappedString;
        FString newFilePaths;
        bool setNewFilePaths = false;
        glm::GlmString correctedFilePath;
        for (int32 iFilePath = 0; iFilePath < aFilePathList.Num(); iFilePath++)
        {
            glm::GlmString filePathtoDirMap = TCHAR_TO_ANSI(*aFilePathList[iFilePath]);
            if (!filePathtoDirMap.empty())
            {
                bool dirmapped = glm::findDirmappedFile(correctedFilePath, filePathtoDirMap, dirmaps);
                setNewFilePaths |= dirmapped;
                if(dirmapped)
                {
                    // force forward slash in resolution
                    correctedFilePath = glm::replaceChar(correctedFilePath, '\\', '/');;
                    newFilePaths += correctedFilePath.c_str();
                }
                else
                {
                    newFilePaths += filePathtoDirMap.c_str();
                }

                if (iFilePath != (aFilePathList.Num() - 1))
                {
                    newFilePaths += ";";
                }
            }
        }
        // if at least one matched dirmap occured, replace value
        if (setNewFilePaths)
        {
            aFilePathsString = newFilePaths;
        }
    }
}

//-----------------------------------------------------------------------------
void GolaemUtils::ShowNotification(const FString& notificationStr, const FString& notificationURL, float expireDuration)
{
    FText notificationText = FText::FromString(notificationStr);
    FNotificationInfo Info(notificationText);

    // icon
    FSlateIcon golaemIcon("GolaemUIStyle", "Golaem.Icon16");
    if (golaemIcon.GetIcon() != FStyleDefaults::GetNoBrush())
        Info.Image = golaemIcon.GetIcon();

    // url
    if (notificationURL.Len())
    {
        Info.Hyperlink = FSimpleDelegate::CreateStatic([](FString SourceFilePath) { FPlatformProcess::ExploreFolder(*(FPaths::GetPath(SourceFilePath))); }, notificationURL);
        Info.HyperlinkText = FText::FromString(notificationURL);
    }
    // more
    Info.FadeInDuration = 0.1f;
    Info.FadeOutDuration = 0.5f;
    Info.ExpireDuration = expireDuration;
    Info.bUseThrobber = false;
    Info.bUseSuccessFailIcons = true;
    Info.bUseLargeFont = true;
    Info.bFireAndForget = false;
    Info.bAllowThrottleWhenFrameRateIsLow = false;
    auto NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
    NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
    NotificationItem->ExpireAndFadeout();
}

//-----------------------------------------------------------------------------
float GolaemUtils::getCrowdUnitFactor()
{
    float crowdUnitFactor = 100.f; //default factor to 100 as for maya

    glm::GlmString CrowdUnitStr = glm::getEnvironmentVariable("GLMCROWD_UNIT");

    if (CrowdUnitStr == "0") //millimeters
    {
        crowdUnitFactor = 0.1f;
    }
    else if (CrowdUnitStr == "1") //centimeters
    {
        crowdUnitFactor = 1.f;
    }
    else if (CrowdUnitStr == "2") //decimeters
    {
        crowdUnitFactor = 10.f;
    }
    else if (CrowdUnitStr == "3") //meters
    {
        crowdUnitFactor = 100.f;
    }
    else if (CrowdUnitStr == "4") //inches
    {
        crowdUnitFactor = 2.54f;
    }
    else if (CrowdUnitStr == "5") //feet
    {
        crowdUnitFactor = 30.48f;
    }
    else if (CrowdUnitStr == "6") //yard
    {
        crowdUnitFactor = 91.44f;
    }

    return crowdUnitFactor;
}

#undef LOCTEXT_NAMESPACE // "GolaemUtils"
