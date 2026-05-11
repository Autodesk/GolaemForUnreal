/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemTrackEditor.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimSequenceBase.h"
#include "Modules/ModuleManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "SequencerSectionPainter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "SequencerUtilities.h"
#include "ISectionLayoutBuilder.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"
#include "EditorStyleSet.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "MovieSceneTimeHelpers.h"
#include "Fonts/FontMeasure.h"
#include "SequencerTimeSliderController.h"
#include "AnimationEditorUtils.h"
#include "Factories/PoseAssetFactory.h"
#include "Misc/MessageDialog.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "LevelSequence.h"
#include "TimeToPixel.h"


#include "MovieSceneGolaemSection.h"
#include "MovieSceneGolaemAnimationTrack.h"

#include "GolaemCache.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "glmLog.h"
THIRD_PARTY_INCLUDES_END

namespace GolaemCacheEditorConstants
{
    // @todo Sequencer Allow this to be customizable
    const uint32 GolaemTrackHeight = 20;
} // namespace GolaemCacheEditorConstants

#define LOCTEXT_NAMESPACE "FGolaemTrackEditor"

AGolaemCache* AcquireGolaemCacheFromObjectGuid(const FGuid& Guid, TSharedPtr<ISequencer> SequencerPtr)
{
    UObject* BoundObject = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(Guid) : nullptr;

    if (AGolaemCache* Actor = Cast<AGolaemCache>(BoundObject))
    {
        return Actor;
    }

    return nullptr;
}

FGolaemSection::FGolaemSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
    : Section(*CastChecked<UMovieSceneGolaemSection>(&InSection))
    , Sequencer(InSequencer)
    , InitialFirstLoopStartOffsetDuringResize(0)
    , InitialStartTimeDuringResize(0)
{
}

UMovieSceneSection* FGolaemSection::GetSectionObject()
{
    return &Section;
}

FText FGolaemSection::GetSectionTitle() const
{
    if (Section.GetOuter() != nullptr)
    {
        UMovieSceneGolaemAnimationTrack* golaemTrack = Cast<UMovieSceneGolaemAnimationTrack>(Section.GetOuter());
        if (golaemTrack->TrackGolaemCache.IsValid())
            return FText::FromString(golaemTrack->TrackGolaemCache.GetAssetName());
    }

    return LOCTEXT("GolaemCacheInstanceSection", "GolaemCacheInstance");
}

float FGolaemSection::GetSectionHeight() const
{
    return (float)GolaemCacheEditorConstants::GolaemTrackHeight;
}

