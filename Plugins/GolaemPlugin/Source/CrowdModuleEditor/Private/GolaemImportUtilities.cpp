/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

//#include "GolaemImporterModulePCH.h"
#include "GolaemImportUtilities.h"
#include "Stats/StatsMisc.h"
#include "Runtime/Launch/Resources/Version.h"

#include "GolaemImporter.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#include "Rendering/SkeletalMeshLODModel.h"

#define LOCTEXT_NAMESPACE "GolaemImporterUtilities"

//
bool GolaemImporterUtilities::AreVerticesEqual(const FSoftSkinVertex& V1, const FSoftSkinVertex& V2)
{
    if (FMath::Abs(V1.Position.X - V2.Position.X) > THRESH_POINTS_ARE_SAME || FMath::Abs(V1.Position.Y - V2.Position.Y) > THRESH_POINTS_ARE_SAME || FMath::Abs(V1.Position.Z - V2.Position.Z) > THRESH_POINTS_ARE_SAME)
    {
        return false;
    }

    // Set to 1 for now as we only import one UV set
    for (int32 UVIdx = 0; UVIdx < 1 /*MAX_TEXCOORDS*/; ++UVIdx)
    {
        if (FMath::Abs(V1.UVs[UVIdx].X - V2.UVs[UVIdx].X) > (1.0f / 1024.0f))
            return false;

        if (FMath::Abs(V1.UVs[UVIdx].Y - V2.UVs[UVIdx].Y) > (1.0f / 1024.0f))
            return false;
    }

    FVector4f N1, N2;
    N1 = V1.TangentZ;
    N2 = V2.TangentZ;

    if (FMath::Abs(N1.X - N2.X) > THRESH_NORMALS_ARE_SAME || FMath::Abs(N1.Y - N2.Y) > THRESH_NORMALS_ARE_SAME || FMath::Abs(N1.Z - N2.Z) > THRESH_NORMALS_ARE_SAME)
    {
        return false;
    }

    return true;
}

#undef LOCTEXT_NAMESPACE // "GolaemImporterUtilities"
