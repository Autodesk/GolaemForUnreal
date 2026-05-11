/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

TSharedPtr< FSlateStyleSet > FGolaemStyle::StyleSet = NULL;
TSharedPtr< class ISlateStyle > FGolaemStyle::Get() { return StyleSet; }

//-----------------------------------------------------------------------------
void FGolaemStyle::Initialize()
{
	// Const icon sizes
	const FVector2D Icon16x16(16.0f, 16.0f);
    const FVector2D Icon20x20(20.0f, 20.0f);
    const FVector2D Icon40x40(40.0f, 40.0f);
    const FVector2D Icon64x64(64.0f, 64.0f);
	const FVector2D Icon128x128(128.0f, 128.0f);

	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	FString ContentDir = IPluginManager::Get().FindPlugin("GolaemPlugin")->GetBaseDir();
	StyleSet->SetContentRoot(ContentDir / TEXT("Resources/Icons"));

	// Golaem Icons
	{
		StyleSet->Set("Golaem.Icon16", new IMAGE_BRUSH("Icon16", Icon16x16));
		StyleSet->Set("Golaem.Icon40", new IMAGE_BRUSH("Icon64", Icon40x40));
		StyleSet->Set("Golaem.Icon64", new IMAGE_BRUSH("Icon64", Icon64x64));

		StyleSet->Set("GolaemEngine.Icon16", new IMAGE_BRUSH("IconEngine16", Icon16x16));
		StyleSet->Set("GolaemEngine.Icon40", new IMAGE_BRUSH("IconEngine64", Icon40x40));
		StyleSet->Set("GolaemEngine.Icon64", new IMAGE_BRUSH("IconEngine64", Icon64x64));

        StyleSet->Set("Golaem.Editors.RefreshIcon", new IMAGE_BRUSH("editors/refresh", Icon20x20));

        //was sharing a pointer for each icon size, but it crashes at exit, probably deleting both pointers.
        StyleSet->Set("ClassThumbnail.GolaemCache", new IMAGE_BRUSH("Icon64", Icon64x64));
        StyleSet->Set("ClassThumbnail.GolaemCharacter", new IMAGE_BRUSH("Icon64", Icon64x64));
        StyleSet->Set("ClassThumbnail.GolaemCharacterInstanced", new IMAGE_BRUSH("Icon64", Icon64x64));
		StyleSet->Set("ClassThumbnail.GolaemSimulation", new IMAGE_BRUSH("IconEngine64", Icon64x64));

		StyleSet->Set("ClassIcon.GolaemCache", new IMAGE_BRUSH("Icon16", Icon16x16));
		StyleSet->Set("ClassIcon.GolaemCharacter", new IMAGE_BRUSH("Icon16", Icon16x16));
		StyleSet->Set("ClassIcon.GolaemCharacterInstanced", new IMAGE_BRUSH("Icon16", Icon16x16));
		StyleSet->Set("ClassIcon.GolaemSimulation", new IMAGE_BRUSH("IconEngine16", Icon16x16));
	}

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH

//-----------------------------------------------------------------------------
void FGolaemStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

//-----------------------------------------------------------------------------
FName FGolaemStyle::GetStyleSetName()
{
	static FName GolaemStyleName(TEXT("GolaemUIStyle"));
	return GolaemStyleName;
}

