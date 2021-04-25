#pragma once

#include <coroutine>

#include "promise.hpp"

namespace kuro::detail
{

class task_executor
{
public:
    class promise_type
    {
    public:
        task_executor get_return_object() noexcept
        {
            return task_executor{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() const noexcept
        { 
            return {};
        }
        std::suspend_never final_suspend() const noexcept
        {
            return {};
        }
        void return_void() noexcept {}
        void unhandled_exception() {}
    };

    std::coroutine_handle<promise_type> m_handle;
};

template <typename T>
class task_executor_return
{
public:
    class promise_type final : public base_promise_t<T>
    {
    public:
        task_executor_return<T> get_return_object() noexcept
        {
            return task_executor_return(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
    };

    std::coroutine_handle<promise_type> m_handle;
};

}