/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

//#include "GeometryCache.h"
//#include "GeometryCacheTrackFlipbookAnimation.h"
//#include "GeometryCacheTrackTransformAnimation.h"
//#include "GeometryCacheTrackStreamable.h"
//#include "GeometryCacheMeshData.h"
//#include "GeometryCacheComponent.h"

//#include "Async/ParallelFor.h"
//#include "RawMesh.h"
#include "MeshUtilities.h"

#include "GolaemMeshSample.h"
#include "GolaemImportLogger.h"
#include "GolaemImportSettings.h"

struct FGolaemMeshSample;
struct FCompressedGolaemData;

enum class ESampleReadFlags : uint8
{
	Default = 0,
	Positions = 1 << 1,
	Indices = 1 << 2,
	UVs = 1 << 3,
	Normals = 1 << 4,
	Colors = 1 << 5,
	MaterialIndices = 1 << 6
};
ENUM_CLASS_FLAGS(ESampleReadFlags);

namespace GolaemImporterUtilities
{
	bool AreVerticesEqual(const FSoftSkinVertex& V1, const FSoftSkinVertex& V2);
}
