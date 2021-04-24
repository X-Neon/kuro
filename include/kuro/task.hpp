#pragma once

#include <coroutine>
#include <exception>
#include <type_traits>
#include <utility>

#include "promise.hpp"
#include "unique_coroutine_handle.hpp"

namespace kuro
{

template <typename T>
class task
{
public:
    class promise_type final : public detail::base_promise_t<T>
    {
    public:
        task<T> get_return_object() noexcept
        {
            return task(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        auto final_suspend() const noexcept
        {
            struct awaitable
            {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> handle) noexcept
                {
                    auto continuation = handle.promise().m_continuation;
                    if (continuation) {
                        return continuation;
                    }

                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return awaitable{};
        }
        void set_continuation(std::coroutine_handle<> continuation)
        {
            m_continuation = continuation;
        }

    private:
        std::coroutine_handle<> m_continuation;
    };

    auto operator co_await()
    {
        struct awaitable
        {
            awaitable(task* ptr) : m_ptr(ptr) {}
            T await_resume()
            {
                return m_ptr->m_handle.promise().result();
            }
            bool await_ready() const noexcept
            {
                return m_ptr->m_handle.done();
            }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent_handle) noexcept
            {
                m_ptr->m_handle.promise().set_continuation(parent_handle);
                return m_ptr->m_handle;
            }
        
        private:
            task* m_ptr;
        };

        return awaitable{this};
    }

    bool done() const noexcept
    {
        return m_handle.done();
    }
    

private:
    task(std::coroutine_handle<promise_type> handle) : m_handle(handle) {}

    detail::unique_coroutine_handle<promise_type> m_handle;
};

}