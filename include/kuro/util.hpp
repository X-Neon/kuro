#pragma once

#include <concepts>
#include <coroutine>
#include <type_traits>

namespace kuro
{

class void_t {};

namespace detail
{

template <typename T>
concept member_co_awaitable = requires (T a) { a.operator co_await(); };

template <typename T>
concept non_member_co_awaitable = requires (T a) { operator co_await(a); };

template <typename T>
concept generated_co_awaitable = member_co_awaitable<T> || non_member_co_awaitable<T>;

template <generated_co_awaitable T>
decltype(auto) get_base_awaitable(T& awaitable)
{
    if constexpr (member_co_awaitable<T>) {
        return awaitable.operator co_await();
    } else if constexpr (non_member_co_awaitable<T>) {
        return operator co_await(awaitable);
    } else {
        return operator co_await(awaitable);
    }
}

template <typename T>
struct get_base_awaitable_impl {};

template <member_co_awaitable T>
struct get_base_awaitable_impl<T> { using Type = decltype(std::declval<T>().operator co_await()); };

template <non_member_co_awaitable T>
struct get_base_awaitable_impl<T> { using Type = decltype(operator co_await(std::declval<T>())); };

template <typename T>
using get_base_awaitable_t = get_base_awaitable_impl<T>::Type;

template <typename T>
auto base_awaitable(T awaitable)
{
    if constexpr (member_co_awaitable<T>) {
        return awaitable.operator co_await();
    } else if constexpr (non_member_co_awaitable<T>) {
        return operator co_await(awaitable);
    } else {
        return awaitable;
    }
}

template <typename T>
using base_awaitable_t = decltype(base_awaitable(std::declval<std::remove_reference_t<T>>()));

template <typename T>
class awaitable_container
{
public:
    awaitable_container(T aw) : m_original(std::forward<T>(aw)) {}
    auto& get() { return m_original; }

private:
    T m_original;
};

template <generated_co_awaitable T>
class awaitable_container<T>
{
public:
    awaitable_container(T aw) : m_original(std::forward<T>(aw)), m_base(get_base_awaitable(m_original)) {}
    auto& get() { return m_base; }

private:
    T m_original;
    get_base_awaitable_t<T> m_base;
};

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