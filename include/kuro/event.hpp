#pragma once

#include "continuation.hpp"

namespace kuro
{

template <continuation_container Continuation = multi_continuation>
class event
{
public:
    void set()
    {
        m_set = true;
        m_continuation.resume_all();
    }
    bool is_set() const
    {
        return m_set;
    }

    auto wait()
    {
        struct awaitable
        {
            awaitable(event* c) : m_event(c) {}
            bool await_ready() const noexcept
            {
                return m_event->is_set();
            }
            void await_suspend(std::coroutine_handle<> handle) noexcept
            {
                m_handle = handle;
                m_event->m_continuation.push(handle);
            }
            void await_resume() const noexcept {}
            void await_cancel() noexcept
            {
                m_event->m_continuation.erase(m_handle);
            }
        
        private:
            event* m_event;
            std::coroutine_handle<> m_handle;
        };

        return awaitable{this};
    }

private:
    Continuation m_continuation;
    bool m_set = false;
};

}