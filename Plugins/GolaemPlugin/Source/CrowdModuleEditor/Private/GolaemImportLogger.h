/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"

class FGolaemImportLogger
{
protected:
	FGolaemImportLogger();
public:
	/** Adds an import message to the stored array for later output*/
	static void AddImportMessage(EMessageSeverity::Type severity, const FString& Message);
	/** Outputs the messages to a new named page in the message log */
	static void OutputMessages(const FString& PageName);
private:
	/** Error messages **/
	static TArray<TSharedRef<FTokenizedMessage>> TokenizedErrorMessages;
	static FCriticalSection MessageLock;
};

