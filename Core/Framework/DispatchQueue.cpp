#include "DispatchQueue.h"
#ifdef _WIN32
#include <Windows.h>
#endif
#if defined(__APPLE__) && defined(__MACH__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#if defined(__linux__)
#include <time.h>
#include <stdlib.h>
#include <dirent.h>
#include <fnmatch.h>
#include <iostream>
#include <fstream>
#endif

namespace {
    uint32_t numberOfCPUCores() {
#ifdef _WIN32
		static int ncpu = []()->int {
			SYSTEM_INFO sysInfo;
			GetSystemInfo(&sysInfo);
			DWORD numLogicalProcessors = sysInfo.dwNumberOfProcessors;
			DWORD numPhysicalProcessors = numLogicalProcessors;

			DWORD buffSize = 0;
			if (!GetLogicalProcessorInformationEx(RelationProcessorCore, 0, &buffSize) &&
				GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
				uint8_t* buffer = new uint8_t[buffSize];

				if (GetLogicalProcessorInformationEx(RelationProcessorCore, (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)buffer, &buffSize)) {
					DWORD numCores = 0;
					DWORD offset = 0;
					do {
						SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* processorInfo = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)&buffer[offset];
						offset += processorInfo->Size;
						numCores += processorInfo->Processor.GroupCount;
					} while (offset < buffSize);
					if (numCores > 0 && numCores < numLogicalProcessors) {
						numPhysicalProcessors = numCores;
					}
				}
				delete[] buffer;
			}
			return numPhysicalProcessors;
		}();
#endif
#if defined(__APPLE__) && defined(__MACH__)
		static int ncpu = []() {
			int num = 0;
			size_t len = sizeof(num);
			if (!sysctlbyname("hw.physicalcpu", &num, &len, nullptr, 0))
				return num;
			if (!sysctlbyname("hw.physicalcpu_max", &num, &len, nullptr, 0))
				return num;
			return 1;
		}();
#endif
#if defined(__linux__)
		static int ncpu = []()->int {
			int count = 0;
			DIR* cpuDir = opendir("/sys/devices/system/cpu"); // linux only
			if (cpuDir) {
				const struct dirent* dirEntry;
				while ((dirEntry = readdir(cpuDir))) {
					if (fnmatch("cpu[0-9]*", dirEntry->d_name, 0) == 0)
						count++;
				}
				closedir(cpuDir);
				return count;
			}
			return -1;
		}();
#endif
		if (ncpu > 1)
			return ncpu;
		return 1;
    }

    uint32_t numberOfProcessors() {
#ifdef _WIN32
		DWORD ncpu = GetMaximumProcessorCount(ALL_PROCESSOR_GROUPS);
		if (!ncpu) {
			SYSTEM_INFO sysinfo;
			GetSystemInfo(&sysinfo);
			ncpu = sysinfo.dwNumberOfProcessors;
		}
#endif
#if defined(__APPLE__) && defined(__MACH__)
		static int ncpu = []() {
			int num = 0;
			int mib[2] = { CTL_HW, HW_AVAILCPU };
			size_t len = sizeof(num);
			if (!sysctl(mib, 2, &num, &len, NULL, 0))
				return num;
			mib[1] = HW_NCPU;
			if (!sysctl(mib, 2, &num, &len, NULL, 0))
				return num;
			if (!sysctlbyname("hw.logicalcpu", &num, &len, nullptr, 0))
				return num;
			if (!sysctlbyname("hw.logicalcpu_max", &num, &len, nullptr, 0))
				return num;
			return 1;
		}();
#endif
#if defined(__linux__)
		static int ncpu = sysconf(_SC_NPROCESSORS_CONF);
#endif
		if (ncpu > 1)
			return ncpu;
		return 1;
    }

	std::thread::id mainThreadID = {};
}

void setDispatchQueueMainThread() {
	mainThreadID = std::this_thread::get_id();
}

using namespace FV;

DispatchQueue::DispatchQueue(_DispatchQueueMain)
	: maxConcurrentQueues(1) {
}

DispatchQueue::DispatchQueue(uint32_t q)
    : maxConcurrentQueues(q) {

	auto threadProc = [this](std::stop_token token) {
		std::unique_lock lock(mutex);
		while (token.stop_requested() == false) {
			if (this->dispatch(lock) == 0) {
				cond.wait(lock);
			}
		}
	};

	for (uint32_t i = 0; i < q; ++i) {
		auto t = std::jthread(threadProc);
		threads.push_back(std::move(t));
	}
}

