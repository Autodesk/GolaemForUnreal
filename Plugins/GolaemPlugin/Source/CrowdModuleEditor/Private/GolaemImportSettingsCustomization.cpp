/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

//#include "GolaemImporterModulePCH.h"
#include "GolaemImportSettingsCustomization.h"

#include "GolaemImportSettings.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyRestriction.h"

void FGolaemImportSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder)
{
	//FSimpleDelegate OnImportTypeChangedDelegate = FSimpleDelegate::CreateSP(this, &FGolaemImportSettingsCustomization::OnImportTypeChanged, &LayoutBuilder);
	//ImportType->SetOnPropertyValueChanged(OnImportTypeChangedDelegate);

	TArray<TWeakObjectPtr<UObject>> Objects;
	LayoutBuilder.GetObjectsBeingCustomized(Objects);
	
	TWeakObjectPtr<UObject>* SettingsObject = Objects.FindByPredicate([](const TWeakObjectPtr<UObject>& Object) { return Object->IsA<UGolaemImportSettings>(); } );

	UGolaemImportSettings* CurrentSettings = SettingsObject ? Cast<UGolaemImportSettings>(SettingsObject->Get()) : nullptr;

	//if (CurrentSettings && CurrentSettings->bReimport)
	//{
	//	//UEnum* ImportTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGolaemImportType"));		
	//	static FText RestrictReason = NSLOCTEXT("GolaemImportFactory", "ReimportRestriction", "Unable to change type while reimporting");
	//	TSharedPtr<FPropertyRestriction> EnumRestriction = MakeShareable(new FPropertyRestriction(RestrictReason));

	//	for (uint8 EnumIndex = 0; EnumIndex < (ImportTypeEnum->GetMaxEnumValue() + 1); ++EnumIndex)
	//	{
	//		if (EnumValue != EnumIndex)
	//		{
	//			EnumRestriction->AddDisabledValue(ImportTypeEnum->GetNameStringByIndex(EnumIndex));
	//		}
	//	}		
	//	ImportType->AddRestriction(EnumRestriction.ToSharedRef());
	//}	
}

TSharedRef<IDetailCustomization> FGolaemImportSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FGolaemImportSettingsCustomization());
}

void FGolaemImportSettingsCustomization::OnImportTypeChanged(IDetailLayoutBuilder* LayoutBuilder)
{
	LayoutBuilder->ForceRefreshDetails();
}

TSharedRef<IPropertyTypeCustomization> FGolaemConversionSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FGolaemConversionSettingsCustomization);
}

void FGolaemConversionSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FSimpleDelegate OnPresetChanged = FSimpleDelegate::CreateSP(this, &FGolaemConversionSettingsCustomization::OnConversionPresetChanged);
	FSimpleDelegate OnValueChanged = FSimpleDelegate::CreateSP(this, &FGolaemConversionSettingsCustomization::OnConversionValueChanged);

	TArray<void*> StructPtrs;
	StructPropertyHandle->AccessRawData(StructPtrs);
	if (StructPtrs.Num() == 1)
	{
		Settings = (FGolaemConversionSettings*)StructPtrs[0];
	}

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();

		if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FGolaemConversionSettings, Preset))
		{			
			ChildHandle->SetOnPropertyValueChanged(OnPresetChanged);
		}
		else
		{
			ChildHandle->SetOnPropertyValueChanged(OnValueChanged);
			ChildHandle->SetOnChildPropertyValueChanged(OnValueChanged);			
		}

		IDetailPropertyRow& Property = StructBuilder.AddProperty(ChildHandle);
	}
}

void FGolaemConversionSettingsCustomization::OnConversionPresetChanged()
{
	if (Settings)
	{
		// Set values to specified preset
		switch (Settings->Preset)
		{
			case EGolaemConversionPreset::Maya:
			{
				Settings->bFlipU = false;
				Settings->bFlipV = true;
				Settings->Scale = FVector(1.0f, -1.0f, 1.0f);
				Settings->Rotation = FVector::ZeroVector;
				break;
			}

			case EGolaemConversionPreset::Max:
			{
				Settings->bFlipU = false;
				Settings->bFlipV = true;
				Settings->Scale = FVector(1.0f, -1.0f, 1.0f);
				Settings->Rotation = FVector(90.0f, 0.0f, 0);
				break;
			}
		}
	}
}

void FGolaemConversionSettingsCustomization::OnConversionValueChanged()
{
	// Set conversion preset to custom
	Settings->Preset = EGolaemConversionPreset::Custom;
}
