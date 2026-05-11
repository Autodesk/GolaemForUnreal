/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "Materials/MaterialExpression.h"

#include "GolaemSwitchShader.generated.h" //needs to be included last

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object)
class UGolaemSwitchShader : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Selector int */
	UPROPERTY()
	FExpressionInput Selector;

	/** StartOffset int */
	UPROPERTY()
	FExpressionInput StartOffset;

	/** Shader to evaluate if Selector out of bounds */
	UPROPERTY()
	FExpressionInput Default;

	/** Shader to evaluate at Selector 0 */
	UPROPERTY()
	FExpressionInput Shader0;

	/** Shader to evaluate at Selector 1 */
	UPROPERTY()
	FExpressionInput Shader1;

	/** Shader to evaluate at Selector 2 */
	UPROPERTY()
	FExpressionInput Shader2;

	/** Shader to evaluate at Selector 3 */
	UPROPERTY()
	FExpressionInput Shader3;

	/** Shader to evaluate at Selector 4 */
	UPROPERTY()
	FExpressionInput Shader4;

	/** Shader to evaluate at Selector 5 */
	UPROPERTY()
	FExpressionInput Shader5;

	/** Shader to evaluate at Selector 6 */
	UPROPERTY()
	FExpressionInput Shader6;

	/** Shader to evaluate at Selector 7 */
	UPROPERTY()
	FExpressionInput Shader7;

	/** Shader to evaluate at Selector 8 */
	UPROPERTY()
	FExpressionInput Shader8;

	/** Shader to evaluate at Selector 9 */
	UPROPERTY()
	FExpressionInput Shader9;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};
