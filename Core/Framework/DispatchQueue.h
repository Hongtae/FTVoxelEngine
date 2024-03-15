#pragma once
#include "../include.h"
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <coroutine>


namespace FV {
    class DispatchQueue {
    public:
        DispatchQueue(uint32_t numThreads) noexcept;
        DispatchQueue(DispatchQueue&&) noexcept;
        ~DispatchQueue() noexcept {
            shutdown();
        }

        auto schedule() const noexcept {
            struct [[nodiscard]] Awaiter {
                constexpr bool await_ready() const noexcept { return false; }
                constexpr void await_resume() const noexcept {}

                void await_suspend(std::coroutine_handle<> handle) const {
                    queue._dispatcher->enqueue(handle);
                }
                const DispatchQueue& queue;
            };
            return Awaiter{ *this };
        }

        auto schedule(double after) const noexcept {
            struct [[nodiscard]] Awaiter {
                constexpr bool await_ready() const noexcept { return false; }
                constexpr void await_resume() const noexcept {}

                void await_suspend(std::coroutine_handle<> handle) const {
                    queue._dispatcher->enqueue(handle, after);
                }
                const DispatchQueue& queue;
                double after;
            };
            return Awaiter{ *this, after };
        }

        auto operator co_await() const noexcept {
            return schedule();
        }

        static DispatchQueue& main() noexcept;
        static DispatchQueue& global() noexcept;

        uint32_t numThreads() const noexcept { return _numThreads; }

        class _Dispatcher {
        public:
            virtual ~_Dispatcher() {}
            virtual uint32_t dispatch() = 0;
            virtual void wait() = 0;
            virtual bool wait(double timeout) = 0;
            virtual void notify() = 0;
            virtual void enqueue(std::coroutine_handle<>) = 0;
            virtual void enqueue(std::coroutine_handle<>, double) = 0;
            virtual bool isMain() const = 0;
            auto enter() noexcept {
                struct [[nodiscard]] Awaiter {
                    constexpr bool await_ready() const noexcept { return false; }
                    constexpr void await_resume() const noexcept {}
                    void await_suspend(std::coroutine_handle<> handle) const {
                        dispatcher->enqueue(handle);
                    }
                    _Dispatcher* dispatcher;
                };
                return Awaiter{ this };
            }
            // automatically destroyed when it finishes running.
            virtual void detach(std::coroutine_handle<>) = 0;
        };
        static std::shared_ptr<_Dispatcher> localDispatcher() noexcept;
        static std::shared_ptr<_Dispatcher> threadDispatcher(std::thread::id) noexcept;
        std::shared_ptr<_Dispatcher> dispatcher() const noexcept { return _dispatcher; }

        static bool isMainThread() noexcept;
        bool isMain() const noexcept { return _dispatcher->isMain(); }

        DispatchQueue& operator = (DispatchQueue&&) noexcept;
    private:
        struct _mainQueue {};
        DispatchQueue(_mainQueue) noexcept;
        void shutdown() noexcept;

        uint32_t _numThreads;
        std::vector<std::jthread> _threads;
        std::shared_ptr<_Dispatcher> _dispatcher;

        DispatchQueue(const DispatchQueue&) = delete;
        DispatchQueue& operator = (const DispatchQueue&) = delete;
    };

    void setDispatchQueueMainThread();

    inline DispatchQueue& dispatchGlobal() noexcept {
        return DispatchQueue::global();
    }

    inline DispatchQueue& dispatchMain() noexcept {
        return DispatchQueue::main();
    }

    template <typename T> concept AsyncQueue = requires {
        {T::queue()}->std::convertible_to<DispatchQueue&>;
    };

    struct AsyncQueueGlobal {
        static DispatchQueue& queue() noexcept { return DispatchQueue::global(); }
    };

    struct AsyncQueueMain {
        static DispatchQueue& queue() noexcept { return DispatchQueue::main(); }
    };

    static_assert(AsyncQueue<AsyncQueueGlobal>);
    static_assert(AsyncQueue<AsyncQueueMain>);

