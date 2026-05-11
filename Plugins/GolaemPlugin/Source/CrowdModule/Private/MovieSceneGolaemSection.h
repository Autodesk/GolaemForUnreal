/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSection.h"
#include "Animation/AnimSequenceBase.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Runtime/Launch/Resources/Version.h"
#include "MovieSceneGolaemSection.generated.h"

class AGolaemCache;

USTRUCT()
struct FMovieSceneGolaemCacheParams
{
	GENERATED_BODY()

	FMovieSceneGolaemCacheParams();

    int32 GetFrameAtTime(float CacheTime)
    {
        return CacheStartFrame + CacheTime / PlayRate;
    }

	/** Gets the animation duration, modified by play rate */
    float GetDuration() const
    {
        return FMath::IsNearlyZero(PlayRate) || CacheEndFrame == CacheStartFrame ? 0.f : (CacheEndFrame - CacheStartFrame) / PlayRate;
    }

	/** Gets the animation sequence length, not modified by play rate */
    float GetSequenceLength() const
    {
        return CacheEndFrame - CacheStartFrame;
    }

	/** The offset into the beginning of the animation clip */
	UPROPERTY(EditAnywhere, Category = "Animation")
	FFrameNumber FirstLoopStartFrameOffset; // In Timeline frame rate

	/** The offset into the end of the animation clip */
	UPROPERTY(EditAnywhere, Category = "Animation")
	FFrameNumber EndFrameOffset; // In Timeline frame rate

	/** The playback rate of the animation clip */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float PlayRate;

	/** Reverse the playback of the animation clip */
	UPROPERTY(EditAnywhere, Category = "Animation")
	uint32 bReverse : 1;

	/** The slot name to use for the animation */
	UPROPERTY(EditAnywhere, Category = "Animation")
	FName SlotName;

	UPROPERTY()
	float CacheStartFrame; // filled when created on track

	UPROPERTY()
	float CacheEndFrame; // filled when created on track
	/** The animation this section plays */
	
	UPROPERTY(meta = (AllowedClasses = "/Script/CrowdModule.GolaemCache"))
	FSoftObjectPath GolaemCache; // soft path to be able to restore link after reload 

	UPROPERTY()
	float StartOffset_DEPRECATED;

	UPROPERTY()
	float EndOffset_DEPRECATED;

	//UPROPERTY()
	//TWeakObjectPtr< AGolaemCache > GolaemCache;

	// this is a run time helper to tell that new sections must not use cache info to be placed
    UPROPERTY()
	bool bSplitting = false;
};

/**
 * Movie scene section that control skeletal animation
 */
UCLASS(MinimalAPI)
class UMovieSceneGolaemSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ShowOnlyInnerProperties))
		FMovieSceneGolaemCacheParams Params;

	/** Get Frame Time as Animation Time*/
	CROWDMODULE_API float MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const;	

protected:
	//~ UMovieSceneSection interface
	virtual TOptional<TRange<FFrameNumber> > GetAutoSizeRange() const override;
	virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys) override;
	virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys) override;

	//virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft) override;
	//virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime) override;
	virtual void GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const override;
	virtual TOptional<FFrameTime> GetOffsetTime() const override;

    // replaced by IMovieSceneTrackTemplateProducer on MovieSceneGolaemAnimationTrack since 4.26

    /**~ UObject interface */
    virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

private:
	//~ UObject interface

#if WITH_EDITOR

    virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

public:
	float PreviousPlayRate;

#endif

private:
};
