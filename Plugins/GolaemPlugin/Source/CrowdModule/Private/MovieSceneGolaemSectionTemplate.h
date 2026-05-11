/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneEvalTemplate.h"

#include "MovieSceneGolaemSection.h"
#include "MovieSceneGolaemSectionTemplate.generated.h"

USTRUCT()
struct FMovieSceneGolaemSectionTemplateParameters
{
	GENERATED_BODY()

	FMovieSceneGolaemSectionTemplateParameters() {}
	FMovieSceneGolaemSectionTemplateParameters(const FMovieSceneGolaemCacheParams& BaseParams, FFrameNumber InSectionStartTime, FFrameNumber InSectionEndTime)
		: SectionStartTime(InSectionStartTime)
		, SectionEndTime(InSectionEndTime)
	{
		GolaemCacheParams = BaseParams;
	}
	float MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const;

	UPROPERTY()
	FFrameNumber SectionStartTime;

	UPROPERTY()
	FFrameNumber SectionEndTime;

	UPROPERTY()
	FMovieSceneGolaemCacheParams GolaemCacheParams;
};

USTRUCT()
struct FMovieSceneGolaemSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneGolaemSectionTemplate() {}
	FMovieSceneGolaemSectionTemplate(const UMovieSceneGolaemSection& Section);

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FMovieSceneGolaemSectionTemplateParameters GolaemSectionTemplateParams;
};
