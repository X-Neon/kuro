#pragma once

#include <coroutine>
#include <tuple>

#include "util.hpp"

namespace kuro
{

namespace detail
{

template <typename... T>
class gather_impl
{
public:
    gather_impl(T... args) : m_await(detail::awaitable_container<T>(std::forward<T>(args))...) {}

    bool await_ready() noexcept
    {
        std::size_t n_ready = 0;
        constexpr_for<0UL, std::tuple_size_v<decltype(m_await)>, 1UL>([this, &n_ready](auto i) mutable {
            n_ready += std::get<i.value>(m_await).get().await_ready();
        });

        return n_ready == std::tuple_size_v<decltype(m_await)>;
    }
    void await_suspend(std::coroutine_handle<> handle) noexcept
    {
        std::coroutine_handle<> inc_handle = [this](std::coroutine_handle<> resume) -> detail::task_executor {
            for (auto i = 0UL; i < std::tuple_size_v<decltype(m_await)>; ++i) {
                co_await std::suspend_always{};
            }
            resume.resume();
        }(handle).m_handle;

        constexpr_for<0UL, std::tuple_size_v<decltype(m_await)>, 1UL>([this, inc_handle](auto i) {
            using S = decltype(std::get<i.value>(m_await).get().await_suspend(inc_handle));
            if constexpr (std::is_void_v<S>) {
                std::get<i.value>(m_await).get().await_suspend(inc_handle);
            } else if constexpr (std::is_same_v<S, bool>) {
                if (!std::get<i.value>(m_await).get().await_suspend(inc_handle)) {
                    inc_handle.resume();
                }
            } else {
                std::get<i.value>(m_await).get().await_suspend(inc_handle).resume();
            }
        });
    }
    auto await_resume() noexcept
    {
        return std::apply([](auto&&... args) -> std::tuple<detail::non_void_awaited_t<T>...> { 
            return {non_void_resume(args.get())...}; 
        }, m_await);
    }

protected:
    template <typename U>
    static decltype(auto) non_void_resume(U& await)
    {
        if constexpr (std::is_void_v<decltype(await.await_resume())>) {
            await.await_resume();
            return void_t{};
        } else {
            return await.await_resume();
        }
    }

    template <auto Start, auto End, auto Inc, class F>
    static constexpr void constexpr_for(F&& f)
    {
        if constexpr (Start < End) {
            f(std::integral_constant<decltype(Start), Start>());
            constexpr_for<Start + Inc, End, Inc>(f);
        }
    }

    std::tuple<detail::awaitable_container<T>...> m_await;
};

template <typename... T>
class gather_cancel_impl : public gather_impl<T...>
{
public:
    gather_cancel_impl(T... args) : gather_impl<T...>(std::forward<T>(args)...) {}

    void await_cancel() noexcept
    {
        constexpr_for<0UL, std::tuple_size_v<decltype(this->m_await)>, 1UL>([this](auto i) mutable {
            std::get<i.value>(this->m_await).get().await_cancel();
        });
    }
};

}

template <awaitable... T>
auto gather(T&&... args)
{
    constexpr bool cancel = (cancellable<T> && ...);
    if constexpr (cancel) {
        return detail::gather_cancel_impl<T...>(std::forward<T>(args)...);
    } else {
        return detail::gather_impl<T...>(std::forward<T>(args)...);
    }
}


}