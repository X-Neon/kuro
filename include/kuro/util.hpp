#pragma once

#include <concepts>
#include <coroutine>
#include <type_traits>

namespace kuro
{

class void_t {};

namespace detail
{

template <typename Awaitable>
auto base_awaitable(Awaitable awaitable)
{
    constexpr bool member_co_await = requires (Awaitable a) { a.operator co_await(); };
    constexpr bool non_member_co_await = requires (Awaitable a) { operator co_await(a); };
    if constexpr (member_co_await) {
        return awaitable.operator co_await();
    } else if constexpr (non_member_co_await) {
        return operator co_await(awaitable);
    } else {
        return awaitable;
    }
}

template <typename T>
using base_awaitable_t = decltype(base_awaitable(std::declval<T>()));

template <typename T>
using awaited_t = decltype(std::declval<base_awaitable_t<T>>().await_resume());

template <typename T>
using non_void_awaited_t = std::conditional_t<std::is_void_v<awaited_t<T>>, void_t, awaited_t<T>>;

template <typename T>
concept await_suspend_returnable = std::same_as<T, bool> || std::same_as<T, void> || std::same_as<T, std::coroutine_handle<>>;

}

template <typename T>
concept awaitable = requires(detail::base_awaitable_t<T> a) {
    { a.await_ready() } -> std::same_as<bool>;
    { a.await_suspend(std::coroutine_handle<>{}) } -> detail::await_suspend_returnable<>;
    { a.await_resume() };
};

template <typename T>
concept cancellable = requires(detail::base_awaitable_t<T> a) {
    { a.await_cancel() } -> std::same_as<void>;
};

template <typename T>
concept cancellable_awaitable = awaitable<T> && cancellable<T>;

}