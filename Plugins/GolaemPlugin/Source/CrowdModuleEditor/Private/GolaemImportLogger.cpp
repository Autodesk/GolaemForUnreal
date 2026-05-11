/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

//#include "GolaemImporterModulePCH.h"
#include "GolaemImportLogger.h"
#include "Logging/MessageLog.h"

TArray<TSharedRef<FTokenizedMessage>> FGolaemImportLogger::TokenizedErrorMessages;
FCriticalSection FGolaemImportLogger::MessageLock;

//-----------------------------------------------------------------------------
void FGolaemImportLogger::AddImportMessage(EMessageSeverity::Type severity, const FString& Message)
{
	TSharedRef<FTokenizedMessage> TokenMsg = FTokenizedMessage::Create(severity, FText::FromString(Message));
	
	MessageLock.Lock();
	TokenizedErrorMessages.Add(TokenMsg);
	MessageLock.Unlock();
}

//-----------------------------------------------------------------------------
void FGolaemImportLogger::OutputMessages(const FString& PageName)
{	
	static const FName LogName = "AssetTools";
	FMessageLog MessageLog(LogName);
	MessageLog.NewPage(FText::FromString(PageName));
	MessageLog.Open(EMessageSeverity::Info, true);
	
	MessageLock.Lock();	
	MessageLog.AddMessages(TokenizedErrorMessages);
	TokenizedErrorMessages.Empty();
	MessageLock.Unlock();
}
