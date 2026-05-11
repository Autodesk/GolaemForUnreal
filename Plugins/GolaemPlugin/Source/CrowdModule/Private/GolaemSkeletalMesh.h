/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "Engine/SkeletalMesh.h"

#include "GolaemSkeletalMesh.generated.h" //needs to be included last

UCLASS()
class CROWDMODULE_API UGolaemSkeletalMesh : public USkeletalMesh
{
	GENERATED_UCLASS_BODY()

public:
	//UGolaemSkeletalMesh(const FObjectInitializer& ObjectInitializer);

	#if WITH_EDITOR
	/** This is a bit hacky. If you are inherriting from SkeletalMesh you can opt out of using the skeletal mesh actor factory. Note that this only works for one level of inherritence and is not a good long term solution */
	virtual bool HasCustomActorFactory() const override
	{
		return true;
	}

	/** This is a bit hacky. If you are inherriting from SkeletalMesh you can opt out of using the skeletal mesh actor factory. Note that this only works for one level of inherritence and is not a good long term solution */
	virtual bool HasCustomActorReimportFactory() const override
	{
		return true;
	}
	#endif
};
