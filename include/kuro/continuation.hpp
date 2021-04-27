#pragma once

#include <coroutine>
#include <optional>
#include <vector>

namespace kuro
{

template <typename T>
concept continuation_container = requires (T a) {
    { a.push(std::coroutine_handle<>{}) } -> std::same_as<void>;
    { a.resume_one() } -> std::same_as<void>;
    { a.resume_all() } -> std::same_as<void>;
    { a.erase(std::coroutine_handle<>{}) } -> std::same_as<void>;
};

class multi_continuation
{
public:
    multi_continuation() = default;
    multi_continuation(const multi_continuation&) = delete;
    multi_continuation& operator=(const multi_continuation&) = delete;

    void push(std::coroutine_handle<> handle)
    {
        m_continuations.push_back(handle);
    }
    void resume_one()
    {
        if (!m_continuations.empty()) {
            auto h = m_continuations.back();
            m_continuations.pop_back();
            h.resume();
        }
    }
    void resume_all()
    {
        while (!m_continuations.empty()) {
            auto h = m_continuations.back();
            m_continuations.pop_back();
            h.resume();
        }
    }

    void erase(std::coroutine_handle<> handle)
    {
        for (auto i = 0UL; i < m_continuations.size(); ++i) {
            if (m_continuations[i].address() == handle.address()) {
                m_continuations.erase(m_continuations.begin() + i);
                return;
            }
        }
    }

private:
    std::vector<std::coroutine_handle<>> m_continuations;
};

class single_continuation
{
public:
    single_continuation() = default;
    single_continuation(const single_continuation&) = delete;
    single_continuation& operator=(const single_continuation&) = delete;

    void push(std::coroutine_handle<> handle)
    {
        m_continuation = handle;
    }
    void resume_one()
    {
        if (m_continuation) {
            auto h = *m_continuation;
            m_continuation.reset();
            h.resume();
        }
    }
    void resume_all()
    {
        resume_one();
    }

    void erase(std::coroutine_handle<> handle)
    {
        if (m_continuation && m_continuation->address() == handle.address()) {
            m_continuation.reset();
        }
    }

private:
    std::optional<std::coroutine_handle<>> m_continuation;
};

}