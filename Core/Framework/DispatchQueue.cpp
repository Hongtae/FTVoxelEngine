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
}

using namespace FV;

namespace {
    // cond, mutex for AsyncTask client waiting, thread registration locking
    std::condition_variable globalDispatchCond;
    std::mutex globalDispatchMutex;

    struct ThreadDispatcher {
        std::thread::id threadID;
        std::weak_ptr<DispatchQueue::_Dispatcher> dispatcher;
    };
    std::thread::id mainThreadID = {};
    std::vector<ThreadDispatcher> threadDispatcher;
    std::weak_ptr<DispatchQueue::_Dispatcher> mainDispatcher;

    void setThreadDispatcher(std::shared_ptr<DispatchQueue::_Dispatcher> dispatcher) {
        std::thread::id threadID = std::this_thread::get_id();
        std::unique_lock lock(globalDispatchMutex);
        if (auto it = std::find_if(
            threadDispatcher.begin(),
            threadDispatcher.end(),
            [threadID](auto&& it) {
                return it.threadID == threadID;
            }); it != threadDispatcher.end()) {
            if (dispatcher)
                it->dispatcher = dispatcher;
            else
                threadDispatcher.erase(it);
        } else {
                if (dispatcher) {
                    threadDispatcher.push_back(ThreadDispatcher{ threadID, dispatcher });
                }
            }
            globalDispatchCond.notify_all();
    }

    std::shared_ptr<DispatchQueue::_Dispatcher> getLocalDispatcher(std::unique_lock<std::mutex>& lock) {
        FVASSERT_DEBUG(lock.mutex() == &globalDispatchMutex);
        FVASSERT_DEBUG(lock.owns_lock());

        std::thread::id threadID = std::this_thread::get_id();
        if (threadID == mainThreadID) {
            if (auto dp = mainDispatcher.lock())
                return dp;
        }
        if (auto it = std::find_if(
            threadDispatcher.begin(),
            threadDispatcher.end(),
            [threadID](auto&& it) {
                return it.threadID == threadID;
            }); it != threadDispatcher.end()) {
            if (auto dp = it->dispatcher.lock())
                return dp;
        }
        return nullptr;
    }

    std::shared_ptr<DispatchQueue::_Dispatcher> getLocalDispatcher() {
        std::unique_lock lock(globalDispatchMutex);
    }

    class AsyncTaskImpl : public AsyncTask {
    public:
        AsyncTaskImpl(std::function<void()>& task, State st)
            : _op(task), _state(st) {
        }
        std::function<void()> _op;
        State _state;

        State state() const override {
            std::unique_lock lock(globalDispatchMutex);
            return _state;
        }
        void setState(State st) {
            std::unique_lock lock(globalDispatchMutex);
            _state = st;
        }
        bool wait() const override {
            std::unique_lock lock(globalDispatchMutex);
            auto st = _state;
            while (st != StateCompleted && st != StateCancelled) {
                if (auto dp = getLocalDispatcher(lock); dp) {
                    lock.unlock();
                    dp->dispatch();
                    lock.lock();
                } else {
                    globalDispatchCond.wait(lock);
                }
                st = _state;
            }
            return st == StateCompleted;
        }
    };

    class Dispatcher : public FV::DispatchQueue::_Dispatcher {
    public:
        std::vector<std::shared_ptr<AsyncTaskImpl>> tasks;
        // cond, mutex for enqueued tasks
        std::condition_variable cond;
        std::mutex mutex;

        std::shared_ptr<AsyncTask> enqueue(DispatchQueue::Task& t) override {
            auto task = std::make_shared<AsyncTaskImpl>(t, AsyncTask::StatePending);
            std::unique_lock lock(mutex);
            tasks.push_back(task);
            cond.notify_all();
            return task;
        }

