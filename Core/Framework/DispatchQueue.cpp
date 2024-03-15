#include <deque>
#include <unordered_map>
#include <unordered_set>
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
    static bool _initializedLocalStorage;
    struct _local { // linkage local storage
        std::unordered_map<std::thread::id, std::weak_ptr<DispatchQueue::_Dispatcher>> dispatchers;
        std::weak_ptr<DispatchQueue::_Dispatcher> mainDispatcher;
        std::thread::id mainThreadID;
        std::mutex mutex;
        std::unordered_set<std::coroutine_handle<>> detachedCoroutines;
        std::unordered_map<std::thread::id, std::vector<std::function<void()>>> threadLocalDeferred;
        _local() { _initializedLocalStorage = true; }
        ~_local() { _initializedLocalStorage = false; }
        _local(_local&) = delete;
        _local(_local&&) = delete;
        static _local& get() {
            static _local local{};
            return local;
        }
    };

    class Dispatcher : public DispatchQueue::_Dispatcher {
    public:
        using Clock = std::chrono::steady_clock;
        struct Task {
            std::coroutine_handle<> coroutine;
            std::chrono::time_point<Clock> timepoint;
        };
        std::deque<Task> tasks;
        struct {
            std::mutex mutex;
            std::condition_variable cv;
        } cond;

        uint32_t dispatch() override {
            uint32_t fetch = 0;
            Task task{};
            do {
                auto lock = std::scoped_lock{ cond.mutex };
                if (tasks.empty() == false &&
                    tasks.front().timepoint <= Clock::now()) {
                    task = tasks.front();
                    tasks.pop_front();
                    fetch += 1;
                }
            } while (0);
            if (task.coroutine) {
                task.coroutine.resume();

                if (task.coroutine.done()) {
                    auto& local = _local::get();
                    auto lock = std::scoped_lock{ local.mutex };
                    if (local.detachedCoroutines.contains(task.coroutine)) {
                        local.detachedCoroutines.erase(task.coroutine);
                        task.coroutine.destroy();
                    }
                }
            }
            std::vector<std::function<void()>> deferred;
            do {
                auto& local = _local::get();
                auto lock = std::scoped_lock{ local.mutex };
                if (auto it = local.threadLocalDeferred.find(std::this_thread::get_id());
                    it != local.threadLocalDeferred.end()) {
                    it->second.swap(deferred);
                    it->second.clear();
                }
            } while (0);
            std::ranges::for_each(deferred, [](auto&& fn) { fn(); });
            return fetch;
        }

        void enqueue(std::coroutine_handle<> coro) override {
            auto tp = Clock::now();
            auto lock = std::unique_lock{ cond.mutex };
            auto pos = std::lower_bound(tasks.begin(), tasks.end(), tp,
                                        [](const auto& value, const auto& tp) {
                return value.timepoint < tp;
            });
            tasks.emplace(pos, coro, tp);
            cond.cv.notify_all();
        }

        void enqueue(std::coroutine_handle<> coro, double t) override {
            auto offset = std::chrono::duration<double>(std::max(t, 0.0));
            auto timepoint = Clock::now() + offset;
            auto tp = std::chrono::time_point_cast<Clock::duration>(timepoint);
            auto lock = std::unique_lock{ cond.mutex };
            auto pos = std::lower_bound(tasks.begin(), tasks.end(), tp,
                                        [](const auto& value, const auto& tp) {
                return value.timepoint < tp;
            });
            tasks.emplace(pos, coro, tp);
            cond.cv.notify_all();
        }

        void detach(std::coroutine_handle<> coro) override {
            if (coro) {
                enqueue(coro);
                auto& local = _local::get();
                auto lock = std::unique_lock{ local.mutex };
                local.detachedCoroutines.insert(coro);
            }
        }

        void wait() override {
            auto lock = std::unique_lock{ cond.mutex };
            if (tasks.empty() == false) {
                std::chrono::duration<double> interval = tasks.front().timepoint - Clock::now();
                if (interval.count() > 0)
                    cond.cv.wait_for(lock, interval);
            } else {
                cond.cv.wait(lock);
            }
        }

        bool wait(double timeout) override {
            auto offset = std::chrono::duration<double>(std::max(timeout, 0.0));
            auto tp = std::chrono::time_point_cast<Clock::duration>(Clock::now() + offset);
            auto lock = std::unique_lock{ cond.mutex };
            if (tasks.empty() == false && tasks.front().timepoint < tp) {
                std::chrono::duration<double> interval = tasks.front().timepoint - Clock::now();
                if (interval.count() > 0) {
                    cond.cv.wait_for(lock, interval);
                }
                return true;
            }
            return cond.cv.wait_until(lock, tp) == std::cv_status::no_timeout;
        }

        void notify() override {
            cond.cv.notify_all();
        }

        bool isMain() const override {
            return _local::get().mainDispatcher.lock().get() == this;
        }
    };

    void setThreadDispatcher(std::shared_ptr<DispatchQueue::_Dispatcher> dispatcher) {
        auto threadID = std::this_thread::get_id();
        auto& local = _local::get();
        auto lock = std::unique_lock{ local.mutex };
        if (dispatcher) {
            local.dispatchers.emplace(threadID, dispatcher);
        } else {
            if (_initializedLocalStorage) {
                local.dispatchers.erase(threadID);
                local.threadLocalDeferred.erase(threadID);
            } else {
                // app is begin terminated or unloading DLL. do nothing.
            }
        }
    }

    std::shared_ptr<DispatchQueue::_Dispatcher> getThreadDispatcher(std::thread::id threadID) {
        if (threadID != std::thread::id()) {
            auto& local = _local::get();
            auto lock = std::unique_lock{ local.mutex };

            if (threadID == local.mainThreadID) {
                return local.mainDispatcher.lock();
            }
            if (auto iter = local.dispatchers.find(threadID);
                iter != local.dispatchers.end()) {
                return iter->second.lock();
            }
        }
        return nullptr;
    }
}

