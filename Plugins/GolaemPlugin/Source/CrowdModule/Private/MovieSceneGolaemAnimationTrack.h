/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneNameableTrack.h"
#include "Compilation/IMovieSceneTrackTemplateProducer.h"

#include "MovieSceneGolaemAnimationTrack.generated.h"

class AGolaemCache;

/**
 * Handles animation of skeletal mesh actors
 */
UCLASS(MinimalAPI)
class UMovieSceneGolaemAnimationTrack
	: public UMovieSceneNameableTrack
	, public IMovieSceneTrackTemplateProducer
{
	GENERATED_UCLASS_BODY()

public:

	///** Adds a new animation to this track */
	//virtual UMovieSceneSection* AddNewAnimationOnRow(FFrameNumber KeyTime, class UAnimSequenceBase* AnimSequence, int32 RowIndex);

	///** Adds a new animation to this track on the next available/non-overlapping row */
	//virtual UMovieSceneSection* AddNewAnimation(FFrameNumber KeyTime, class UAnimSequenceBase* AnimSequence) { return AddNewAnimationOnRow(KeyTime, AnimSequence, INDEX_NONE); }

	///** Adds a new cache section to this track */
	virtual UMovieSceneSection* AddNewGolaemCacheSectionOnRow(FFrameNumber KeyTime);


	///** Gets the animation sections at a certain time */
	TArray<UMovieSceneSection*> GetGolaemCacheSectionAtTime(FFrameNumber Time);

	UPROPERTY(meta = (AllowedClasses = "/Script/CrowdModule.GolaemCache"))
		FSoftObjectPath TrackGolaemCache; // soft path to be able to restore link after reload 

	//UPROPERTY()
	//TWeakObjectPtr< AGolaemCache > TrackGolaemCache;

public:

	// UMovieSceneTrack interface

	virtual void PostLoad() override;
	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
    virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneTrackRowSegmentBlenderPtr GetRowSegmentBlender() const override;
	virtual bool SupportsMultipleRows() const override;

    // ~IMovieSceneTrackTemplateProducer interface
    virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

private:

	/** List of all animation sections */
	UPROPERTY()
		TArray<UMovieSceneSection*> GolaemCacheSections;

	//UPROPERTY()
	//	bool bUseLegacySectionIndexBlend;
};