int32 FGolaemSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
    const ESlateDrawEffect DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

    const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

    int32 LayerId = Painter.PaintSectionBackground();

    static const FSlateBrush* GenericDivider = FEditorStyle::GetBrush("Sequencer.GenericDivider");

    if (!Section.HasStartFrame() || !Section.HasEndFrame())
    {
        return LayerId;
    }
	
	FFrameRate TickResolution = TimeToPixelConverter.GetTickResolution();

    // Add lines where the animation starts and ends/loops
	const float AnimPlayRate = FMath::IsNearlyZero(Section.Params.PlayRate) ? 1.0f : Section.Params.PlayRate;
	const float Duration = Section.Params.GetSequenceLength();
	const float SeqLength = Duration - TickResolution.AsSeconds(Section.Params.FirstLoopStartFrameOffset + Section.Params.EndFrameOffset) / AnimPlayRate;
	const float FirstLoopSeqLength = SeqLength - TickResolution.AsSeconds(Section.Params.FirstLoopStartFrameOffset) / AnimPlayRate;

    if (!FMath::IsNearlyZero(SeqLength, KINDA_SMALL_NUMBER) && SeqLength > 0)
    {
        float MaxOffset = Section.GetRange().Size<FFrameTime>() / TickResolution;
		float OffsetTime = FirstLoopSeqLength;
        float StartTime = Section.GetInclusiveStartFrame() / TickResolution;

        while (OffsetTime < MaxOffset)
        {
            float OffsetPixel = TimeToPixelConverter.SecondsToPixel(StartTime + OffsetTime) - TimeToPixelConverter.SecondsToPixel(StartTime);

            FSlateDrawElement::MakeBox(
                Painter.DrawElements,
                LayerId,
                Painter.SectionGeometry.MakeChild(
					FVector2D(2.f, Painter.SectionGeometry.Size.Y - 2.f),
					FSlateLayoutTransform(FVector2D(OffsetPixel, 1.f))
				).ToPaintGeometry(),
                GenericDivider,
                DrawEffects
			);

            OffsetTime += SeqLength;
        }
    }

    TSharedPtr<ISequencer> SequencerPtr = Sequencer.Pin();
    if (Painter.bIsSelected && SequencerPtr.IsValid())
    {
        FFrameTime CurrentTime = SequencerPtr->GetLocalTime().Time;
        if (Section.GetRange().Contains(CurrentTime.FrameNumber) && Section.Params.CacheEndFrame > Section.Params.CacheStartFrame)
        {
            const float Time = TimeToPixelConverter.FrameToPixel(CurrentTime);

            // Draw the current time next to the scrub handle
            const float CacheTime = Section.MapTimeToAnimation(CurrentTime, TickResolution);
            //int32 FrameTime = Section.Params.GetFrameAtTime(CacheTime);
            FString FrameString = FString::FromInt(static_cast<int>(CacheTime));

            const FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
            const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            FVector2D TextSize = FontMeasureService->Measure(FrameString, SmallLayoutFont);

            // Flip the text position if getting near the end of the view range
            static const float TextOffsetPx = 10.f;
            bool bDrawLeft = (Painter.SectionGeometry.Size.X - Time) < (TextSize.X + 22.f) - TextOffsetPx;
            float TextPosition = bDrawLeft ? Time - TextSize.X - TextOffsetPx : Time + TextOffsetPx;
            //handle mirrored labels
            const float MajorTickHeight = 9.0f;
            FVector2D TextOffset(TextPosition, Painter.SectionGeometry.Size.Y - (MajorTickHeight + TextSize.Y));

            const FLinearColor DrawColor = FEditorStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle());
            const FVector2D BoxPadding = FVector2D(4.0f, 2.0f);
            // draw time string

            FSlateDrawElement::MakeBox(
                Painter.DrawElements,
                LayerId + 5,
                Painter.SectionGeometry.ToPaintGeometry(TextOffset - BoxPadding, TextSize + 2.0f * BoxPadding),
                FEditorStyle::GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                FLinearColor::Black.CopyWithNewOpacity(0.5f)
			);

            FSlateDrawElement::MakeText(
                Painter.DrawElements,
                LayerId + 6,
                Painter.SectionGeometry.ToPaintGeometry(TextOffset, TextSize),
                FrameString,
                SmallLayoutFont,
                DrawEffects,
                DrawColor
			);
        }
    }

    return LayerId;
}

void FGolaemSection::BeginResizeSection()
{
	InitialFirstLoopStartOffsetDuringResize = Section.Params.FirstLoopStartFrameOffset;
    InitialStartTimeDuringResize = Section.HasStartFrame() ? Section.GetInclusiveStartFrame() : 0;
}

void FGolaemSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime)
{
	if (ResizeMode == SSRM_LeadingEdge)
	{
		FFrameRate FrameRate = Section.GetTypedOuter<UMovieScene>()->GetTickResolution();
		FFrameNumber StartOffset = FrameRate.AsFrameNumber((ResizeTime - InitialStartTimeDuringResize) / FrameRate * Section.Params.PlayRate);

		StartOffset += InitialFirstLoopStartOffsetDuringResize;

		// Ensure start offset is not less than 0 and adjust ResizeTime
		if (StartOffset < 0)
		{
			// Ensure start offset is not less than 0 and adjust ResizeTime
			ResizeTime = ResizeTime - StartOffset;

			StartOffset = FFrameNumber(0);
		}
		else
		{
			// If the start offset exceeds the length of one loop, trim it back.
			const FFrameNumber SeqLength = FrameRate.AsFrameNumber(Section.Params.GetSequenceLength()) - Section.Params.FirstLoopStartFrameOffset - Section.Params.EndFrameOffset;
			StartOffset = StartOffset % SeqLength;
		}

		Section.Params.FirstLoopStartFrameOffset = StartOffset;
	}

	ISequencerSection::ResizeSection(ResizeMode, ResizeTime);
}

