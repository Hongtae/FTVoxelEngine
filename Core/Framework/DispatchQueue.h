#pragma once
#include "../include.h"
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace FV {
    class FVCORE_API DispatchQueue {
    public:
        ~DispatchQueue();

        static DispatchQueue& main();
        static DispatchQueue& global();

        const uint32_t maxConcurrentQueues;

        using Task = std::function<void()>;
        void async(Task fn);
        void await(Task fn);

        template <typename R>
        auto await(std::function<R()> fn) -> R {
            R result{};
            await([&] { result = fn(); });
            return result;
        }

        void yield();
        uint32_t dispatch();

        void setHook(const void* key, std::function<void(Task&)>);
        void unsetHook(const void* key);
    private:
        DispatchQueue(uint32_t);
        struct _DispatchQueueMain {
            enum { _Unused = 1 };
        };
        DispatchQueue(_DispatchQueueMain);

        enum State {
            StatePending,
            StateProcessing,
            StateCompleted,
            StateCancelled,
        };

        struct Command {
            std::function<void()> op;
            State state;
        };
        std::vector<std::shared_ptr<Command>> commands;
        std::condition_variable cond;
        mutable std::mutex mutex;

        struct Hook {
            const void* key;
            std::function<void(Task&)> notify;
        };
        std::vector<Hook> hooks;
        std::vector<std::jthread> threads;
        
        bool isWorkerThread() const;
        bool isWorkerThread(std::unique_lock<std::mutex>&) const;
        void notifyHook(std::unique_lock<std::mutex>&, Task& t) const;
        uint32_t dispatch(std::unique_lock<std::mutex>&);

        DispatchQueue(const DispatchQueue&) = delete;
        DispatchQueue& operator = (const DispatchQueue&) = delete;
    };

    inline void async(std::function<void()> fn) {
        DispatchQueue::global().async(fn);
    }

    template <typename T> requires std::invocable<T>
    inline auto await(T&& t) -> decltype(t()) {
        std::function fn = std::forward<T>(t);
        return DispatchQueue::global().await(fn);
    }
}
