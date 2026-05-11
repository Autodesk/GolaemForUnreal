/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/
//
////#include "MyProjectPCH.h"
//#include "GolaemCachePreloadThread.h" //needs to be included first
//
//#include "HAL/PlatformProcess.h"
//
//#include "GolaemCache.h"
//
//// ----------------------------------------------------------------------------------------
//FCachePreloadWorker::FCachePreloadWorker(AGolaemCache* ManagedCacheIn)
//	: Thread(nullptr)
//{
//	ManagedCache = ManagedCacheIn;
//	Thread = FRunnableThread::Create(this, TEXT("Golaem Cache Preload"), 0, TPri_AboveNormal);
//}
//
//// ----------------------------------------------------------------------------------------
//FCachePreloadWorker::~FCachePreloadWorker()
//{
//	if (Thread)
//	{
//		//Cleanup the worker thread
//		delete Thread;
//		Thread = nullptr;
//	}
//}
//
//// ----------------------------------------------------------------------------------------
//uint32 FCachePreloadWorker::Run()
//{
//	bStopThread = false;
//
//	// Keep processing until we're cancelled through Stop() or we're done,
//	// although this thread will suspended for other stuff to happen at the same time
//	int lastProcessedFrame = -1;
//	while (!bStopThread)
//	{
//		FPlatformProcess::Sleep(0.001f); // can sleep 1 ms, may still allows 1000fps 
//		if (lastProcessedFrame != ManagedCache->CurrentFrame)
//		{
//			// unlock previous frames
//			ManagedCache->releaseFinalFrameDataBefore(ManagedCache->CurrentFrame);
//
//			int loadingFrame = ManagedCache->CurrentFrame;
//
//			// ToDo : may add task for loading each frame here !!
//
//			// load loadingFrame if not already done, will be blocking, to avoid cache using a frame not completely loaded
//			//{
//			//	glm::ScopedLock<glm::SpinLock> lock(ManagedCache->CacheCurrentFrameLoadingLock);
//			ManagedCache->PreloadFrame(ManagedCache->CurrentFrame);
//			//}
//
//			// preload lastProcessedFrame + x frames
//			for (int i = 1; i <= ManagedCache->FramesToPreload; i++)
//			{
//				// we changed of frame, break to insure that the current frame is loaded before preload anything
//				if (loadingFrame != ManagedCache->CurrentFrame)
//					break;
//
//				ManagedCache->PreloadFrame(ManagedCache->CurrentFrame + i);
//			}
//
//			lastProcessedFrame = loadingFrame;
//		}
//	}
//
//	// Return success
//	return 0;
//}
//
//void FCachePreloadWorker::Exit()
//{
//	// Here's where we can do any cleanup we want to 
//}
//
//void FCachePreloadWorker::Stop()
//{
//	// Force our thread to stop early
//	bStopThread = true;
//
//	if (Thread)
//	{
//		Thread->WaitForCompletion();
//	}
//}