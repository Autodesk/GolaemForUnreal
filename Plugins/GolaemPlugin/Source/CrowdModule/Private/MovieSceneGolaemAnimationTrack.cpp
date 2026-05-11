/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include <MovieSceneGolaemAnimationTrack.h>
//#include "Evaluation/MovieSceneEvaluationCustomVersion.h"
//#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include <Compilation/MovieSceneCompilerRules.h>
#include <Evaluation/MovieSceneEvaluationTrack.h>
#include "MovieSceneGolaemSectionTemplate.h"
#include <Compilation/IMovieSceneTemplateGenerator.h>
#include <MovieScene.h>
#include <MovieSceneBinding.h>

#include "GolaemCache.h"
#include "MovieSceneGolaemSection.h"

#define LOCTEXT_NAMESPACE "MovieSceneGolaemAnimationTrack"


/* UMovieSceneGolaemAnimationTrack structors
 *****************************************************************************/
UMovieSceneGolaemAnimationTrack::UMovieSceneGolaemAnimationTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
//	, bUseLegacySectionIndexBlend(false)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(124, 15, 124, 65);
#endif

	SupportedBlendTypes.Add(EMovieSceneBlendType::Absolute);

	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
}


/* UMovieSceneGolaemAnimationTrack interface
 *****************************************************************************/
UMovieSceneSection* UMovieSceneGolaemAnimationTrack::AddNewGolaemCacheSectionOnRow(FFrameNumber KeyTime)
{
	UMovieSceneGolaemSection* NewSection = Cast<UMovieSceneGolaemSection>(CreateNewSection());
	if (NewSection)
	{
		AddSection(*NewSection);
	}

	return NewSection;
}

TArray<UMovieSceneSection*> UMovieSceneGolaemAnimationTrack::GetGolaemCacheSectionAtTime(FFrameNumber Time)
{
	TArray<UMovieSceneSection*> Sections;
	for (auto Section : GolaemCacheSections)
	{
		if (Section->IsTimeWithinSection(Time))
		{
			Sections.Add(Section);
		}
	}

	return Sections;
}


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneGolaemAnimationTrack::PostLoad()
{
	Super::PostLoad();
}

bool UMovieSceneGolaemAnimationTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
    return SectionClass == UMovieSceneGolaemSection::StaticClass();
}

const TArray<UMovieSceneSection*>& UMovieSceneGolaemAnimationTrack::GetAllSections() const
{
	return GolaemCacheSections;
}


// only one row containing the cache ! we can imagine a copy of the same section to repeat it, not more.
bool UMovieSceneGolaemAnimationTrack::SupportsMultipleRows() const
{
	return false;
}


UMovieSceneSection* UMovieSceneGolaemAnimationTrack::CreateNewSection()
{
	return NewObject<UMovieSceneGolaemSection>(this, NAME_None, RF_Transactional);
}


void UMovieSceneGolaemAnimationTrack::RemoveAllAnimationData()
{
	GolaemCacheSections.Empty();
}


bool UMovieSceneGolaemAnimationTrack::HasSection(const UMovieSceneSection& Section) const
{
	return GolaemCacheSections.Contains(&Section);
}


void UMovieSceneGolaemAnimationTrack::AddSection(UMovieSceneSection& Section)
{
	UMovieSceneGolaemSection* GolaemSection = Cast<UMovieSceneGolaemSection>(&Section);
	if (GolaemSection && TrackGolaemCache.IsValid())
	{
		if (!GolaemSection->Params.bSplitting)
		{
            FFrameRate tickResolution = GetTypedOuter<UMovieScene>()->GetTickResolution();

            AGolaemCache* GolaemCache = Cast<AGolaemCache>(TrackGolaemCache.ResolveObject());
            GolaemSection->Params.GolaemCache = TrackGolaemCache;
            GolaemSection->Params.CacheStartFrame = 1;
            GolaemSection->Params.CacheEndFrame = 25;
            if (GolaemCache)
            {
                GolaemSection->Params.CacheStartFrame = GolaemCache->StartFrame;
                GolaemSection->Params.CacheEndFrame = GolaemCache->EndFrame;
            }
            GolaemSection->Params.FirstLoopStartFrameOffset = FFrameNumber(0);
            GolaemSection->Params.EndFrameOffset = FFrameNumber(0);
            // GolaemSection->Params.PlayRate = TrackGolaemCache->FrameRate;
            // for now consider that the fps of cache and moveScene matches
            GolaemSection->Params.PlayRate = GetTypedOuter<UMovieScene>()->GetDisplayRate().Numerator;
            FFrameTime startTime = ((GolaemSection->Params.CacheStartFrame / GolaemSection->Params.PlayRate) * tickResolution);
            FFrameTime AnimationLength = (GolaemSection->Params.CacheEndFrame - GolaemSection->Params.CacheStartFrame) / GolaemSection->Params.PlayRate * tickResolution;
            GolaemSection->InitialPlacementOnRow(GolaemCacheSections, startTime.FrameNumber.Value, AnimationLength.FrameNumber.Value, 0 /*INDEX_NONE*/);
        }
		// else keep duplicated section

		GolaemCacheSections.Add(&Section);
	}
}


void UMovieSceneGolaemAnimationTrack::RemoveSection(UMovieSceneSection& Section)
{
	GolaemCacheSections.Remove(&Section);
}


bool UMovieSceneGolaemAnimationTrack::IsEmpty() const
{
	return GolaemCacheSections.Num() == 0;
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneGolaemAnimationTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Crowd Cache");
}

#endif

// overlapping should never happen as we only allow one track !
FMovieSceneTrackRowSegmentBlenderPtr UMovieSceneGolaemAnimationTrack::GetRowSegmentBlender() const
{
	// Apply an upper bound exclusive blend
	struct FSkeletalAnimationRowCompilerRules : FMovieSceneTrackRowSegmentBlender
	{
		//bool bUseLegacySectionIndexBlend;
		FSkeletalAnimationRowCompilerRules(/*bool bInUseLegacySectionIndexBlend*/) /*: bUseLegacySectionIndexBlend(bInUseLegacySectionIndexBlend)*/ {}

		virtual void Blend(FSegmentBlendData& BlendData) const override
		{
			MovieSceneSegmentCompiler::ChooseLowestRowIndex(BlendData);

			// Run the default high pass filter for overlap priority
			//MovieSceneSegmentCompiler::FilterOutUnderlappingSections(BlendData);

			//if (bUseLegacySectionIndexBlend)
			//{
			//	// Weed out based on array index (legacy behaviour)
			//	MovieSceneSegmentCompiler::BlendSegmentLegacySectionOrder(BlendData);
			//}
		}
	};
	return FSkeletalAnimationRowCompilerRules(/*bUseLegacySectionIndexBlend*/);
}

FMovieSceneEvalTemplatePtr UMovieSceneGolaemAnimationTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneGolaemSectionTemplate(*CastChecked<UMovieSceneGolaemSection>(&InSection));
}

#undef LOCTEXT_NAMESPACE
