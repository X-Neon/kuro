#pragma once

#include <coroutine>
#include <functional>
#include <unordered_map>
#include <vector>

#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "cancellation.hpp"
#include "promise.hpp"
#include "shared_task.hpp"
#include "task.hpp"
#include "task_executor.hpp"
#include "unique_fd.hpp"
#include "util.hpp"

namespace kuro
{

class event_loop
{
    template <typename T>
    class active_task
    {
    public:
        class promise_type final : public detail::base_promise_t<T>
        {
        public:
            active_task<T> get_return_object() noexcept
            {
                return active_task(std::coroutine_handle<promise_type>::from_promise(*this));
            }
            std::suspend_never initial_suspend() const noexcept { return {}; }
            auto final_suspend() const noexcept
            {
                struct awaitable
                {
                    bool await_ready() noexcept { return false; }
                    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> handle) noexcept
                    {
                        auto continuation = handle.promise().m_continuation;
                        if (continuation) {
                            return continuation;
                        }

                        return std::noop_coroutine();
                    }
                    void await_resume() noexcept {}
                };
                return awaitable{};
            }
            void set_continuation(std::coroutine_handle<> continuation)
            {
                m_continuation = continuation;
            }

        private:
            std::coroutine_handle<> m_continuation;
        };

        T await_resume()
        {
            return m_handle.promise().result();
        }
        bool await_ready() const noexcept
        {
            return m_handle.done();
        }
        void await_suspend(std::coroutine_handle<> parent_handle) noexcept
        {
            m_handle.promise().set_continuation(parent_handle);
        }
        bool done() const noexcept
        {
            return m_handle.done();
        }


    private:
        active_task(std::coroutine_handle<promise_type> handle) : m_handle(handle) {}

        detail::unique_coroutine_handle<promise_type> m_handle;
    };

public:
    template <typename T>
    static T run(task<T> root_task)
    {
        auto exec = [](task<T>& t) -> detail::task_executor_return<T> {
            co_return co_await t;
        }(root_task);

        io_loop(root_task);

        return exec.m_handle.promise().result();
    }
    static void add_reader(int fd, std::coroutine_handle<> handle)
    {
        epoll_event ev{.events=EPOLLIN, .data={.fd=fd}};
        epoll_ctl(instance().m_epoll_fd.get(), EPOLL_CTL_ADD, fd, &ev);
        instance().m_handles[fd] = handle;
    }
    static void add_writer(int fd, std::coroutine_handle<> handle)
    {
        epoll_event ev{.events=EPOLLOUT, .data={.fd=fd}};
        epoll_ctl(instance().m_epoll_fd.get(), EPOLL_CTL_ADD, fd, &ev);
        instance().m_handles[fd] = handle;
    }
    static void remove_fd(int fd)
    {
        instance().m_handles.erase(fd);
        epoll_ctl(instance().m_epoll_fd.get(), EPOLL_CTL_DEL, fd, nullptr);
    }
    static void add_signal_handler(int signal, std::function<void()> handler)
    {
        detail::check_error(sigaddset(&instance().m_sigmask, signal));
        detail::check_error(sigprocmask(SIG_SETMASK, &instance().m_sigmask, nullptr));
        detail::check_error(signalfd(instance().m_signal_fd.get(), &instance().m_sigmask, 0));
        instance().m_signal_handlers[signal] = std::move(handler);
    }
    static void remove_signal_handler(int signal)
    {
        detail::check_error(sigdelset(&instance().m_sigmask, signal));
        detail::check_error(sigprocmask(SIG_SETMASK, &instance().m_sigmask, nullptr));
        detail::check_error(signalfd(instance().m_signal_fd.get(), &instance().m_sigmask, 0));
        instance().m_signal_handlers.erase(signal);
    }

    template <typename T>
    static auto create_task(task<T> concurrent_task)
    {
        return [](task<T> t) -> active_task<T> {
            co_return co_await t;
        }(std::move(concurrent_task));
    }

    template <typename T>
    static auto create_task(shared_task<T> concurrent_task)
    {
        [](shared_task<T> t) -> detail::task_executor {
            co_await t;
        }(concurrent_task);

        return concurrent_task;
    }

private:
    event_loop()
    {
        m_epoll_fd = epoll_create1(0);
        m_events.resize(32);
        detail::check_error(sigemptyset(&m_sigmask));
        m_signal_fd = detail::check_fd(signalfd(-1, &m_sigmask, 0));
        epoll_event ev{.events=EPOLLIN, .data={.fd=m_signal_fd.get()}};
        epoll_ctl(m_epoll_fd.get(), EPOLL_CTL_ADD, m_signal_fd.get(), &ev);
    }

    template <typename T>
    static void io_loop(const task<T>& root_task)
    {
        while (!root_task.done()) {
            int n_ev = epoll_wait(
                instance().m_epoll_fd.get(),
                instance().m_events.data(),
                instance().m_events.size(),
                -1
            );
            for (int i = 0; i < n_ev; ++i) {
                int fd = instance().m_events[i].data.fd;
                if (fd == instance().m_signal_fd.get()) {
                    signalfd_siginfo info;
                    if (read(fd, &info, sizeof(info)) != sizeof(info)) {
                        throw std::runtime_error("Failed to read signal handler info");
                    }

                    instance().m_signal_handlers[info.ssi_signo]();
                } else {
                    auto h = instance().m_handles[fd];
                    remove_fd(fd);
                    h.resume();
                }
            }
        }
    }

    static event_loop& instance()
    {
        static event_loop inst;
        return inst;
    }

    detail::unique_fd m_epoll_fd;
    std::vector<epoll_event> m_events;
    std::unordered_map<int, std::coroutine_handle<>> m_handles;
    sigset_t m_sigmask;
    detail::unique_fd m_signal_fd;
    std::unordered_map<int, std::function<void()>> m_signal_handlers;
};

}