    template <typename T>
    struct _AwaiterContinuation {
        constexpr bool await_ready() const noexcept { return false; }
        auto await_suspend(std::coroutine_handle<typename T::promise_type> handle) const noexcept {
            auto continuation = handle.promise().continuation;
            handle.promise().continuation = std::noop_coroutine();
            return continuation;
        }
        constexpr void await_resume() noexcept {}
    };

    template <typename T>
    struct _AwaiterCoroutineBase {
        bool await_ready() const noexcept {
            return !handle || handle.done();
        }
        auto await_suspend(std::coroutine_handle<> awaiting_coroutine) const noexcept {
            handle.promise().continuation = awaiting_coroutine;
            return handle;
        }
        std::coroutine_handle<typename T::promise_type> handle;
    };

    template <typename T>
    struct _PromiseBase {
        std::suspend_always initial_suspend() noexcept { return {}; }
        auto final_suspend() noexcept {
            return _AwaiterContinuation<T>{};
        }
        T get_return_object() noexcept {
            return T{ std::coroutine_handle<T::promise_type>::from_promise((typename T::promise_type&)*this) };
        }
        void unhandled_exception() { throw; }
        std::coroutine_handle<> continuation = std::noop_coroutine();
    };

    void _threadLocalDeferred(std::function<void()>);

