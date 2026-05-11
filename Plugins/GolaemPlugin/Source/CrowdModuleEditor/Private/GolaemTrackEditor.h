/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"
#include "Runtime/Launch/Resources/Version.h"

struct FAssetData;
class FMenuBuilder;
class FSequencerSectionPainter;
class UMovieSceneGolaemSection;
class UMovieSceneSequence;
class USkeleton;
class AGolaemCache;

/**
 * Tools for animation tracks
 */
class FGolaemTrackEditor : public FMovieSceneTrackEditor
{
public:
	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FGolaemTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FGolaemTrackEditor() { }

	/**
	 * Creates an instance of this class.  Called by a sequencer
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	// ISequencerTrackEditor interface

    virtual void AddKey(const FGuid& ObjectGuid) /*override not anymore since 4.26*/;
    virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
    virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
    virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;

    virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
    virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
    //virtual void BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	//virtual bool OnAllowDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track, int32 RowIndex, const FGuid& TargetObjectGuid) override;
	//virtual FReply OnDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track, int32 RowIndex, const FGuid& TargetObjectGuid) override;

	void AddGolaemTrack(FFrameNumber KeyTime, UObject* Object, AGolaemCache* parentCache, UMovieSceneTrack* Track/*, int32 RowIndex*/);

private:

	/** Golaem Cache sub menu */
	TSharedRef<SWidget> BuildGolaemCacheSubMenu(FGuid ObjectBinding, AGolaemCache* GolaemCache, UMovieSceneTrack* Track);
	void AddGolaemCacheSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, AGolaemCache* GolaemCache, UMovieSceneTrack* Track);

	///** Animation sub menu filter function */
	//bool ShouldFilterAsset(const FAssetData& AssetData);

	///** Animation asset selected */
	//void OnAnimationAssetSelected(const FAssetData& AssetData, FGuid ObjectBinding, UMovieSceneTrack* Track);

	///** Animation asset enter pressed */
	//void OnAnimationAssetEnterPressed(const TArray<FAssetData>& AssetData, FGuid ObjectBinding, UMovieSceneTrack* Track);

	/** Delegate for AnimatablePropertyChanged in AddKey */
	//FKeyPropertyResult AddKeyInternal(FFrameNumber KeyTime, UObject* Object, class AGolaemCache* parentCache, UMovieSceneTrack* Track/*, int32 RowIndex*/);	
};


/** Class for Golaem Cache sections */
class FGolaemSection
	: public ISequencerSection
	, public TSharedFromThis<FGolaemSection>
{
public:

	/** Constructor. */
	FGolaemSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FGolaemSection() { }

public:

	// ISequencerSection interface

	virtual UMovieSceneSection* GetSectionObject() override;
	virtual FText GetSectionTitle() const override;
	virtual float GetSectionHeight() const override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;
	virtual void BeginResizeSection() override;
	virtual void ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime) override;
    virtual void BeginSlipSection() override;
    virtual void SlipSection(FFrameNumber SlipTime) override;

    virtual void BeginDilateSection() override;
    virtual void DilateSection(const TRange<FFrameNumber>& NewRange, float DilationFactor) override;

    virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding) override;

private:

	//bool CreatePoseAsset(const TArray<UObject*> NewAssets, FGuid InObjectBinding);
	//void HandleCreatePoseAsset(FGuid InObjectBinding);

private:

	/** The section we are visualizing */
	UMovieSceneGolaemSection& Section;

	/** Used to draw animation frame, need selection state and local time*/
	TWeakPtr<ISequencer> Sequencer;

	/** Cached start offset value valid only during resize */
	FFrameNumber InitialFirstLoopStartOffsetDuringResize;

	/** Cached start time valid only during resize */
	FFrameNumber InitialStartTimeDuringResize;
};