void FGolaemSection::BeginSlipSection()
{
    BeginResizeSection();
}

void FGolaemSection::SlipSection(FFrameNumber SlipTime)
{
	FFrameRate FrameRate = Section.GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber StartOffset = FrameRate.AsFrameNumber((SlipTime - InitialStartTimeDuringResize) / FrameRate * Section.Params.PlayRate);
	
	StartOffset += InitialFirstLoopStartOffsetDuringResize;

	if (StartOffset < 0)
	{
		// Ensure start offset is not less than 0 and adjust ResizeTime
		SlipTime = SlipTime - StartOffset;
		StartOffset = FFrameNumber(0);
	}
	else
	{
		// If the start offset exceeds the length of one loop, trim it back.
		const FFrameNumber SeqLength = FrameRate.AsFrameNumber(Section.Params.GetSequenceLength()) - Section.Params.FirstLoopStartFrameOffset - Section.Params.EndFrameOffset;
		StartOffset = StartOffset % SeqLength;
	}

	Section.Params.FirstLoopStartFrameOffset = StartOffset;

	ISequencerSection::SlipSection(SlipTime);
}

void FGolaemSection::BeginDilateSection()
{
	Section.PreviousPlayRate = Section.Params.PlayRate; //make sure to cache the play rate
}
void FGolaemSection::DilateSection(const TRange<FFrameNumber>& NewRange, float DilationFactor)
{
	Section.Params.PlayRate = Section.PreviousPlayRate / DilationFactor;
	Section.SetRange(NewRange);
}

void FGolaemSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding)
{
}

FGolaemTrackEditor::FGolaemTrackEditor(TSharedRef<ISequencer> InSequencer)
    : FMovieSceneTrackEditor(InSequencer)
{
}

TSharedRef<ISequencerTrackEditor> FGolaemTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
    return MakeShareable(new FGolaemTrackEditor(InSequencer));
}

bool FGolaemTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return InSequence && InSequence->IsA(ULevelSequence::StaticClass());
}

bool FGolaemTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
    return Type == UMovieSceneGolaemAnimationTrack::StaticClass();
}

TSharedRef<ISequencerSection> FGolaemTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
    check(SupportsType(SectionObject.GetOuter()->GetClass()));

    return MakeShareable(new FGolaemSection(SectionObject, GetSequencer()));
}

void FGolaemTrackEditor::AddKey(const FGuid& ObjectGuid)
{
    AGolaemCache* GolaemCache = AcquireGolaemCacheFromObjectGuid(ObjectGuid, GetSequencer());

    if (GolaemCache)
    {
        // Load the asset registry module
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

        // Collect a full list of assets with the specified class
        TArray<FAssetData> AssetDataList;
        AssetRegistryModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetClassPathName(), AssetDataList, true);

        if (AssetDataList.Num())
        {
            TSharedPtr<SWindow> Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
            if (Parent.IsValid())
            {
                FSlateApplication::Get().PushMenu(
                    Parent.ToSharedRef(),
                    FWidgetPath(),
                    BuildGolaemCacheSubMenu(ObjectGuid, GolaemCache, nullptr),
                    FSlateApplication::Get().GetCursorPos(),
                    FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
            }
        }
    }
}

bool FGolaemTrackEditor::HandleAssetAdded(UObject* /*Asset*/, const FGuid& /*TargetObjectGuid*/)
{
    return false;
}

void FGolaemTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
    if (ObjectClass->IsChildOf(USkeletalMeshComponent::StaticClass()) || ObjectClass->IsChildOf(AActor::StaticClass()))
    {		
        const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

		AGolaemCache* GolaemCache = AcquireGolaemCacheFromObjectGuid(ObjectBindings[0], GetSequencer());

        if (GolaemCache)
        {
            UMovieSceneTrack* Track = nullptr;

            MenuBuilder.AddSubMenu(
                LOCTEXT("AddAnimation", "Animation"), NSLOCTEXT("Sequencer", "AddAnimationTooltip", "Adds an animation track."),
				FNewMenuDelegate::CreateRaw(this, &FGolaemTrackEditor::AddGolaemCacheSubMenu, ObjectBindings[0], GolaemCache, Track));
        }
    }
}

TSharedRef<SWidget> FGolaemTrackEditor::BuildGolaemCacheSubMenu(FGuid ObjectBinding, AGolaemCache* GolaemCache, UMovieSceneTrack* Track)
{
    FMenuBuilder MenuBuilder(true, nullptr);

    AddGolaemCacheSubMenu(MenuBuilder, ObjectBinding, GolaemCache, Track);

    return MenuBuilder.MakeWidget();
}

void FGolaemTrackEditor::AddGolaemCacheSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, AGolaemCache* GolaemCache, UMovieSceneTrack* Track)
{
    FFrameNumber KeyTime = 0; // Add at 0 by default
    TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
    UObject* Object = SequencerPtr->FindSpawnedObjectOrTemplate(ObjectBinding);

    MenuBuilder.AddMenuEntry(
        LOCTEXT("AddCacheTrack", "Golaem Cache Track"),
        LOCTEXT("AddCacheTrackTooltip", "Add a Golaem Cache track"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FGolaemTrackEditor::AddGolaemTrack, KeyTime, Object, GolaemCache, Track)));
}

void FGolaemTrackEditor::AddGolaemTrack(FFrameNumber KeyTime, UObject* Object, AGolaemCache* GolaemCache, UMovieSceneTrack* Track)
{
    FKeyPropertyResult KeyPropertyResult;

    FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object);
    FGuid ObjectHandle = HandleResult.Handle;
    KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
    if (ObjectHandle.IsValid())
    {
        if (!Track)
        {
            // changed behavior compared to before : allow several tracks on same object
            UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
            Track = AddTrack(MovieScene, ObjectHandle, UMovieSceneGolaemAnimationTrack::StaticClass(), NAME_None);
            if (Track != nullptr)
            {
                if (ensure(Track))
                {
                    Track->Modify();
                    UMovieSceneGolaemAnimationTrack* GolaemTrack = Cast<UMovieSceneGolaemAnimationTrack>(Track);
                    //GolaemTrack->GolaemCacheName = ObjectTools::SanitizeObjectName(GolaemCache->GetName()).ToString();
                    GolaemTrack->TrackGolaemCache = GolaemCache;

                    UMovieSceneSection* NewSection = GolaemTrack->AddNewGolaemCacheSectionOnRow(KeyTime);
                    KeyPropertyResult.bTrackModified = true;

                    GetSequencer()->EmptySelection();
                    GetSequencer()->SelectSection(NewSection);
                    GetSequencer()->ThrobSectionSelection();
                    GetSequencer()->RefreshTree();
                }
            }
            else
            {
                GLM_CROWD_TRACE_WARNING("Golaem cache track creation failed for Golaem Cache "<< TCHAR_TO_ANSI(*GolaemCache->GetName()));
            }
        }
    }
}

void AddASectionToTrack(TSharedRef<ISequencer> Sequencer, AGolaemCache* GolaemCache, UMovieSceneGolaemAnimationTrack* golaemAnimationTrack)
{
}

TSharedPtr<SWidget> FGolaemTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
    return TSharedPtr<SWidget>();
}

#undef LOCTEXT_NAMESPACE
