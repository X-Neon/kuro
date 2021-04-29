#pragma once

#include <coroutine>

#include "cancellation.hpp"
#include "util.hpp"
#include "sleep.hpp"
#include "task_executor.hpp"

namespace kuro
{

template <typename T, typename Cancel>
class cancel_awaitable
{
public:
    cancel_awaitable(T awaitable, Cancel cancel) : 
        m_await(std::forward<T>(awaitable)),
        m_cancel_await(std::move(cancel)),
        m_cancelled(false) {}

    bool await_ready()
    {
        if (m_cancel_await.await_ready()) {
            m_cancelled = true;
            return true;
        }
        
        return m_await.get().await_ready();
    }
    auto await_suspend(std::coroutine_handle<> handle)
    {
        m_exec_handle = [](std::coroutine_handle<> h, cancel_awaitable* ptr) -> detail::task_executor {
            co_await ptr->m_cancel_await;
            ptr->m_cancelled = true;
            ptr->m_await.get().await_cancel();
            h.resume();
        }(handle, this).m_handle;
        return m_await.get().await_suspend(handle);
    }
    std::optional<detail::non_void_awaited_t<T>> await_resume()
    {
        if (m_cancelled) {
            return {};
        } else {
            m_cancel_await.await_cancel();
            m_exec_handle.destroy();
            if constexpr (std::is_void_v<detail::awaited_t<T>>) {
                m_await.get().await_resume();
                return void_t{};
            } else {
                return m_await.get().await_resume();
            }
        }
    }
    void await_cancel()
    {
        m_await.get().await_cancel();
        m_cancel_await.await_cancel();
        m_exec_handle.destroy();
    }

private:
    detail::awaitable_container<T> m_await;
    Cancel m_cancel_await;
    bool m_cancelled;
    std::coroutine_handle<> m_exec_handle;
};

template <cancellable_awaitable T>
auto with_cancellation(T&& aw, cancellation& cancel)
{
    return cancel_awaitable<T, decltype(cancel.wait())>(std::forward<T>(aw), cancel.wait());
}

template <cancellable_awaitable T>
auto with_timeout(T&& aw, duration d)
{
    return cancel_awaitable<T, decltype(sleep_for(d))>(std::forward<T>(aw), sleep_for(d));
}

}