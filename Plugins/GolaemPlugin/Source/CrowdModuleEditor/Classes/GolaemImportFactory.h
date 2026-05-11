/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "Factories/Factory.h"
#include "EditorReimportHandler.h"

#include "GolaemSkeletalMesh.h"
#include "GolaemImporter.h"

#include "GolaemImportFactory.generated.h" // needs to be included last

class UGolaemImportSettings;
class FGolaemImporter;
//class UGeometryCache;
//class UStaticMesh;
class USkeletalMesh;
//class UGolaemAssetImportData;
class SGolaemImportOptions;
class SGolaemTrackSelectionWindow;

UCLASS(hidecategories=Object)
class UGolaemImportFactory : public UFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	/** Object used to show import options for Golaem */
	UPROPERTY()
	UGolaemImportSettings* ImportSettings;

	UPROPERTY()
	bool bShowOption;

	//~ Begin UObject Interface
	void PostInitProperties();
	//~ End UObject Interface

	float ScaleVariable;
	bool bImportBlendshapes;
	EGolaemMaterialImportLocation::Value MaterialImportLocation;
	//bool ImportAsStaticMesh;

	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual bool DoesSupportClass(UClass * Class) override;
	virtual UClass* ResolveSupportedClass() override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	//~ End UFactory Interface

	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;

	void ShowImportOptionsWindow(TSharedPtr<SGolaemImportOptions>& Options, FString FilePath, const FGolaemImporter& Importer);

	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface
		
	/**
	* ImportGeometryCache
	*	
	* @param Importer - GolaemImporter instance
	* @param InParent - Parent for the GeometryCache asset
	* @param Flags - Creation Flags for the GeometryCache asset
	* @return UObject*
	*/
	UObject* ImportSkeletalMesh(FGolaemImporter& Importer, UObject* InParent, EObjectFlags Flags);

	//UObject* ImportStaticMeshes(FGolaemImporter& Importer, UObject* InParent, EObjectFlags Flags);

	/**
	* ReimportGeometryCache
	*
	* @param Cache - Geometry cache instance to re import
	* @return EReimportResult::Type
	*/
	EReimportResult::Type ReimportSkeletalMesh(UGolaemSkeletalMesh* SkeletalMesh);

	//void PopulateOptionsWithImportData(UGolaemAssetImportData* ImportData);
};