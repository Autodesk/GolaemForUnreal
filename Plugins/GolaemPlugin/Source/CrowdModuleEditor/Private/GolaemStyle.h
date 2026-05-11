/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"


class FGolaemStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static TSharedPtr< class ISlateStyle > Get();

	CROWDMODULEEDITOR_API static FName GetStyleSetName();

    static const FSlateBrush* GetBrush(FName PropertyName, const ANSICHAR* Specifier = NULL)
    {
        return StyleSet->GetBrush(PropertyName, Specifier);
    }

private:

	static TSharedPtr< class FSlateStyleSet > StyleSet;
};