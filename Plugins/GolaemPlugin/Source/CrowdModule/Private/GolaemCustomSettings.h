/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "HAL/Platform.h"
#include <EngineGlobals.h>

#include "GolaemCustomSettings.generated.h"

UENUM(BlueprintType)        //"BlueprintType" is essential to include
enum class EGolaemCacheMode : uint8
{
	GCM_SKINNED UMETA(DisplayName = "Skinned Actor"),
    GCM_MIXED UMETA(DisplayName = "Skinned Actor with Instanced Rigid Meshes"),
	GCM_RIGID UMETA(DisplayName = "Rigid Instanced Components"),
	Count UMETA(Hidden)
};

UCLASS(config=Engine, defaultconfig)
class CROWDMODULE_API UGolaemCustomSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, config, Category=General)
    EGolaemCacheMode CacheImportMode = EGolaemCacheMode::GCM_MIXED;
};