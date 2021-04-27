#pragma once

#include "continuation.hpp"

namespace kuro
{

class cancellation
{
public:
    void trigger()
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
            awaitable(cancellation* c) : m_cancel(c) {}
            bool await_ready() const noexcept
            {
                return m_cancel->is_set();
            }
            void await_suspend(std::coroutine_handle<> handle) noexcept
            {
                m_handle = handle;
                m_cancel->m_continuation.push(handle);
            }
            void await_resume() const noexcept {}
            void await_cancel() noexcept
            {
                m_cancel->m_continuation.erase(m_handle);
            }
        
        private:
            cancellation* m_cancel;
            std::coroutine_handle<> m_handle;
        };

        return awaitable{this};
    }

private:
    multi_continuation m_continuation;
    bool m_set = false;
};

}