namespace FV {
    void setDispatchQueueMainThread() {
        auto& local = _local::get();
        auto lock = std::unique_lock{ local.mutex };
        local.mainThreadID = std::this_thread::get_id();
        lock.unlock();
    }

    void _threadLocalDeferred(std::function<void()> fn) {
        if (fn) {
            auto& local = _local::get();
            auto lock = std::unique_lock{ local.mutex };
            local.threadLocalDeferred[std::this_thread::get_id()].push_back(fn);
        }
    }
}

DispatchQueue::DispatchQueue(_mainQueue) noexcept
    : _numThreads(1) {
    this->_dispatcher = std::make_shared<Dispatcher>();
    auto& local = _local::get();
    auto lock = std::unique_lock{ local.mutex };
    local.mainDispatcher = this->_dispatcher;
}

DispatchQueue::DispatchQueue(uint32_t maxThreads) noexcept
    : _numThreads(std::max(maxThreads, 1U)) {
    _dispatcher = std::make_shared<Dispatcher>();

    auto work = [this](std::stop_token token) {
        setThreadDispatcher(_dispatcher);
        while (token.stop_requested() == false) {
            if (_dispatcher->dispatch() == 0)
                _dispatcher->wait();
        }
        setThreadDispatcher(nullptr);
    };

    this->_threads.reserve(_numThreads);
    for (uint32_t i = 0; i < _numThreads; ++i)
        this->_threads.push_back(std::jthread(work));
}

DispatchQueue::DispatchQueue(DispatchQueue&& tmp) noexcept
    : _threads(std::move(tmp._threads))
    , _dispatcher(std::move(tmp._dispatcher))
    , _numThreads(tmp._numThreads) {
    tmp._threads.clear();
    tmp._dispatcher = {};
    tmp._numThreads = 0;
}

DispatchQueue& DispatchQueue::operator = (DispatchQueue&& tmp) noexcept {
    shutdown();
    this->_threads = std::move(tmp._threads);
    this->_dispatcher = std::move(tmp._dispatcher);
    this->_numThreads = tmp._numThreads;
    tmp._threads.clear();
    tmp._dispatcher = {};
    tmp._numThreads = 0;
    return *this;
}

void DispatchQueue::shutdown() noexcept {
    if (_threads.empty() == false) {
        for (auto& t : _threads)
            t.request_stop();

        _dispatcher->notify();

        for (auto& t : _threads)
            t.join();
    }
    _threads.clear();
    _dispatcher = nullptr;
    _numThreads = 0;
}

DispatchQueue& DispatchQueue::main() noexcept {
    static DispatchQueue queue(_mainQueue{});
    return queue;
}

DispatchQueue& DispatchQueue::global() noexcept {
    static uint32_t maxThreads =
#ifdef CPP_CORO_DISPATCHQUEUE_MAX_GLOBAL_THREADS
    (uint32_t)std::max(int(CPP_CORO_DISPATCHQUEUE_MAX_GLOBAL_THREADS), 1);
#else
        std::max(std::jthread::hardware_concurrency(), 3U) - 1;
#endif
    static DispatchQueue queue(maxThreads);
    return queue;
}

std::shared_ptr<DispatchQueue::_Dispatcher>
DispatchQueue::threadDispatcher(std::thread::id threadID) noexcept {
    return getThreadDispatcher(threadID);
}

std::shared_ptr<DispatchQueue::_Dispatcher>
DispatchQueue::localDispatcher() noexcept {
    return getThreadDispatcher(std::this_thread::get_id());
}

bool DispatchQueue::isMainThread() noexcept {
    return std::this_thread::get_id() == _local::get().mainThreadID;
}
