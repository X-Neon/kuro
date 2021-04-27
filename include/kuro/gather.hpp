#pragma once

#include <coroutine>
#include <tuple>

#include "util.hpp"

namespace kuro
{

template <awaitable... T>
class gather
{
    template <typename U>
    struct ref_wrapper { U* ptr; };

    template <typename U>
    using non_ref_t = std::conditional_t<std::is_reference_v<U>, ref_wrapper<std::remove_reference_t<U>>, U>;

public:
    gather(T... args) : m_await(std::move(args)...) {}
    bool await_ready() const noexcept
    {
        return false;
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
            [i](gather* ptr, std::coroutine_handle<> h) -> detail::task_executor {
                using V = detail::awaited_t<std::tuple_element_t<i.value, std::tuple<T...>>>;
                if constexpr (std::is_void_v<V>) {
                    co_await std::get<i.value>(ptr->m_await);
                } else if constexpr (std::is_reference_v<V>) {
                    std::get<i.value>(ptr->m_result) = {&(co_await std::get<i.value>(ptr->m_await))};
                } else {
                    std::get<i.value>(ptr->m_result) = co_await std::get<i.value>(ptr->m_await);
                }
                h.resume();
            }(this, inc_handle);
        });
    }
    auto await_resume() noexcept
    {
        return std::apply([](auto&&... args) -> std::tuple<detail::non_void_awaited_t<T>...> { return {transform(args)...}; }, m_result);
    }

private:
    template <typename U>
    static auto&& transform(U& u)
    {
        return std::move(u);
    }

    template <typename U>
    static auto& transform(ref_wrapper<U>& u)
    {
        return *u.ptr;
    }

    template <auto Start, auto End, auto Inc, class F>
    static constexpr void constexpr_for(F&& f)
    {
        if constexpr (Start < End) {
            f(std::integral_constant<decltype(Start), Start>());
            constexpr_for<Start + Inc, End, Inc>(f);
        }
    }

    std::tuple<T...> m_await;
    std::tuple<non_ref_t<detail::non_void_awaited_t<T>>...> m_result;
};

}