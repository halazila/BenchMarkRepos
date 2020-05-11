#pragma once
#ifdef _MSC_VER
#include <Windows.h>
#endif // _MSC_VER

#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <shared_mutex>
#include <qmutex.h>

namespace mutexBenchMark {

	const int g_cRepeatCount = 1000000;
	const int g_cThreadCount = 16;
	double g_shmem = 8;

#ifdef _MSC_VER
	CRITICAL_SECTION g_critSec;
#endif // _MSC_VER
	std::mutex g_mutex;
	std::shared_mutex g_sharedMutex;
	QMutex g_qMutex;

	void sharedFunc(int i)
	{
		if (i % 2 == 0)
			g_shmem = sqrt(g_shmem);
		else
			g_shmem *= g_shmem;
	}

	void threadFuncNoLock() {
		for (int i = 0; i < g_cRepeatCount; ++i) {
			sharedFunc(i);
		}
	}

#ifdef _MSC_VER
	void threadFuncCritSec() {
		for (int i = 0; i < g_cRepeatCount; ++i) {
			EnterCriticalSection(&g_critSec);
			sharedFunc(i);
			LeaveCriticalSection(&g_critSec);
		}
	}
#endif

	void threadFuncMutex() {
		for (int i = 0; i < g_cRepeatCount; ++i) {
			std::lock_guard<std::mutex> lock(g_mutex);
			sharedFunc(i);
		}
	}

	void threadFuncSharedMutex() {
		for (int i = 0; i < g_cRepeatCount; ++i) {
			std::unique_lock<std::shared_mutex> lock(g_sharedMutex);
			sharedFunc(i);
		}
	}

	void threadFuncSharedMutexShareLock() {
		for (int i = 0; i < g_cRepeatCount; ++i) {
			///writing is not safe
			std::shared_lock<std::shared_mutex> lock(g_sharedMutex);
			sharedFunc(i);
		}
	}

	void threadFuncQtMutex() {
		for (int i = 0; i < g_cRepeatCount; ++i) {
			QMutexLocker lock(&g_qMutex);
			sharedFunc(i);
		}
	}

	void testRound(int threadCount)
	{
		std::vector<std::thread> threads;
		///Critical Section
#ifdef _MSC_VER
		threads.clear();
		auto startCritSec = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < threadCount; ++i)
			threads.push_back(std::thread(threadFuncCritSec));
		for (std::thread& thd : threads)
			thd.join();
		auto endCritSec = std::chrono::high_resolution_clock::now();
		std::cout << "CRITICAL_SECTION:              ";
		std::cout << std::chrono::duration_cast<std::chrono::microseconds>(endCritSec - startCritSec).count();
		std::cout << " us \n\r";
#endif
		///std::mutex
		threads.clear();
		auto startMutex = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < threadCount; ++i)
			threads.push_back(std::thread(threadFuncMutex));
		for (std::thread& thd : threads)
			thd.join();
		auto endMutex = std::chrono::high_resolution_clock::now();
		std::cout << "std::mutex:                    ";
		std::cout << std::chrono::duration_cast<std::chrono::microseconds>(endMutex - startMutex).count();
		std::cout << " us \n\r";

		///std::shared_mutex
		threads.clear();
		auto startSharedMutex = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < threadCount; ++i)
			threads.push_back(std::thread(threadFuncSharedMutex));
		for (std::thread& thd : threads)
			thd.join();
		auto endSharedMutex = std::chrono::high_resolution_clock::now();
		std::cout << "std::shared_mutex unique_lock: ";
		std::cout << std::chrono::duration_cast<std::chrono::microseconds>(endSharedMutex - startSharedMutex).count();
		std::cout << " us \n\r";

		/////std::shared_mutex shared_lock
		//threads.clear();
		//auto startSharedMutexSharedLock = std::chrono::high_resolution_clock::now();
		//for (int i = 0; i < threadCount; ++i)
		//	threads.push_back(std::thread(threadFuncSharedMutexShareLock));
		//for (std::thread& thd : threads)
		//	thd.join();
		//auto endSharedMutexSharedLock = std::chrono::high_resolution_clock::now();
		//std::cout << "std::shared_mutex shared_lock: ";
		//std::cout << std::chrono::duration_cast<std::chrono::microseconds>(endSharedMutexSharedLock - startSharedMutexSharedLock).count();
		//std::cout << " us \n\r";

		///QMutex
		threads.clear();
		auto startQtMutex = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < threadCount; ++i)
			threads.push_back(std::thread(threadFuncQtMutex));
		for (std::thread& thd : threads)
			thd.join();
		auto endQtMutex = std::chrono::high_resolution_clock::now();
		std::cout << "Qt Mutex:                      ";
		std::cout << std::chrono::duration_cast<std::chrono::microseconds>(endQtMutex - startQtMutex).count();
		std::cout << " us \n\r";

	}

	int runBenchMark() {
#ifdef _MSC_VER
		InitializeCriticalSection(&g_critSec);
#endif
		std::cout << "Iterations: " << g_cRepeatCount << "\n\r";

		auto startNoLock = std::chrono::high_resolution_clock::now();
		threadFuncNoLock();
		auto endNoLock = std::chrono::high_resolution_clock::now();
		std::cout << "No Lock:                       ";
		std::cout << std::chrono::duration_cast<std::chrono::microseconds>(endNoLock - startNoLock).count();
		std::cout << " us \n\r";

		for (int i = 1; i <= g_cThreadCount; i *= 2) {
			std::cout << "Thread count: " << i << "\n\r";
			testRound(i);
			if (i == 4) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				std::cout << "Thread count: " << i + 2 << "\n\r";
				testRound(i + 2);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}

#ifdef _MSC_VER
		DeleteCriticalSection(&g_critSec);
#endif
		// try to prevent the compiler to completely
		// optimize out the code around g_shmem if it wouldn't be used anywhere.
		std::cout << "Shared variable value: " << g_shmem << std::endl;
		return 0;
	}
}