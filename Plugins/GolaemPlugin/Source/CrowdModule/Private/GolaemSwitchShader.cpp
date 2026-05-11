/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemSwitchShader.h" //needs to be included first

#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "GolaemSwitchShader"

UGolaemSwitchShader::UGolaemSwitchShader(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITOR
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UGolaemSwitchShader::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// default value
	int32 returnValue = Compiler->Constant3(1.f, 0.47f, 0.f);
	if (Default.Expression) returnValue = Default.Compile(Compiler);
		
	if (Selector.Expression)
	{
		int32 selectorIndex(Selector.Compile(Compiler));
		if (StartOffset.Expression) selectorIndex = Compiler->Sub(selectorIndex, StartOffset.Compile(Compiler));
		int32 threshold(Compiler->Constant(0.f));

		if (Shader0.Expression) returnValue = Compiler->If(selectorIndex, Compiler->Constant(0.f), returnValue, Shader0.Compile(Compiler), returnValue, threshold);
		if (Shader1.Expression) returnValue = Compiler->If(selectorIndex, Compiler->Constant(1.f), returnValue, Shader1.Compile(Compiler), returnValue, threshold);
		if (Shader2.Expression) returnValue = Compiler->If(selectorIndex, Compiler->Constant(2.f), returnValue, Shader2.Compile(Compiler), returnValue, threshold);
		if (Shader3.Expression) returnValue = Compiler->If(selectorIndex, Compiler->Constant(3.f), returnValue, Shader3.Compile(Compiler), returnValue, threshold);
		if (Shader4.Expression) returnValue = Compiler->If(selectorIndex, Compiler->Constant(4.f), returnValue, Shader4.Compile(Compiler), returnValue, threshold);
		if (Shader5.Expression) returnValue = Compiler->If(selectorIndex, Compiler->Constant(5.f), returnValue, Shader5.Compile(Compiler), returnValue, threshold);
		if (Shader6.Expression) returnValue = Compiler->If(selectorIndex, Compiler->Constant(6.f), returnValue, Shader6.Compile(Compiler), returnValue, threshold);
		if (Shader7.Expression) returnValue = Compiler->If(selectorIndex, Compiler->Constant(7.f), returnValue, Shader7.Compile(Compiler), returnValue, threshold);
		if (Shader8.Expression) returnValue = Compiler->If(selectorIndex, Compiler->Constant(8.f), returnValue, Shader8.Compile(Compiler), returnValue, threshold);
		if (Shader9.Expression) returnValue = Compiler->If(selectorIndex, Compiler->Constant(9.f), returnValue, Shader9.Compile(Compiler), returnValue, threshold);
	}
	else
	{
		return Compiler->Errorf(TEXT("Missing Selector input"));
	}

	return returnValue;

}

void UGolaemSwitchShader::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add("Switch");
	
}
#endif

#undef LOCTEXT_NAMESPACE //"GolaemSwitchShader"