DispatchQueue::~DispatchQueue() {
	std::unique_lock lock(mutex);
	if (threads.empty() == false) {
		for (auto& t : threads)
			t.request_stop();
		for (auto& cmd : commands)
			if (cmd->state != StateCompleted)
				cmd->state = StateCancelled;
		cond.notify_all();
		lock.unlock();

		for (auto& t : threads)
			t.join();
	}
}

DispatchQueue& DispatchQueue::main() {
	static DispatchQueue queue(_DispatchQueueMain{});
    return queue;
}

DispatchQueue& DispatchQueue::global() {
    static DispatchQueue queue(std::max(numberOfCPUCores(),2U));
    return queue;
}

void DispatchQueue::async(Task fn) {
	std::unique_lock lock(mutex);
	std::shared_ptr<Command> cmd = std::make_shared<Command>(fn, StatePending);
	commands.push_back(cmd);
	notifyHook(lock, fn);
	cond.notify_all();
}

void DispatchQueue::await(Task fn) {
	std::unique_lock lock(mutex);
	std::shared_ptr<Command> cmd = std::make_shared<Command>(fn, StatePending);
	commands.push_back(cmd);
	auto state = cmd->state;

	DispatchQueue* localQueue = nullptr;
	if (isWorkerThread(lock))
		localQueue = this;
	else {
		// must be a persistent queue (i.e., main or global)
		if (auto* q = &main(); this != q && q->isWorkerThread())
			localQueue = q;
		else if (auto* q = &global(); this != q && q->isWorkerThread())
			localQueue = q;
	}
	notifyHook(lock, fn);
	cond.notify_all();

    while (state != StateCompleted && state != StateCancelled) {
		if (localQueue == this) {
			dispatch(lock);
		} else if (localQueue) {
			lock.unlock();
			localQueue->dispatch();
			if (lock.try_lock() == false) // Our queue is busy. Try next time.
				continue;
		} else {
		    cond.wait(lock);
		}
        state = cmd->state;
    }
}

void DispatchQueue::notifyHook(std::unique_lock<std::mutex>& lock, Task& t) const {
	FVASSERT_DEBUG(lock.mutex() == &this->mutex);

	std::vector<Hook> hooksCopy = hooks;
	lock.unlock();

	for (auto& hook : hooksCopy) {
		hook.notify(t);
	}
	lock.lock();
}

bool DispatchQueue::isWorkerThread() const {
	std::unique_lock lock(mutex);
	return isWorkerThread(lock);
}

bool DispatchQueue::isWorkerThread(std::unique_lock<std::mutex>& lock) const {
	FVASSERT_DEBUG(lock.mutex() == &this->mutex);

	auto current = std::this_thread::get_id();
	if (threads.empty()) {
		return current == mainThreadID;
	}

	for (auto& t : threads)
		if (current == t.get_id())
			return true;
	return false;
}

void DispatchQueue::yield() {
	dispatch();
}

uint32_t DispatchQueue::dispatch() {
	std::unique_lock lock(mutex);
	return dispatch(lock);
}

uint32_t DispatchQueue::dispatch(std::unique_lock<std::mutex>& lock) {
	FVASSERT_DEBUG(lock.mutex() == &this->mutex);
	while (commands.empty() == false) {
		std::shared_ptr<Command> cmd = commands.front();
		commands.erase(commands.begin());
		if (cmd->state == StatePending) {
			cmd->state = StateProcessing;
			lock.unlock();

			cmd->op();

			lock.lock();
			cmd->state = StateCompleted;
			cond.notify_all();
			return 1;
		}
	}
	return 0;
}

void DispatchQueue::setHook(const void* key, std::function<void(Task&)> fn) {
	std::unique_lock lock(mutex);
	for (auto& hook : hooks) {
		if (hook.key == key) {
			hook.notify = fn;
			return;
		}
	}
	Hook hook = { key, fn };
	hooks.push_back(hook);
}

void DispatchQueue::unsetHook(const void* key) {
	std::unique_lock lock(mutex);
	if (auto it = std::find_if(hooks.begin(), hooks.end(),
								[key](auto it) {
									return it.key == key;
								}); it != hooks.end()) {
		hooks.erase(it);
	}
}