    template <typename T>
    struct _AsyncAwaiterDispatchContinuation {
        constexpr bool await_ready() const noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<typename T::promise_type> handle) const noexcept {
            auto continuation = handle.promise().continuation;
            handle.promise().continuation = {};

            auto target = DispatchQueue::threadDispatcher(continuation.threadID);
            if (target == nullptr)
                return continuation.coroutine;
            auto current = DispatchQueue::localDispatcher();
            if (current == nullptr)
                return continuation.coroutine;
            if (target == current && target->isMain() == false)
                return continuation.coroutine;
            _threadLocalDeferred([=] { target->enqueue(continuation.coroutine); });
            return std::noop_coroutine();
        }
        constexpr void await_resume() noexcept {}
    };

    template <typename T>
    struct _AsyncAwaiterDispatchCoroutineBase {
        bool await_ready() const noexcept {
            return !handle || handle.done();
        }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coroutine) {
            handle.promise().continuation.coroutine = awaiting_coroutine;
            handle.promise().continuation.threadID = std::this_thread::get_id();

            auto dispatcher = T::Queue::queue().dispatcher();
            if (dispatcher->isMain() == false && dispatcher == DispatchQueue::localDispatcher())
                return handle;
            dispatcher->enqueue(handle);
            return std::noop_coroutine();
        }
        std::coroutine_handle<typename T::promise_type> handle;
    };

    template <typename T>
    struct _AsyncPromiseBase {
        std::suspend_always initial_suspend() noexcept { return {}; }
        auto final_suspend() noexcept {
            return _AsyncAwaiterDispatchContinuation<T>{};
        }
        T get_return_object() noexcept {
            return T{ std::coroutine_handle<T::promise_type>::from_promise((typename T::promise_type&)*this) };
        }
        void unhandled_exception() { throw; }
        struct {
            std::coroutine_handle<> coroutine = std::noop_coroutine();
            std::thread::id threadID = {};
        } continuation;
    };

    template <typename T = void> struct [[nodiscard]] Task {
        template <typename R> struct _Promise : _PromiseBase<Task> {
            template <std::convertible_to<R> V>
            void return_value(V&& value) noexcept {
                _result.emplace(std::forward<V>(value));
            }
            std::optional<R> _result;
            auto&& result() const {
                return _result.value();
            }
        };
        template <> struct _Promise<void> : _PromiseBase<Task> {
            constexpr void return_void() const noexcept {}
            constexpr void result() const noexcept {}
        };
        using promise_type = _Promise<T>;
        std::coroutine_handle<promise_type> handle;

        auto operator co_await() const noexcept {
            struct Awaiter : _AwaiterCoroutineBase<Task> {
                auto await_resume() -> decltype(handle.promise().result()) const {
                    return handle.promise().result();
                }
            };
            return Awaiter{ handle };
        }

        auto result() -> decltype(handle.promise().result()) const {
            return handle.promise().result();
        }

        bool done() const noexcept {
            return (!handle || handle.done());
        }

        explicit operator bool() const noexcept {
            return done() == false;
        }

        Task() = default;
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
        Task(Task&& other) noexcept : handle{ other.handle } { other.handle = {}; }
        Task& operator=(Task&& other) noexcept {
            if (this != &other) {
                if (handle)
                    handle.destroy();
                handle = other.handle;
                other.handle = {};
            }
            return *this;
        }
        explicit Task(std::coroutine_handle<promise_type> h) noexcept : handle(h) {}
        ~Task() {
            if (handle)
                handle.destroy();
        }
    };

    template <typename T, AsyncQueue Q>
    struct [[nodiscard]] AsyncTask {
        using Queue = Q;
        template <typename R> struct _Promise : _AsyncPromiseBase<AsyncTask> {
            template <std::convertible_to<R> V>
            void return_value(V&& value) noexcept {
                _result.emplace(std::forward<V>(value));
            }
            auto&& result() const { return _result.value(); }
            std::optional<R> _result;
        };
        template <> struct _Promise<void> : _AsyncPromiseBase<AsyncTask> {
            constexpr void return_void() const noexcept {}
            constexpr void result() const noexcept {}
        };
        using promise_type = _Promise<T>;
        std::coroutine_handle<promise_type> handle;

        auto operator co_await() const noexcept {
            struct Awaiter : _AsyncAwaiterDispatchCoroutineBase<AsyncTask> {
                auto await_resume() -> decltype(handle.promise().result()) const {
                    return handle.promise().result();
                }
            };
            return Awaiter{ handle };
        }

        auto result() -> decltype(handle.promise().result()) const {
            return handle.promise().result();
        }

        bool done() const noexcept {
            return (!handle || handle.done());
        }

        explicit operator bool() const noexcept {
            return done() == false;
        }

        AsyncTask() = default;
        AsyncTask(const AsyncTask&) = delete;
        AsyncTask& operator=(const AsyncTask&) = delete;
        AsyncTask(AsyncTask&& other) noexcept : handle{ other.handle } {
            other.handle = {};
        }
        AsyncTask& operator=(AsyncTask&& other) noexcept {
            if (this != &other) {
                if (handle)
                    handle.destroy();
                handle = other.handle;
                other.handle = {};
            }
            return *this;
        }
        explicit AsyncTask(std::coroutine_handle<promise_type> h) noexcept : handle(h) {}
        ~AsyncTask() {
            if (handle)
                handle.destroy();
        }
    };

    template <typename T> requires (!std::same_as<T, void>)
    struct [[nodiscard]] Generator {
        struct promise_type : _PromiseBase<Generator> {
            template <std::convertible_to<T> R>
            auto yield_value(R&& v) noexcept {
                _value.emplace(std::forward<R>(v));
                return _AwaiterContinuation<Generator>{};
            }
            constexpr void return_void() const noexcept {}
            std::optional<T> _value;
            auto&& value() const { return _value.value(); }
        };
        std::coroutine_handle<promise_type> handle;

        auto operator co_await() const noexcept {
            struct Awaiter : _AwaiterCoroutineBase<Generator> {
                bool await_resume() const noexcept {
                    return this->await_ready() == false;
                }
            };
            return Awaiter{ handle };
        }

        auto&& value() const {
            return handle.promise().value();
        }

        bool done() const noexcept {
            return (!handle || handle.done());
        }

        explicit operator bool() const noexcept {
            return done() == false;
        }

        Generator() = default;
        Generator(const Generator&) = delete;
        Generator& operator=(const Generator&) = delete;
        Generator(Generator&& other) noexcept : handle{ other.handle } {
            other.handle = {};
        }
        Generator& operator=(Generator&& other) noexcept {
            if (this != &other) {
                if (handle)
                    handle.destroy();
                handle = other.handle;
                other.handle = {};
            }
            return *this;
        }
        explicit Generator(std::coroutine_handle<promise_type> h) noexcept : handle(h) {}
        ~Generator() {
            if (handle)
                handle.destroy();
        }
    };

    template <typename T, AsyncQueue Q = AsyncQueueGlobal> requires (!std::same_as<T, void>)
    struct [[nodiscard]] AsyncGenerator {
        using Queue = Q;
        struct promise_type : _AsyncPromiseBase<AsyncGenerator> {
            template <std::convertible_to<T> R>
            auto yield_value(R&& v) noexcept {
                value.emplace(std::forward<R>(v));
                return _AsyncAwaiterDispatchContinuation<AsyncGenerator>{};
            }
            constexpr void return_void() const noexcept {}
            std::optional<T> value;
        };
        std::coroutine_handle<promise_type> handle;

        auto operator co_await() const noexcept {
            struct Awaiter : _AsyncAwaiterDispatchCoroutineBase<AsyncGenerator> {
                bool await_resume() const noexcept {
                    return this->await_ready() == false;
                }
            };
            return Awaiter{ handle };
        }

        auto&& value() const {
            return handle.promise().value.value();
        }

        bool done() const noexcept {
            return (!handle || handle.done());
        }

        explicit operator bool() const noexcept {
            return done() == false;
        }

        AsyncGenerator() = default;
        AsyncGenerator(const AsyncGenerator&) = delete;
        AsyncGenerator& operator=(const AsyncGenerator&) = delete;
        AsyncGenerator(AsyncGenerator&& other) noexcept : handle{ other.handle } {
            other.handle = {};
        }
        AsyncGenerator& operator=(AsyncGenerator&& other) noexcept {
            if (this != &other) {
                if (handle)
                    handle.destroy();
                handle = other.handle;
                other.handle = {};
            }
            return *this;
        }
        explicit AsyncGenerator(std::coroutine_handle<promise_type> h) noexcept : handle(h) {}
        ~AsyncGenerator() {
            if (handle)
                handle.destroy();
        }
    };

    template <AsyncQueue Q>
    inline auto detachedTask(AsyncTask<void, Q> task) {
        Q::queue().dispatcher()->detach(task.handle);
        task.handle = {};
    }
    template <AsyncQueue Q>
    inline auto detachedTask(AsyncTask<void, Q>& task) {
        Q::queue().dispatcher()->detach(task.handle);
        task.handle = {};
    }
    template <std::convertible_to<Task<void>> T>
    inline auto detachedTask(T&& task, DispatchQueue& queue = dispatchGlobal()) {
        queue.dispatcher()->detach(task.handle);
        task.handle = {};
    }

    template <AsyncQueue Q = AsyncQueueGlobal>
    inline AsyncTask<void, Q> asyncSleep(double t) {
        co_await Q::queue().schedule(t);
        co_return;
    }

    template <AsyncQueue Q = AsyncQueueGlobal>
    inline AsyncTask<void, Q> asyncYield() {
        co_await Q::queue().schedule();
        co_return;
    }

    template <AsyncQueue Q = AsyncQueueGlobal>
    inline AsyncTask<void, Q> asyncWaitQueue(DispatchQueue& queue) {
        co_await queue;
        FVASSERT_DEBUG(queue.dispatcher() == DispatchQueue::localDispatcher());
        queue.dispatcher()->wait();
        co_return;
    }

    template <typename _Task> struct _TaskGroup {
        std::vector<_Task> tasks;

        template <std::convertible_to<_Task> ... Ts>
        constexpr _TaskGroup(Ts&&... ts) {
            tasks.reserve(sizeof...(Ts));
            (tasks.emplace_back(std::forward<Ts>(ts)), ...);
        }

        template <std::convertible_to<_Task> ... Ts>
        constexpr void emplace(Ts&&... ts) {
            tasks.reserve(tasks.size() + sizeof...(Ts));
            (tasks.emplace_back(std::forward<Ts>(ts)), ...);
        }

        template <std::convertible_to<_Task> ... Ts>
        _TaskGroup& operator << (Ts&&... ts) {
            emplace(std::forward<Ts>(ts)...);
            return *this;
        }

        constexpr size_t size() const noexcept {
            return tasks.size();
        }

        _TaskGroup(const _TaskGroup&) = delete;
        _TaskGroup& operator = (const _TaskGroup&) = delete;
        _TaskGroup(_TaskGroup&& tmp) : tasks(std::move(tmp.tasks)) {}
        _TaskGroup& operator = (_TaskGroup&& other) {
            tasks = std::move(other.tasks);
            return *this;
        }
    };

    template <typename T = void>
    using TaskGroup = _TaskGroup<Task<T>>;

    template <typename T = void, AsyncQueue Q = AsyncQueueGlobal>
    using AsyncTaskGroup = _TaskGroup<AsyncTask<T, Q>>;

    template <AsyncQueue Q, typename Group>
    inline Task<void> _async(Group group) {
        if (group.tasks.empty())
            co_return;

        auto& tasks = group.tasks;
        auto& queue = Q::queue();
        std::ranges::for_each(tasks, [&](auto& task) {
            queue.schedule().await_suspend(task.handle);
        });

        while (1) {
            bool done = true;
            for (auto& task : tasks) {
                if (task.handle.done() == false) {
                    done = false;
                    break;
                }
            }
            if (done)
                break;
            co_await asyncYield();
        }
        co_return;
    }

    template <typename T, AsyncQueue Q, typename Group>
    inline Generator<T> _async(Group group) {
        if (group.tasks.empty())
            co_return;

        auto& tasks = group.tasks;

        auto& queue = Q::queue();
        std::ranges::for_each(tasks, [&](auto& task) {
            queue.schedule().await_suspend(task.handle);
        });
        while (tasks.empty() == false) {
            if (tasks.front().handle.done()) {
                co_yield tasks.front().result();
                tasks.erase(tasks.begin());
            }
            co_await asyncYield();
        }
    }

    template <AsyncQueue Q = AsyncQueueGlobal>
    inline Task<void> async(TaskGroup<void>& group) {
        return _async<Q>(std::move(group));
    }

    template <AsyncQueue Q = AsyncQueueGlobal>
    inline Task<void> async(TaskGroup<void>&& group) {
        return _async<Q>(std::move(group));
    }

    template <AsyncQueue Q = AsyncQueueGlobal>
    inline Task<void> async(AsyncTaskGroup<void, Q>& group) {
        return _async<Q>(std::move(group));
    }

    template <AsyncQueue Q = AsyncQueueGlobal>
    inline Task<void> async(AsyncTaskGroup<void, Q>&& group) {
        return _async<Q>(std::move(group));
    }

    template <typename T, AsyncQueue Q = AsyncQueueGlobal>
    inline Generator<T> async(TaskGroup<T>& group) {
        return _async<T, Q>(std::move(group));
    }

    template <typename T, AsyncQueue Q = AsyncQueueGlobal>
    inline Generator<T> async(TaskGroup<T>&& group) {
        return _async<T, Q>(std::move(group));
    }

    template <typename T, AsyncQueue Q = AsyncQueueGlobal>
    inline Generator<T> async(AsyncTaskGroup<T, Q>& group) {
        return _async<T, Q>(std::move(group));
    }

    template <typename T, AsyncQueue Q = AsyncQueueGlobal>
    inline Generator<T> async(AsyncTaskGroup<T, Q>&& group) {
        return _async<T, Q>(std::move(group));
    }

    template <typename T = void>
    using Async = AsyncTask<T, AsyncQueueGlobal>;

    template <typename T = void>
    using AsyncMain = AsyncTask<T, AsyncQueueMain>;
}
