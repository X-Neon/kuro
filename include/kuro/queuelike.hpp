#pragma once

#include <queue>
#include <stack>

#include "continuation.hpp"

namespace kuro
{

template <typename Container, continuation_container Continuation = multi_continuation>
class queuelike
{
    using T = Container::value_type;

public:
    void push(const T& value)
    {
        m_queue.push(value);
        m_continuation.resume_one();
    }
    void push(T&& value)
    {
        m_queue.push(std::move(value));
        m_continuation.resume_one();
    }
    bool empty() const
    {
        return m_queue.empty();
    }
    auto pop()
    {
        struct pop_operation
        {
            pop_operation(queuelike* q) : m_queuelike(q) {}
            bool await_ready() const noexcept
            {
                return !m_queuelike->empty();
            }
            void await_suspend(std::coroutine_handle<> handle) noexcept
            {
                m_handle = handle;
                m_queuelike->m_continuation.push(handle);
            }
            T await_resume() noexcept
            {
                constexpr bool has_top = requires(Container c) { c.top(); };

                if constexpr (has_top) {
                    T value = std::move(m_queuelike->m_queue.top());
                    m_queuelike->m_queue.pop();
                    return value;
                } else {
                    T value = std::move(m_queuelike->m_queue.front());
                    m_queuelike->m_queue.pop();
                    return value;
                }
                
            }
            void await_cancel() noexcept
            {
                m_queuelike->m_continuation.erase(m_handle);
            }
        
        private:
            queuelike* m_queuelike;
            std::coroutine_handle<> m_handle;
        };

        return pop_operation{this};
    }

private:
    Container m_queue;
    Continuation m_continuation;
};

template <std::movable T>
using queue = queuelike<std::queue<T>>;

template <std::movable T>
using stack = queuelike<std::stack<T>>;

template <std::movable T>
using priority_queue = queuelike<std::priority_queue<T>>;

}