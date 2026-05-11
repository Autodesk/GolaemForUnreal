/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

//#include "GolaemImporterModulePCH.h"
#include "GolaemImportOptions.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "GolaemImportSettings.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboBox.h"
#include "EditorStyleSet.h"

#include "GolaemUtils.h"

#define LOCTEXT_NAMESPACE "GolaemImportOptions"

//const TArray<FText> SGolaemImportOptions::MaterialImportLocations =
//{
//	LOCTEXT("Local", "Local"),
//	LOCTEXT("Under Parent", "Under Parent"),
//	LOCTEXT("Under Root", "Under Root"),
//	LOCTEXT("All Assets", "All Assets"),
//	LOCTEXT("Do Not Search", "Do Not Search"),
//};

const TArray<FString> SGolaemImportOptions::MaterialImportLocations =
{
	"Local",
	"Under Parent",
	"Under Root",
	"All Assets",
	"Do Not Search"
};


void SGolaemImportOptions::Construct(const FArguments& InArgs)
{
	ImportSettings = InArgs._ImportSettings;
	WidgetWindow = InArgs._WidgetWindow;
    ChosenScale = 100.f;
	bImportBlendshapes = true;

	check(ImportSettings);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(ImportSettings);
	
	//for (FGolaemPolyMesh* PolyMesh : InArgs._PolyMeshes)
	//{
	//	PolyMeshData.Add(FPolyMeshDataPtr(new FPolyMeshData(PolyMesh)));
	//}

	// retrieve the crowd unit to put it as default scale

	float crowdUnitFactor = GolaemUtils::getCrowdUnitFactor();

	this->ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
					.Text(LOCTEXT("Import_CurrentFileTitle", "Current File: "))
				]
				+ SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
					.Text(InArgs._FullPath)
				]
			]
		]

		//+ SVerticalBox::Slot()
		//.AutoHeight()
		//.Padding(2)
		//[
		//	SNew(SBorder)
		//	.Padding(FMargin(3))
		//	.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		//	[
		//		SNew(SBox)				
		//		.MinDesiredWidth(512.0f)
		//		.MaxDesiredHeight(350.0f)
		//		[
		//			SNew(SListView<FPolyMeshDataPtr>)
		//			.ItemHeight(24)						
		//			.ScrollbarVisibility(EVisibility::Visible)
		//			.ListItemsSource(&PolyMeshData)
		//			.OnMouseButtonDoubleClick(this, &SGolaemImportOptions::OnItemDoubleClicked)
		//			.OnGenerateRow(this, &SGolaemImportOptions::OnGenerateWidgetForList)
		//			.HeaderRow
		//			(
		//				SNew(SHeaderRow)

		//				+ SHeaderRow::Column("ShouldImport")
		//				.DefaultLabel(FText::FromString(TEXT("Include")))
		//				.FillWidth(0.1f)

		//				+ SHeaderRow::Column("TrackName")
		//				.DefaultLabel(LOCTEXT("TrackNameHeader", "Track Name"))
		//				.FillWidth(0.45f)
		//					
		//				+ SHeaderRow::Column("TrackFrameStart")
		//				.DefaultLabel(LOCTEXT("TrackFrameStartHeader", "Start Frame"))
		//				.FillWidth(0.15f)

		//				+ SHeaderRow::Column("TrackFrameEnd")
		//				.DefaultLabel(LOCTEXT("TrackFrameEndHeader", "End Frame"))
		//				.FillWidth(0.15f)

		//				+ SHeaderRow::Column("TrackFrameNum")
		//				.DefaultLabel(LOCTEXT("TrackFrameNumHeader", "Num Frames"))
		//				.FillWidth(0.15f)
		//			)
		//		]
		//	]
		//]
         + SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox) 
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
					.Text(LOCTEXT("Import_UniformScale", "Import Uniform Scale: "))
				] 
				+ SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SSpinBox<float>)
					.Value(crowdUnitFactor)
					.MaxValue(1000.f)
					.MinSliderValue(0.f)
					.MaxSliderValue(1000.f)
					.OnValueCommitted(this, &SGolaemImportOptions::StoreUniformScale)
					.MinDesiredWidth(50.f)
					.Delta(0.01f)
				]				
			]
		]

		 + SVerticalBox::Slot()
			 .AutoHeight()
			 .Padding(2)
			 [
				 SNew(SBorder)
				 .Padding(FMargin(3))
			 .BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			 [
				 SNew(SHorizontalBox)
				 + SHorizontalBox::Slot()
			 .AutoWidth()
			 [
				 SNew(STextBlock)
				 .Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
			 .Text(LOCTEXT("Import_Blendshapes", "Import Blendshapes: "))
			 ]
		 + SHorizontalBox::Slot()
			 .Padding(5, 0, 0, 0)
			 .AutoWidth()
			 .VAlign(VAlign_Center)
			 [
				 SNew(SCheckBox)
				 .IsChecked(this, &SGolaemImportOptions::IsBSCheckedState)
			 .OnCheckStateChanged(this, &SGolaemImportOptions::OnBSCheckStateChanged)
			 ]
			 ]
			 ]

		 + SVerticalBox::Slot()
			 .AutoHeight()
			 .Padding(2)
			 [
				 SNew(SBorder)
				 .Padding(FMargin(3))
			 .BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			 [
				 SNew(SHorizontalBox)
				 + SHorizontalBox::Slot()
			 .AutoWidth()
			 [
				 SNew(STextBlock)
				 .Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
			 .Text(LOCTEXT("Import_MaterialLocation", "Material Search location : "))
			 ]
		 + SHorizontalBox::Slot()
			 .Padding(5, 0, 0, 0)
			 .AutoWidth()
			 .VAlign(VAlign_Center)
			 [
				 SAssignNew(MaterialImportLocationComboBox, SComboBox<TSharedPtr<FString>>)
					//.InitiallySelectedItem(SelectedMaterialImportLocation)
					.OptionsSource(&MaterialImportLocationOptions)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewMaterialImportLocation, ESelectInfo::Type SelectInfo)
						 {
							SelectedMaterialImportLocation = NewMaterialImportLocation;
						 })
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
						 {
							//return SNew(STextBlock).Text(FText::FromString(*Option));
							 return SNew(STextBlock)
								 //.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
								 .Text(Option.IsValid() ? FText::FromString(*Option) : FText::GetEmpty());
						 })
					 [
						 SNew(STextBlock)
						 //.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
						 .Text_Lambda(
							 [&]()
							 {
								 TSharedPtr<FString> Selection = MaterialImportLocationComboBox->GetSelectedItem();

								 return FText::FromString(Selection.IsValid() ? *Selection : "");
							 }
						 )
					 ]
			 ]
			 ]
			 ]

		 //+ SVerticalBox::Slot()
			// .AutoHeight()
			// .Padding(2)
			// [
			//	 SNew(SBorder)
			//	 .Padding(FMargin(3))
			// .BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			// [
			//	 SNew(SHorizontalBox)
			//	 + SHorizontalBox::Slot()
			// .AutoWidth()
			// [
			//	 SNew(STextBlock)
			//	 .Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
			// .Text(LOCTEXT("Import_RigidMeshes", "Import as StaticMeshes : "))
			// ]
		 //+ SHorizontalBox::Slot()
			// .Padding(5, 0, 0, 0)
			// .AutoWidth()
			// .VAlign(VAlign_Center)
			// [
			// SAssignNew(StaticMeshCheckBox, SCheckBox)
			// .OnCheckStateChanged(this, &SGolaemImportOptions::StoreImportStaticMesh)
			// .ToolTipText(LOCTEXT("EnginePluginButtonToolTip", "Toggles whether this plugin will be created in the current project or the engine directory."))
			// .Content()
			//	[
			//		SNew(STextBlock)
			//		.Text(LOCTEXT("ImportStaticMeshCheckbox", "Import Static Meshes instead of a Skeletal mesh"))
			//	]
			// ]
			// ]
			// ]

		+ SVerticalBox::Slot()
		.Padding(2)
		.MaxHeight(500.0f)
		[
			DetailsView->AsShared()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(2)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(2)
			+ SUniformGridPanel::Slot(0, 0)
			[
				SAssignNew(ImportButton, SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("GolaemOptionWindow_Import", "Import"))
				.IsEnabled(this, &SGolaemImportOptions::CanImport)
				.OnClicked(this, &SGolaemImportOptions::OnImport)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("GolaemOptionWindow_Cancel", "Cancel"))
				.ToolTipText(LOCTEXT("GolaemOptionWindow_Cancel_ToolTip", "Cancels importing this Golaem file"))
				.OnClicked(this, &SGolaemImportOptions::OnCancel)
			]
		]
	];

	// init MaterialImportLocationComboBox
	{
		for (const auto& MaterialImportLocation : MaterialImportLocations)
		{
			MaterialImportLocationOptions.Add(MakeShareable(new FString(MaterialImportLocation)));
		}
		SelectedMaterialImportLocation = MaterialImportLocationOptions[0];
		MaterialImportLocationComboBox->RefreshOptions();
		MaterialImportLocationComboBox->SetSelectedItem(SelectedMaterialImportLocation);
	}
}

