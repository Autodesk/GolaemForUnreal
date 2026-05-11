/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "MovieSceneGolaemSectionTemplate.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"
#include "UObject/ObjectKey.h"

#include "GolaemCache.h"

DECLARE_CYCLE_STAT(TEXT("Golaem Cache Evaluate"), MovieSceneEval_Golaem_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Golaem Cache Token Execute"), MovieSceneEval_Golaem_TokenExecute, STATGROUP_MovieSceneEval);

/** Used to set Manual Tick back to previous when outside section */
struct FPreAnimatedGolaemTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{
		struct FToken : IMovieScenePreAnimatedToken
		{
			FToken(AGolaemCache* InCache)
			{
				// Cache this object's current update flag and animation mode
				bInManualTick = InCache->GetManualTick();
				SaveDrawEntityPercent = InCache->DrawEntityPercent;
				InCache->DrawEntityPercent = 100;
			}

            /**
             * Restore state for the specified object, only called when this token was created with a bound object
             *
             * @param Object The object to restore state for
             * @param Params Parameters for restoring state
             */
            virtual void RestoreState(UObject& Object, const UE::MovieScene::FRestoreStateParams& /*Params*/)
			{
				AGolaemCache* GolaemCache = CastChecked<AGolaemCache>(&Object);
				GolaemCache->SetManualTick(bInManualTick);
				GolaemCache->DrawEntityPercent = SaveDrawEntityPercent;
            }

            bool bInManualTick;
			float SaveDrawEntityPercent;
		};

		return FToken(CastChecked<AGolaemCache>(&Object));
	}
	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FPreAnimatedGolaemTokenProducer>();
	}
};


/** A movie scene execution token that executes a geometry cache */
struct FGolaemExecutionToken
	: IMovieSceneExecutionToken
	
{
	FGolaemExecutionToken(const FMovieSceneGolaemSectionTemplateParameters &InParams):
		Params(InParams)
	{}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		if (Params.GolaemCacheParams.GolaemCache.IsValid()/*.ResolveObject()*/)
		{
			AGolaemCache* TheGolaemCache = Cast<AGolaemCache>(Params.GolaemCacheParams.GolaemCache.ResolveObject());
			if (TheGolaemCache != NULL)
			{
				MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_Golaem_TokenExecute)

				Player.SavePreAnimatedState(*TheGolaemCache, FPreAnimatedGolaemTokenProducer::GetAnimTypeID(), FPreAnimatedGolaemTokenProducer());

				TheGolaemCache->SetManualTick(true);
				// calculate the time at which to evaluate the animation
				float EvalTime = Params.MapTimeToAnimation(Context.GetTime(), Context.GetFrameRate());
				TheGolaemCache->TickAtThisTime(EvalTime, Params.GolaemCacheParams.bReverse, true);
			}
		}

	}

	FMovieSceneGolaemSectionTemplateParameters Params;
};

FMovieSceneGolaemSectionTemplate::FMovieSceneGolaemSectionTemplate(const UMovieSceneGolaemSection& InSection)
	: GolaemSectionTemplateParams(InSection.Params, InSection.GetInclusiveStartFrame(), InSection.GetExclusiveEndFrame())
{
}

//We use a token here so we can set the manual tick state back to what it was previously when outside this section.
//This is similar to how Skeletal Animation evaluation also works.
void FMovieSceneGolaemSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_Golaem_Evaluate)
	ExecutionTokens.Add(FGolaemExecutionToken(GolaemSectionTemplateParams));
}

float FMovieSceneGolaemSectionTemplateParameters::MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const
{
    float AnimPlayRate = FMath::IsNearlyZero(GolaemCacheParams.PlayRate) ? 1.0f : GolaemCacheParams.PlayRate;

	// crop current timeline position by section start/end time
	InPosition = FMath::Clamp(InPosition, FFrameTime(SectionStartTime), FFrameTime(SectionEndTime - 1));
    FFrameTime sectionCurrentTimeInTimeLineFrameRate = InPosition - SectionStartTime;

	float sectionCurrentTimeInCacheFrameRate = static_cast<float>(InFrameRate.AsSeconds(sectionCurrentTimeInTimeLineFrameRate + GolaemCacheParams.FirstLoopStartFrameOffset)) * AnimPlayRate;
    float currentCacheFrame = sectionCurrentTimeInCacheFrameRate + GolaemCacheParams.CacheStartFrame;

	float cacheLengthInFrame = GolaemCacheParams.CacheEndFrame - GolaemCacheParams.CacheStartFrame;// -(startOffsetInCacheFrameRate + endOffsetInCacheFrameRate); 
	if (cacheLengthInFrame > 1.f)
    {
        currentCacheFrame = FMath::Fmod(currentCacheFrame - GolaemCacheParams.CacheStartFrame, cacheLengthInFrame) + GolaemCacheParams.CacheStartFrame;
    }

	if (GolaemCacheParams.bReverse)
	{
		currentCacheFrame = (cacheLengthInFrame - (currentCacheFrame - GolaemCacheParams.CacheStartFrame)) + GolaemCacheParams.CacheStartFrame;
	}

	return currentCacheFrame;
}
