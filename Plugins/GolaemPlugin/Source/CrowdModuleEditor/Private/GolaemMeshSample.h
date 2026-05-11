/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "Components.h"

enum class ESampleReadFlags : uint8;

/** Structure for storing individual track samples */
struct FGolaemMeshSample
{
	FGolaemMeshSample() : NumSmoothingGroups(0)
		, NumUVSets(1)
		, NumMaterials(0)
		, SampleTime(0.0f)
	{}

	/** Constructing from other sample*/
	FGolaemMeshSample(const FGolaemMeshSample& InSample)
	{
		Vertices = InSample.Vertices;
		Indices = InSample.Indices;
		Normals = InSample.Normals;
		TangentX = InSample.TangentX;
		TangentY = InSample.TangentY;
		for (uint32 UVIndex = 0; UVIndex < InSample.NumUVSets; ++UVIndex)
		{
			UVs[UVIndex] = InSample.UVs[UVIndex];
		}
		Colors = InSample.Colors;
		/*Visibility = InSample.Visibility;
		VisibilityIndices = InSample.VisibilityIndices;*/
		MaterialIndices = InSample.MaterialIndices;
		SmoothingGroupIndices = InSample.SmoothingGroupIndices;
		NumSmoothingGroups = InSample.NumSmoothingGroups;
		NumMaterials = InSample.NumMaterials;
		SampleTime = InSample.SampleTime;
		NumUVSets = InSample.NumUVSets;
	}

	void Reset(const ESampleReadFlags ReadFlags);
	void Copy(const FGolaemMeshSample& InSample, const ESampleReadFlags ReadFlags);

	TArray<FVector> Vertices;
	TArray<uint32> Indices;

	// Vertex attributes (per index based)
	TArray<FVector> Normals;
	TArray<FVector> TangentX;
	TArray<FVector> TangentY;
	TArray<FVector2D> UVs[MAX_TEXCOORDS];

	TArray<FLinearColor> Colors;
	/*TArray<FVector2D> Visibility;
	TArray<uint32> VisibilityIndices;*/

	// Per Face material and smoothing group index
	TArray<int32> MaterialIndices;
	TArray<uint32> SmoothingGroupIndices;

	/** Number of smoothing groups and different materials (will always be at least 1) */
	uint32 NumSmoothingGroups;
	uint32 NumUVSets;
	uint32 NumMaterials;

	// Time in track this sample was taken from
	float SampleTime;
};

