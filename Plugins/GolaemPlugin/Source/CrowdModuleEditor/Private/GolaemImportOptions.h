/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "GolaemImporter.h"

// Forward declares
class UGolaemImportSettings;
class IDetailsView;

struct FPolyMeshData
{
    FPolyMeshData(class FGolaemPolyMesh* InPolyMesh)
        : PolyMesh(InPolyMesh)
    {
    }
    class FGolaemPolyMesh* PolyMesh;
};

typedef TSharedPtr<FPolyMeshData> FPolyMeshDataPtr;

class SGolaemImportOptions : public SCompoundWidget
{
public:

    SLATE_BEGIN_ARGS(SGolaemImportOptions)
        : _ImportSettings(nullptr)
        , _WidgetWindow()
    {
    }

    SLATE_ARGUMENT(UGolaemImportSettings*, ImportSettings)
    SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
    SLATE_ARGUMENT(FText, FullPath)
    //SLATE_ARGUMENT(TArray<class FGolaemPolyMesh*>, PolyMeshes)
    SLATE_END_ARGS()

public:
    void Construct(const FArguments& InArgs);
    virtual bool SupportsKeyboardFocus() const override
    {
        return true;
    }

    void StoreUniformScale(float InScale, ETextCommit::Type);

    float GetScaleValue();

	void OnBSCheckStateChanged(ECheckBoxState InState);
	ECheckBoxState IsBSCheckedState() const;
	bool IsBSChecked() const;
    EGolaemMaterialImportLocation::Value getMaterialImportLocation() const;

	//void StoreImportStaticMesh(ECheckBoxState staticMesh);
	//bool GetImportStaticMeshValue() const;

    FReply OnImport()
    {
        bShouldImport = true;
        if (WidgetWindow.IsValid())
        {
            WidgetWindow.Pin()->RequestDestroyWindow();
        }
        return FReply::Handled();
    }

    FReply OnCancel()
    {
        bShouldImport = false;
        if (WidgetWindow.IsValid())
        {
            WidgetWindow.Pin()->RequestDestroyWindow();
        }
        return FReply::Handled();
    }

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
    {
        if (InKeyEvent.GetKey() == EKeys::Escape)
        {
            return OnCancel();
        }

        return FReply::Unhandled();
    }

    bool ShouldImport() const
    {
        return bShouldImport;
    }

    SGolaemImportOptions()
        : ImportSettings(nullptr)
        , bShouldImport(false)
    {
    }

private:
    //TSharedRef<ITableRow> OnGenerateWidgetForList(FPolyMeshDataPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);
    bool CanImport() const;

    //void OnItemDoubleClicked(FPolyMeshDataPtr ClickedItem);
private:
    UGolaemImportSettings* ImportSettings;
    TWeakPtr<SWindow> WidgetWindow;
    TSharedPtr<SButton> ImportButton;
    //TSharedPtr<SSpinBox<float>> ScaleSpinBox;
	//TSharedPtr<SCheckBox> StaticMeshCheckBox;
    bool bShouldImport;
    TArray<FPolyMeshDataPtr> PolyMeshData;
    TSharedPtr<IDetailsView> DetailsView;
    float ChosenScale;
	bool bImportBlendshapes;

    TSharedPtr<SComboBox<TSharedPtr<FString>>> MaterialImportLocationComboBox;
    static const TArray<FString> MaterialImportLocations;
    TArray<TSharedPtr<FString>> MaterialImportLocationOptions;
    TSharedPtr<FString> SelectedMaterialImportLocation;

	//bool ImportStaticMesh;
};