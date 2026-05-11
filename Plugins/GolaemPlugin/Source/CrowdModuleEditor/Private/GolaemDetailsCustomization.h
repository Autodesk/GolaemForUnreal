/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Editor/PropertyEditor/Public/IDetailCustomization.h"

class FGolaemDetailsCustomization : public IDetailCustomization
{
public:

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	static TSharedRef<IDetailCustomization> MakeInstance();

	// utility class
    static void ExportSelectedAsTerrain();

	//FReply ExportDestTerrain();
    FReply RefreshTerrains();
	FReply OpenLayoutFileButton();
	FReply RefreshGDAButton();

	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
};