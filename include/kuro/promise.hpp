#pragma once

#include <coroutine>
#include <exception>
#include <type_traits>

namespace kuro::detail
{

template <typename T>
class return_value_promise
{
    enum class promise_state
    {
        empty,
        data,
        exception
    };

public:
    return_value_promise() {};
    ~return_value_promise()
    {
        if (m_state == promise_state::data) {
            m_data.~T();
        } else if (m_state == promise_state::exception) {
            m_except.~exception_ptr();
        } 
    }
    void unhandled_exception()
    {
        m_except = std::current_exception();
        m_state = promise_state::exception;
    }

protected:
    T& get_data()
    {
        if (m_state == promise_state::data) {
            return m_data;
        } else if (m_state == promise_state::exception) {
            std::rethrow_exception(m_except);
        } else {
            throw std::exception{};
        }
    }
    
    template <typename U>
    void set_data(U&& data)
    {
        new(&m_data) T(std::forward<U>(data));
        m_state = promise_state::data;
    }

private:
    promise_state m_state = promise_state::empty;
    union {
        T m_data;
        std::exception_ptr m_except;
    };
};

template <typename T>
class value_promise : public return_value_promise<T>
{
public:
    T result()
    {
        return std::move(this->get_data());
    }
    void return_value(T value) noexcept
    {
        this->set_data(std::move(value));
    }
};

template <typename T>
class shared_value_promise : public return_value_promise<T>
{
public:
    T& result()
    {
        return this->get_data();
    }
    void return_value(T value) noexcept
    {
        this->set_data(std::move(value));
    }
};

template <typename T>
class reference_promise : public return_value_promise<T*>
{
public:
    T& result()
    {
        return *this->get_data();
    }
    void return_value(T& value) noexcept
    {
        this->set_data(&value);
    }
};

class void_promise
{
public:
    void result() const
    {
        if (m_except) {
            std::rethrow_exception(m_except);
        }
    }
    void return_void() noexcept {}
    void unhandled_exception()
    {
        m_except = std::current_exception();
    }

private:
    std::exception_ptr m_except = nullptr;
};

template <typename T>
struct base_promise {
    using type = value_promise<T>;
};

template <typename T>
struct base_promise<T&> {
    using type = reference_promise<T>;
};

template <>
struct base_promise<void> {
    using type = void_promise;
};

template <typename T>
using base_promise_t = base_promise<T>::type;

template <typename T>
struct base_shared_promise {
    using type = shared_value_promise<T>;
};

template <typename T>
struct base_shared_promise<T&> {
    using type = reference_promise<T>;
};

template <>
struct base_shared_promise<void> {
    using type = void_promise;
};

template <typename T>
using base_shared_promise_t = base_shared_promise<T>::type;

}