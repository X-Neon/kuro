#pragma once

#include <coroutine>
#include <memory>

namespace kuro::detail
{

template <typename Promise>
class unique_coroutine_handle
{
public:
    unique_coroutine_handle() noexcept = default;
    unique_coroutine_handle(std::coroutine_handle<Promise> h) : m_ptr(h.address()) {}
    bool done() const
    {
        return handle().done();
    }
    Promise& promise() const
    {
        return handle().promise();
    }
    void resume() const
    {
        handle().resume();
    }
    operator std::coroutine_handle<Promise>() const
    {
        return handle();
    }

private:
    std::coroutine_handle<Promise> handle() const noexcept
    {
        return std::coroutine_handle<Promise>::from_address(m_ptr.get());
    }

    struct destroyer
    {
        constexpr destroyer() noexcept = default;
        void operator()(void* ptr)
        {
            std::coroutine_handle<Promise>::from_address(ptr).destroy();
        }
    };

    std::unique_ptr<void, destroyer> m_ptr;
};

}