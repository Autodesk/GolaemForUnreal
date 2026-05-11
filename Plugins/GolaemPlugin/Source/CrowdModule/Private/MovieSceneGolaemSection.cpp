/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "MovieSceneGolaemSection.h"

#include "Channels/MovieSceneChannelProxy.h"
#include "Logging/MessageLog.h"
#include "MovieScene.h"
#include "UObject/SequencerObjectVersion.h"
#include "MovieSceneTimeHelpers.h"
//#include "GolaemCache.h"

#include "MovieSceneGolaemSectionTemplate.h"

#define LOCTEXT_NAMESPACE "MovieSceneGolaemSection"

namespace
{
    float GolaemCacheDeprecatedMagicNumber = TNumericLimits<float>::Lowest();

    FName DefaultSlotName("DefaultSlot");
} // namespace

FMovieSceneGolaemCacheParams::FMovieSceneGolaemCacheParams()
{
    PlayRate = 1.f;
    bReverse = false;
    SlotName = DefaultSlotName;
    StartOffset_DEPRECATED = GolaemCacheDeprecatedMagicNumber;
    EndOffset_DEPRECATED = GolaemCacheDeprecatedMagicNumber;

    CacheStartFrame = 0;
    CacheEndFrame = 0;
}

UMovieSceneGolaemSection::UMovieSceneGolaemSection(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    BlendType = EMovieSceneBlendType::Absolute;
    EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::ProjectDefault);

#if WITH_EDITOR

    PreviousPlayRate = Params.PlayRate;

#endif
}

TOptional<FFrameTime> UMovieSceneGolaemSection::GetOffsetTime() const
{
    return TOptional<FFrameTime>(Params.FirstLoopStartFrameOffset);
}

void UMovieSceneGolaemSection::Serialize(FArchive& Ar)
{
    Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);
    Super::Serialize(Ar);
}

void UMovieSceneGolaemSection::PostLoad()
{
    Super::PostLoad();

    FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();

    if (Params.StartOffset_DEPRECATED != GolaemCacheDeprecatedMagicNumber)
    {
        Params.FirstLoopStartFrameOffset = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Params.StartOffset_DEPRECATED).Value;

        Params.StartOffset_DEPRECATED = GolaemCacheDeprecatedMagicNumber;
    }

    if (Params.EndOffset_DEPRECATED != GolaemCacheDeprecatedMagicNumber)
    {
        Params.EndFrameOffset = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Params.EndOffset_DEPRECATED).Value;

        Params.EndOffset_DEPRECATED = GolaemCacheDeprecatedMagicNumber;
    }

    //if (Params.GeometryCache_DEPRECATED.ResolveObject() != nullptr
    //	&& Params.GeometryCacheAsset == nullptr)
    //{
    //	UGeometryCacheComponent *Comp = Cast<UGeometryCacheComponent>(Params.GeometryCache_DEPRECATED.ResolveObject());
    //	if (Comp)
    //	{
    //		Params.GeometryCacheAsset = (Comp->GetGeometryCache());
    //	}
    //}
}

FFrameNumber GetStartOffsetAtTrimTime(FQualifiedFrameTime TrimTime, const FMovieSceneGolaemCacheParams& Params, FFrameNumber StartFrame, FFrameRate FrameRate)
{
    return Params.FirstLoopStartFrameOffset + TrimTime.Time.FrameNumber - StartFrame;

    //float AnimPlayRate = FMath::IsNearlyZero(Params.PlayRate) ? 1.0f : Params.PlayRate;
    //float AnimPosition = (TrimTime.Time - StartFrame) / TrimTime.Rate * AnimPlayRate; // -> trim time inside section in cache frame rate
    //float SeqLength = Params.GetSequenceLength() - FrameRate.AsSeconds(Params.FirstLoopStartFrameOffset + Params.EndFrameOffset) * AnimPlayRate; // to get removed frames we need time in seconds multiplied by playRate in frames/s

    //FFrameNumber NewOffset = FrameRate.AsFrameNumber(FMath::Fmod(AnimPosition, SeqLength));
    //NewOffset += Params.FirstLoopStartFrameOffset;//    +FrameRate.AsFrameNumber(Params.CacheStartFrame);

    //return NewOffset;
}

TOptional<TRange<FFrameNumber>> UMovieSceneGolaemSection::GetAutoSizeRange() const
{
    FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();
    FFrameTime AnimationLength = Params.GetSequenceLength() * FrameRate;
    //int32 IFrameNumber = AnimationLength.FrameNumber.Value + (int)(AnimationLength.GetSubFrame() + 0.5f);

    //return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + IFrameNumber + 1);    
    return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + AnimationLength.FrameNumber);
}

void UMovieSceneGolaemSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys)
{
    SetFlags(RF_Transactional);

    if (TryModify())
    {
        if (bTrimLeft)
        {
            FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

            Params.FirstLoopStartFrameOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(TrimTime, Params, GetInclusiveStartFrame(), FrameRate) : 0;
        }
        else
        {
            // EndFrameOffset goes backward when positive
            Params.EndFrameOffset = HasStartFrame() ? ((GetExclusiveEndFrame() - TrimTime.Time.FrameNumber) + Params.EndFrameOffset) : 0;
        }

        Super::TrimSection(TrimTime, bTrimLeft, bDeleteKeys);
    }
}

UMovieSceneSection* UMovieSceneGolaemSection::SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys)
{
    const FFrameNumber InitialStartFrameOffset = Params.FirstLoopStartFrameOffset;

    FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

    const FFrameNumber NewOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(SplitTime, Params, GetInclusiveStartFrame(), FrameRate) : 0;

    // let addSection knows that it must not use the CacheStartFrame to position the track
    Params.bSplitting = true;
    UMovieSceneSection* NewSection = Super::SplitSection(SplitTime, bDeleteKeys);
    Params.bSplitting = false;
    if (NewSection != nullptr)
    {
        UMovieSceneGolaemSection* NewGeometrySection = Cast<UMovieSceneGolaemSection>(NewSection);
        NewGeometrySection->Params = Params;
        NewGeometrySection->Params.FirstLoopStartFrameOffset = NewOffset;
    }

    // Restore original offset modified by splitting
    Params.FirstLoopStartFrameOffset = InitialStartFrameOffset;

    return NewSection;
}

void UMovieSceneGolaemSection::GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const
{
    Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);

    const FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();
    const FFrameNumber StartFrame = GetInclusiveStartFrame();
    const FFrameNumber EndFrame = GetExclusiveEndFrame() - 1; // -1 because we don't need to add the end frame twice

    const float AnimPlayRate = FMath::IsNearlyZero(Params.PlayRate) ? 1.0f : Params.PlayRate;
    const float SeqLengthSeconds = Params.GetSequenceLength() - FrameRate.AsSeconds(Params.FirstLoopStartFrameOffset + Params.EndFrameOffset) / AnimPlayRate;

    FFrameTime SequenceFrameLength = SeqLengthSeconds * FrameRate;
    if (SequenceFrameLength.FrameNumber > 1)
    {
        // Snap to the repeat times
        FFrameTime CurrentTime = StartFrame;
        while (CurrentTime < EndFrame)
        {
            OutSnapTimes.Add(CurrentTime.FrameNumber);
            CurrentTime += SequenceFrameLength;
        }
    }
}

float UMovieSceneGolaemSection::MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const
{
    FMovieSceneGolaemSectionTemplateParameters TemplateParams(Params, GetInclusiveStartFrame(), GetExclusiveEndFrame());
    return TemplateParams.MapTimeToAnimation(InPosition, InFrameRate);
}

#if WITH_EDITOR
void UMovieSceneGolaemSection::PreEditChange(FProperty* PropertyAboutToChange)
{
    // Store the current play rate so that we can compute the amount to compensate the section end time when the play rate changes
    PreviousPlayRate = Params.PlayRate;

    Super::PreEditChange(PropertyAboutToChange);
}

void UMovieSceneGolaemSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    // Adjust the duration automatically if the play rate changes
    if (PropertyChangedEvent.Property != nullptr &&
        PropertyChangedEvent.Property->GetFName() == TEXT("PlayRate"))
    {
        float NewPlayRate = Params.PlayRate;

        if (!FMath::IsNearlyZero(NewPlayRate))
        {
            Params.FirstLoopStartFrameOffset = Params.FirstLoopStartFrameOffset * (PreviousPlayRate / NewPlayRate);
            Params.EndFrameOffset = Params.EndFrameOffset * (PreviousPlayRate / NewPlayRate);

            float CurrentDuration = UE::MovieScene::DiscreteSize(GetRange());
            float NewDuration = CurrentDuration * (PreviousPlayRate / NewPlayRate);
            SetEndFrame(GetInclusiveStartFrame() + FMath::FloorToInt(NewDuration));

            PreviousPlayRate = NewPlayRate;
        }
    }

    Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#undef LOCTEXT_NAMESPACE
