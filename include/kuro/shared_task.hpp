#pragma once

#include <coroutine>
#include <exception>
#include <type_traits>
#include <utility>
#include <vector>

#include "promise.hpp"

namespace kuro
{

template <typename T>
class shared_task
{
public:
    class promise_type : public detail::base_shared_promise_t<T>
    {
    public:
        shared_task<T> get_return_object() noexcept
        {
            return shared_task(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_always initial_suspend() const noexcept
        { 
            return {};
        }
        auto final_suspend() const noexcept
        {
            struct awaitable
            {
                bool await_ready() noexcept
                { 
                    return false; 
                }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> handle) noexcept
                {
                    auto& continuation = handle.promise().m_continuation;
                    if (!continuation.empty()) {
                        for (auto i = 0UL; i < continuation.size() - 1; ++i) {
                            continuation[i].resume();
                        }
                        return continuation.back();
                    }

                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return awaitable{};
        }
        void add_continuation(std::coroutine_handle<> continuation)
        {
            m_continuation.push_back(continuation);
        }
        void increment_refcount()
        {
            m_refcount++;
        }
        void decrement_refcount()
        {
            m_refcount--;
        }
        bool no_references_remaining() const
        {
            return m_refcount == 0;
        }
        bool has_started() const
        {
            return m_started;
        }
        void start()
        {
            m_started = true;
        }

    private:
        std::vector<std::coroutine_handle<>> m_continuation;
        int m_refcount = 0;
        bool m_started = false;
    };

    shared_task() : m_handle(nullptr) {}
    shared_task(const shared_task& other)
    {
        m_handle = other.m_handle;
        m_handle.promise().increment_refcount();
    }
    shared_task& operator=(const shared_task& other)
    {
        destruct();
        m_handle = other.m_handle;
        m_handle.promise().increment_refcount();
        return *this;
    }
    shared_task(shared_task&& other) noexcept
    {
        m_handle = other.m_handle;
        other.m_handle = {};
    }
    shared_task& operator=(shared_task&& other) noexcept
    {
        if (m_handle != other.m_handle) {
            destruct();
            m_handle = other.m_handle;
            other.m_handle = {};
        }
        return *this;
    }
    ~shared_task()
    {
        destruct();
    }

    auto operator co_await()
    {
        struct awaitable
        {
            awaitable(shared_task* ptr) : m_ptr(ptr) {}
            decltype(auto) await_resume()
            {
                return m_handle.promise().result();
            }
            bool await_ready() const noexcept
            {
                return m_handle.done();
            }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent_handle) noexcept
            {
                m_handle.promise().add_continuation(parent_handle);
                if (!m_handle.promise().has_started()) {
                    m_handle.promise().start();
                    return m_handle;
                }
                return std::noop_coroutine();
            }

        private:
            shared_task* m_ptr;
        };

        return awaitable{this};
    }

    bool done() const noexcept
    {
        return m_handle.done();
    }

private:
    shared_task(std::coroutine_handle<promise_type> handle) : m_handle(handle)
    {
        m_handle.promise().increment_refcount();
    }
    void destruct() noexcept
    {
        if (m_handle){
            m_handle.promise().decrement_refcount();
            if (m_handle.promise().no_references_remaining()) {
                m_handle.destroy();
            }
        }
    }

    std::coroutine_handle<promise_type> m_handle;
};

}