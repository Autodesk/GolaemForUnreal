/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

//#include "GolaemImporterModulePCH.h"
#include "GolaemImportSettings.h"
#include "UObject/Class.h"
#include "UObject/Package.h"

UGolaemImportSettings::UGolaemImportSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	//ImportType = EGolaemImportType::SKeletalMesh;
	bReimport = false;
}

UGolaemImportSettings* UGolaemImportSettings::Get()
{	
	static UGolaemImportSettings* DefaultSettings = nullptr;
	if (!DefaultSettings)
	{
		// This is a singleton, use default object
		DefaultSettings = DuplicateObject(GetMutableDefault<UGolaemImportSettings>(), GetTransientPackage());
		DefaultSettings->AddToRoot();
	}
	
	return DefaultSettings;
}
