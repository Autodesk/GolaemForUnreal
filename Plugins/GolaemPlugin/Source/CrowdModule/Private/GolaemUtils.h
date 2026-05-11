/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"

//************************************************************
/*! @name Utilities
*/
//*********************************************************
//@{
namespace GolaemUtils
{
    //------------------------------------------------------------
    //! ShowNotifications
    //------------------------------------------------------------
    CROWDMODULE_API void ShowNotification(const FString& notificationText, const FString& notificationURL = "", float expireDuration = 20.f);

    CROWDMODULE_API float getCrowdUnitFactor();

    CROWDMODULE_API void dirmapString(FString& aString);
} // namespace GolaemUtils

//@}