        uint32_t dispatch() override {
            std::shared_ptr<AsyncTaskImpl> task;
            std::unique_lock lock(mutex);
            if (tasks.empty() == false) {
                task = tasks.front();
                tasks.erase(tasks.begin());
            }
            if (task && task->state() == AsyncTask::StatePending) {
                task->setState(AsyncTask::StateProcessing);

                if (task->_op) {
                    lock.unlock();
                    task->_op();
                    lock.lock();
                }
                task->_op = nullptr;
                task->setState(AsyncTask::StateCompleted);
                globalDispatchCond.notify_all();
                return 1;
            }
            return 0;
        }
        void cancelAllTasks() override {
            std::vector<DispatchQueue::Task> ops;
            std::unique_lock lock(mutex);
            for (auto& task : tasks) {
                if (task->_state == AsyncTask::StatePending) {
                    task->_state = AsyncTask::StateCancelled;
                    // temporary save to avoid immediate destruction
                    ops.push_back(task->_op);
                    task->_op = nullptr;
                }
            }
            lock.unlock();
            globalDispatchCond.notify_all();
            // tasks will be destroyed afterwards
        }
        void wait() override {
            std::unique_lock lock(mutex);
            cond.wait(lock);
        }
        void notify() override {
            cond.notify_all();
        }
    };
}

void setDispatchQueueMainThread() {
    std::unique_lock lock(globalDispatchMutex);
    mainThreadID = std::this_thread::get_id();
}

DispatchQueue::DispatchQueue(_DispatchQueueMain)
    : maxConcurrentQueues(1) {
    this->dispatcher = std::make_shared<Dispatcher>();
    mainDispatcher = this->dispatcher;
}

DispatchQueue::DispatchQueue(uint32_t q)
    : maxConcurrentQueues(q) {

    this->dispatcher = std::make_shared<Dispatcher>();
    auto threadProc = [this](std::stop_token token) {
        setThreadDispatcher(this->dispatcher);
        while (token.stop_requested() == false) {
            if (dispatcher->dispatch() == 0) {
                dispatcher->wait();
            }
        }
        setThreadDispatcher(nullptr);
    };

    for (uint32_t i = 0; i < q; ++i) {
        auto t = std::jthread(threadProc);
        threads.push_back(std::move(t));
    }
}

DispatchQueue::~DispatchQueue() {

    dispatcher->cancelAllTasks();

    if (threads.empty() == false) {
        for (auto& t : threads)
            t.request_stop();
        dispatcher->notify();
        for (auto& t : threads)
            t.join();
    }
}

DispatchQueue& DispatchQueue::main() {
    static DispatchQueue queue(_DispatchQueueMain{});
    return queue;
}

DispatchQueue& DispatchQueue::global() {
    static DispatchQueue queue(std::max(numberOfCPUCores() - 1, 2U));
    return queue;
}

std::shared_ptr<AsyncTask> DispatchQueue::async(Task fn) {
    auto task = dispatcher->enqueue(fn);
    notifyHook(fn);
    return task;
}

void DispatchQueue::yield() {
    dispatcher->dispatch();
}

uint32_t DispatchQueue::dispatch() {
    return dispatcher->dispatch();
}

void DispatchQueue::setHook(const void* key, std::function<void(Task&)> fn) {
    std::unique_lock lock(mutex);
    if (auto it = std::find_if(
        hooks.begin(), hooks.end(), [key](auto&& it) { return it.key == key; });
        it != hooks.end()) {
        if (fn) {
            it->notify = fn;
        } else {
            hooks.erase(it);
        }
    } else {
        if (fn)
            hooks.push_back({ key, fn });
    }
}

void DispatchQueue::unsetHook(const void* key) {
    setHook(key, nullptr);
}

void DispatchQueue::notifyHook(Task& t) const {
    std::unique_lock lock(mutex);
    auto hooksCopy = hooks;
    lock.unlock();
    for (auto& h : hooksCopy) {
        h.notify(t);
    }
}
