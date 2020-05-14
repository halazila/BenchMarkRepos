#pragma once
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <shared_mutex>
#include <atomic>

namespace atomicBenchmark {

	int pureInt = 0;
	std::atomic<int> atomicInt = 0;
	int mutexInt = 0;
	int g_repeatTimes = 100000000;
	std::mutex mtx;

	void purelyIncreFunc() {
		for (int i = 0; i < g_repeatTimes; i++) {
			pureInt++;
		}
		std::cout << "number: " << pureInt << std::endl;
	}

	void atomicIncreFunc() {
		for (int i = 0; i < g_repeatTimes; i++) {
			atomicInt++;
		}
		std::cout << "number: " << atomicInt << std::endl;
	}

	void mutexIncreFunc() {
		for (int i = 0; i < g_repeatTimes; i++) {
			std::lock_guard<decltype(mtx)> lock(mtx);
			mutexInt++;
		}
		std::cout << "number: " << mutexInt << std::endl;
	}

	int runBenchMark() {
		std::chrono::high_resolution_clock::time_point startTime;
		std::chrono::high_resolution_clock::time_point endTime;
		std::cout << "repeat times: " << g_repeatTimes << std::endl;

		startTime = std::chrono::high_resolution_clock::now();
		purelyIncreFunc();
		endTime = std::chrono::high_resolution_clock::now();
		std::cout << "purely Int increment: " << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << " us" << std::endl;

		startTime = std::chrono::high_resolution_clock::now();
		atomicIncreFunc();
		endTime = std::chrono::high_resolution_clock::now();
		std::cout << "atomic Int increment: " << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << " us" << std::endl;

		startTime = std::chrono::high_resolution_clock::now();
		mutexIncreFunc();
		endTime = std::chrono::high_resolution_clock::now();
		std::cout << "mutexd Int increment: " << std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() << " us" << std::endl;

		return 0;
	}
}