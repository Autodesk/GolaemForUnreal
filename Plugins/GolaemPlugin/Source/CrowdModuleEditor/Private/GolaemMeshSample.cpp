/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

//#include "GolaemImporterModulePCH.h"
#include "GolaemMeshSample.h"
#include "GolaemImportUtilities.h"

#include "Modules/ModuleManager.h"
#include "MeshUtilities.h"

static const ESampleReadFlags ReadAllFlags = ESampleReadFlags::Positions | ESampleReadFlags::Indices | ESampleReadFlags::UVs | ESampleReadFlags::Normals | ESampleReadFlags::Colors | ESampleReadFlags::MaterialIndices;

void FGolaemMeshSample::Reset(const ESampleReadFlags ReadFlags)
{
	if (EnumHasAnyFlags(ReadFlags, ESampleReadFlags::Positions))
	{
		Vertices.SetNum(0, false);
	}

	if (EnumHasAnyFlags(ReadFlags, ESampleReadFlags::Indices))
	{
		Indices.SetNum(0, false);
	}

	if (EnumHasAnyFlags(ReadFlags, ESampleReadFlags::Normals))
	{
		Normals.SetNum(0, false);
		TangentX.SetNum(0, false);
		TangentY.SetNum(0, false);
	}

	if (EnumHasAnyFlags(ReadFlags, ESampleReadFlags::UVs))
	{
		for (uint32 UVIndex = 0; UVIndex < MAX_TEXCOORDS; ++UVIndex)
		{
			UVs[UVIndex].SetNum(0, false);
		}
	}

	if (EnumHasAnyFlags(ReadFlags, ESampleReadFlags::Colors))
	{
		Colors.SetNum(0, false);
	}

	if (EnumHasAnyFlags(ReadFlags, ESampleReadFlags::MaterialIndices))
	{
		MaterialIndices.SetNum(0, false);
	}

	SmoothingGroupIndices.SetNum(0, false);
	NumSmoothingGroups = 0;
	NumMaterials = 0;
	SampleTime = 0.0f;
	NumUVSets = 1;
}

void FGolaemMeshSample::Copy(const FGolaemMeshSample& InSample, const ESampleReadFlags ReadFlags)
{
	Reset(ReadFlags);

	if (!EnumHasAnyFlags(ReadFlags, ESampleReadFlags::Positions))
	{
		Vertices = InSample.Vertices;
	}
	
	if (!EnumHasAnyFlags(ReadFlags, ESampleReadFlags::Indices))
	{
		Indices = InSample.Indices;
	}

	if (!EnumHasAnyFlags(ReadFlags, ESampleReadFlags::Normals))
	{
		Normals = InSample.Normals;
		TangentX = InSample.TangentX;
		TangentY = InSample.TangentY;
		
		SmoothingGroupIndices = InSample.SmoothingGroupIndices;
		NumSmoothingGroups = InSample.NumSmoothingGroups;
	}

	if (!EnumHasAnyFlags(ReadFlags, ESampleReadFlags::UVs))
	{
		for (uint32 UVIndex = 0; UVIndex < InSample.NumUVSets; ++UVIndex)
		{
			UVs[UVIndex] = InSample.UVs[UVIndex];
		}
		NumUVSets = InSample.NumUVSets;
	}

	if (!EnumHasAnyFlags(ReadFlags, ESampleReadFlags::Colors))
	{
		Colors = InSample.Colors;
	}

	if (!EnumHasAnyFlags(ReadFlags, ESampleReadFlags::MaterialIndices))
	{
		MaterialIndices = InSample.MaterialIndices;
		NumMaterials = InSample.NumMaterials;
	}
	
	SampleTime = InSample.SampleTime;	
}
