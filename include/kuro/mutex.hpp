#pragma once

#include "continuation.hpp"

namespace kuro
{

template <continuation_container Continuation = multi_continuation>
class mutex
{
    class locked_mutex
    {
    public:
        locked_mutex(mutex& m) : m_mutex(m)
        {
            m_mutex.lock();
        }
        locked_mutex(const locked_mutex&) = delete;
        locked_mutex& operator=(const locked_mutex&) = delete;
        ~locked_mutex()
        {
            m_mutex.unlock();
        }

    private:
        mutex& m_mutex;
    };

public:
    auto acquire()
    {
        struct mutex_lock_operation
        {
            mutex_lock_operation(mutex* m) : m_mutex(m) {}
            bool await_ready() const noexcept
            {
                return !m_mutex->is_locked();
            }
            void await_suspend(std::coroutine_handle<> handle) noexcept
            {
                m_handle = handle;
                m_mutex->m_continuation.push(handle);
            }
            auto await_resume() const noexcept
            {
                return locked_mutex(*m_mutex);
            }
            void await_cancel() noexcept
            {
                m_mutex->m_continuation.erase(m_handle);
            }

        private:
            mutex* m_mutex;
            std::coroutine_handle<> m_handle;
        };

        return mutex_lock_operation{this};
    }
    bool is_locked() const
    {
        return m_locked;
    }

private:
    void lock()
    {
        m_locked = true;
    }
    void unlock()
    {
        m_locked = false;
        m_continuation.resume_one();
    }

    Continuation m_continuation;
    bool m_locked = false;
};

}