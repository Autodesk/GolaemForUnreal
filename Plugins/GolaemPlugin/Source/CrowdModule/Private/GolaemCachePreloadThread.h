/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/
//
//#pragma once
//
//#include "CoreMinimal.h"
//#include "HAL/Runnable.h"
//
//#pragma once
//
//#include "HAL/Runnable.h"
//
//class AGolaemCache;
//
//// Note that we do not have to mark our class as UCLASS() if we don't want to
//class FCachePreloadWorker : public FRunnable
//{
//public:
//	// Custom constructor for setting up our thread with its target
//	FCachePreloadWorker(AGolaemCache* ManagedCache);
//	virtual ~FCachePreloadWorker();
//
//	// FRunnable functions
//	virtual uint32 Run() override;
//	virtual void Stop() override;
//	virtual void Exit() override;
//	// FRunnable
//
//	// by essence, this task will run forever 
//	//bool IsComplete() const;
//
//protected:
//
//	AGolaemCache* ManagedCache = NULL;
//	bool bStopThread = false;
//
//	FRunnableThread* Thread;// = FRunnableThread::Create(this, TEXT("RNGThread"), 0, TPri_BelowNormal);
//};