//TSharedRef<ITableRow> SGolaemImportOptions::OnGenerateWidgetForList(FPolyMeshDataPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
//{
//	return SNew(STrackSelectionTableRow, OwnerTable)
//		.PolyMesh(InItem);
//}

bool SGolaemImportOptions::CanImport()  const
{
	return true;
}

//void SGolaemImportOptions::OnItemDoubleClicked(FPolyMeshDataPtr ClickedItem)
//{
//	for (FPolyMeshDataPtr& Item : PolyMeshData)
//	{
//		Item->PolyMesh->bShouldImport = (Item == ClickedItem);
//	}
//}

void SGolaemImportOptions::StoreUniformScale(float InScale, ETextCommit::Type)
{
    ChosenScale = InScale;
}

float SGolaemImportOptions::GetScaleValue()
{
	return ChosenScale;
}

void SGolaemImportOptions::OnBSCheckStateChanged(ECheckBoxState InState)
{
	bImportBlendshapes = InState == ECheckBoxState::Checked;
}

ECheckBoxState SGolaemImportOptions::IsBSCheckedState() const
{
	return (bImportBlendshapes) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool SGolaemImportOptions::IsBSChecked() const
{
	return bImportBlendshapes;
}

EGolaemMaterialImportLocation::Value SGolaemImportOptions::getMaterialImportLocation() const
{
	EGolaemMaterialImportLocation::Value SelectedMaterialImportLocationEnum = EGolaemMaterialImportLocation::LOCAL; // default
	int32 indexOfSelected = MaterialImportLocationOptions.IndexOfByKey(SelectedMaterialImportLocation);
	if (indexOfSelected != INDEX_NONE)
	{
		SelectedMaterialImportLocationEnum = static_cast<EGolaemMaterialImportLocation::Value>(indexOfSelected);
	}
	return SelectedMaterialImportLocationEnum;
}

//void SGolaemImportOptions::StoreImportStaticMesh(ECheckBoxState staticMesh)
//{
//	ImportStaticMesh = StaticMeshCheckBox->IsChecked();
//}
//
//bool SGolaemImportOptions::GetImportStaticMeshValue() const
//{
//	return ImportStaticMesh;
//}

#undef LOCTEXT_NAMESPACE

