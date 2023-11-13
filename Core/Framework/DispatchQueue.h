#pragma once
#include "../include.h"
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace FV {
    class AsyncTask {
    public:
        virtual ~AsyncTask() {}

        enum State {
            StatePending,
            StateProcessing,
            StateCompleted,
            StateCancelled,
        };
        virtual State state() const = 0;
        virtual bool wait() const = 0;
    };

    class FVCORE_API DispatchQueue {
    public:
        DispatchQueue(uint32_t);
        ~DispatchQueue();

        static DispatchQueue& main();
        static DispatchQueue& global();

        const uint32_t maxConcurrentQueues;

        using Task = std::function<void()>;
        std::shared_ptr<AsyncTask> async(Task fn);
        void await(Task fn) { async(fn)->wait(); }

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

        class _Dispatcher {
        public:
            virtual ~_Dispatcher() {}
            virtual uint32_t dispatch() = 0;
            virtual void wait() = 0;
            virtual void notify() = 0;
            virtual void cancelAllTasks() = 0;
            virtual std::shared_ptr<AsyncTask> enqueue(Task& t) = 0;
        };

    private:
        struct _DispatchQueueMain {
            enum { _Unused = 1 };
        };
        DispatchQueue(_DispatchQueueMain);

        struct Hook {
            const void* key;
            std::function<void(Task&)> notify;
        };
        std::vector<Hook> hooks;
        void notifyHook(Task& t) const;
        mutable std::mutex mutex;

        std::shared_ptr<_Dispatcher> dispatcher;
        std::vector<std::jthread> threads;

        DispatchQueue(const DispatchQueue&) = delete;
        DispatchQueue& operator = (const DispatchQueue&) = delete;
    };

    inline auto async(std::function<void()> fn) -> std::shared_ptr<AsyncTask> {
        return DispatchQueue::global().async(fn);
    }

    template <typename T> requires std::invocable<T>
    inline auto await(T&& t) -> decltype(t()) {
        std::function fn = std::forward<T>(t);
        return DispatchQueue::global().await(fn);
    }
}
