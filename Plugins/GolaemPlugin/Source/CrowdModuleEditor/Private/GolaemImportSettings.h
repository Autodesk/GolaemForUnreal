/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GolaemImportSettings.generated.h"

///** Enum that describes type of asset to import */
USTRUCT(Blueprintable)
struct FGolaemNormalGenerationSettings
{
	GENERATED_USTRUCT_BODY()

	FGolaemNormalGenerationSettings()
	{
		bRecomputeNormals = false;
		HardEdgeAngleThreshold = 0.9f;
		bForceOneSmoothingGroupPerObject = false;
		bIgnoreDegenerateTriangles = true;
	}

	/** Whether or not to force smooth normals for each individual object rather than calculating smoothing groups */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NormalCalculation, meta = (EditCondition = "bRecomputeNormals"))
	bool bForceOneSmoothingGroupPerObject;

	/** Threshold used to determine whether an angle between two normals should be considered hard, closer to 0 means more smooth vs 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NormalCalculation, meta = (EditCondition = "!bForceOneSmoothingGroupPerObject", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float HardEdgeAngleThreshold;

	/** Determines whether or not the normals should be forced to be recomputed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NormalCalculation)
	bool bRecomputeNormals;

	/** Determines whether or not the degenerate triangles should be ignored when calculating tangents/normals */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NormalCalculation, meta = (EditCondition = "bRecomputeNormals"))
	bool bIgnoreDegenerateTriangles;
};

USTRUCT(Blueprintable)
struct FGolaemMaterialSettings
{
	GENERATED_USTRUCT_BODY()

	FGolaemMaterialSettings()
		: bCreateMaterials(false)
		, bFindMaterials(false)
	{}

	/** Whether or not to create materials according to found Face Set names (will not work without face sets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Materials)
	bool bCreateMaterials;

	/** Whether or not to try and find materials according to found Face Set names (will not work without face sets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Materials)
	bool bFindMaterials;
};



/** Enum that describes type of asset to import */
UENUM(Blueprintable)
enum class EGolaemConversionPreset : uint8
{
	/** Imports only the first frame as one or multiple static meshes */
	Maya UMETA(DisplayName = "Autodesk Maya"),
	/** Imports the Golaem file as flipbook and matrix animated objects */
	Max UMETA(DisplayName = "Autodesk 3ds Max"),
	Custom UMETA(DisplayName = "Custom Settings")
};

USTRUCT(Blueprintable)
struct FGolaemConversionSettings
{
	GENERATED_USTRUCT_BODY()

	FGolaemConversionSettings()
		: Preset(EGolaemConversionPreset::Maya)
		, bFlipU(false)
		, bFlipV(true)
		, Scale(FVector(1.0f, -1.0f, 1.0f))
		, Rotation(FVector::ZeroVector)
	{}

	/** Currently preset that should be applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Conversion)
	EGolaemConversionPreset Preset;

	/** Flag whether or not to flip the U channel in the Texture Coordinates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Conversion)
	bool bFlipU;

	/** Flag whether or not to flip the V channel in the Texture Coordinates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Conversion)
	bool bFlipV;

	/** Scale value that should be applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Conversion)
	FVector Scale;

	/** Rotation in Euler angles that should be applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Conversion)
	FVector Rotation;
};


/** Class that contains all options for importing an Golaem file */
UCLASS(Blueprintable)
class /*CROWDMODULE_API*/ UGolaemImportSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Accessor and initializer **/
	static UGolaemImportSettings* Get();

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ShowOnlyInnerProperties), Category = NormalCalculation)
	//FGolaemNormalGenerationSettings NormalGenerationSettings;	
	//
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ShowOnlyInnerProperties), Category = Materials)
	//FGolaemMaterialSettings MaterialSettings;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ShowOnlyInnerProperties), Category = Conversion)
	//FGolaemConversionSettings ConversionSettings;

	bool bReimport;
	int32 NumThreads;
};
