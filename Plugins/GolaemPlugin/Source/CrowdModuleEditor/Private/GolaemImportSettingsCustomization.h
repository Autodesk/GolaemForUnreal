/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "PropertyCustomizationHelpers.h"

class UGolaemImportSettings;

class FGolaemImportSettingsCustomization : public IDetailCustomization
{
public:
	/** Begin IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder) override;
	/** End IDetailCustomization interface */
public:

	/** Creates a new instance. */
	static TSharedRef<IDetailCustomization> MakeInstance();
protected:
	void OnImportTypeChanged(IDetailLayoutBuilder* LayoutBuilder);

protected:
	UGolaemImportSettings* ImportSettings;
};

class FGolaemConversionSettingsCustomization
	: public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	FGolaemConversionSettingsCustomization() : Settings(nullptr) { }
	virtual ~FGolaemConversionSettingsCustomization() {}

	/** Begin IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	/** End IPropertyTypeCustomization interface */

protected:
	void OnConversionPresetChanged();
	void OnConversionValueChanged();

	struct FGolaemConversionSettings* Settings;
};