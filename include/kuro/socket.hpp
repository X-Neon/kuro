#pragma once

#include <coroutine>
#include <optional>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "address.hpp"
#include "cancellation.hpp"
#include "error.hpp"
#include "event_loop.hpp"
#include "task.hpp"
#include "task_executor.hpp"
#include "unique_fd.hpp"
#include "with_cancellation.hpp"

namespace kuro
{

class listen_socket;

class socket
{
    friend listen_socket;

protected:
    template <typename T>
    auto base_connect(T addr)
    {
        struct connect_awaitable
        {
            connect_awaitable(T addr, int fd) : m_addr(addr), m_fd(fd) {}
            bool await_ready()
            {
                int ret = ::connect(m_fd, reinterpret_cast<sockaddr*>(&m_addr), sizeof(m_addr));
                if (ret == 0) {
                    m_success = true;
                    return true;
                } else if (errno == EINPROGRESS) {
                    return false;
                } else {
                    throw std::system_error(errno, std::system_category());
                }
            }
            void await_suspend(std::coroutine_handle<> handle) const
            {
                event_loop::add_writer(m_fd, handle);
            }
            int await_resume() const
            {
                if (m_success) {
                    return 0;
                }

                int error_code;
                socklen_t size = sizeof(int);
                detail::check_error(getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &error_code, &size));
                return error_code;
            }
            void await_cancel() const
            {
                event_loop::remove_fd(m_fd);
            }

        private:
            T m_addr;
            int m_fd;
            bool m_success = false;
        };

        return connect_awaitable(addr, m_fd.get());
    }

    template <typename T>
    auto base_sendto(const void* buf, size_t len, T addr)
    {
        struct sendto_awaitable
        {
            sendto_awaitable(int fd, const void* buf, size_t len, T addr) : m_fd(fd), m_buf(buf), m_len(len), m_addr(addr) {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> handle) const
            {
                event_loop::add_writer(m_fd, handle);
            }
            ssize_t await_resume()
            {
                return ::sendto(m_fd, m_buf, m_len, 0, reinterpret_cast<sockaddr*>(&m_addr), sizeof(m_addr));
            }
            void await_cancel() const
            {
                event_loop::remove_fd(m_fd);
            }

        private:
            int m_fd;
            const void* m_buf;
            size_t m_len;
            T m_addr;
        };

        return sendto_awaitable(m_fd.get(), buf, len, addr);
    }

    template <typename T>
    auto base_recvfrom(void* buf, size_t len)
    {
        struct recvfrom_awaitable
        {
            recvfrom_awaitable(int fd, void* buf, size_t len) : m_fd(fd), m_buf(buf), m_len(len) {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> handle) const
            {
                event_loop::add_reader(m_fd, handle);
            }
            auto await_resume()
            {
                T s_addr;
                socklen_t len = sizeof(T);
                auto n_bytes = ::recvfrom(m_fd, m_buf, m_len, 0, reinterpret_cast<sockaddr*>(&s_addr), &len);
                return std::pair(n_bytes, detail::from_sockaddr(s_addr));
            }
            void await_cancel() const
            {
                event_loop::remove_fd(m_fd);
            }

        private:
            int m_fd;
            void* m_buf;
            size_t m_len;
        };

        return recvfrom_awaitable(m_fd.get(), buf, len);
    }

    template <typename T>
    void bind(T addr)
    {
        detail::check_error(::bind(m_fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));
    }

public:
    auto recv(void* buf, size_t len)
    {
        struct recv_awaitable
        {
            recv_awaitable(int fd, void* buf, size_t len) : m_fd(fd), m_buf(buf), m_len(len) {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> handle) const
            {
                event_loop::add_reader(m_fd, handle);
            }
            ssize_t await_resume()
            {
                return ::recv(m_fd, m_buf, m_len, 0);
            }
            void await_cancel() const
            {
                event_loop::remove_fd(m_fd);
            }

        private:
            int m_fd;
            void* m_buf;
            size_t m_len;
        };

        return recv_awaitable(m_fd.get(), buf, len);
    }

    auto recv(iovec* iov, size_t iovlen) 
    {
        struct recv_awaitable
        {
            recv_awaitable(int fd, iovec* iov, size_t len) : m_fd(fd), m_iov(iov), m_len(len) {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> handle) const
            {
                event_loop::add_reader(m_fd, handle);
            }
            ssize_t await_resume()
            {
                msghdr hdr{nullptr, 0, m_iov, m_len, nullptr, 0, 0};
                return recvmsg(m_fd, &hdr, 0);
            }
            void await_cancel() const
            {
                event_loop::remove_fd(m_fd);
            }

        private:
            int m_fd;
            iovec* m_iov;
            size_t m_len;
        };

        return recv_awaitable(m_fd.get(), iov, iovlen);
    }

    auto send(const void* buf, size_t len)
    {
        struct send_awaitable
        {
            send_awaitable(int fd, const void* buf, size_t len) : m_fd(fd), m_buf(buf), m_len(len) {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> handle) const
            {
                event_loop::add_writer(m_fd, handle);
            }
            ssize_t await_resume()
            {
                return ::send(m_fd, m_buf, m_len, 0);
            }
            void await_cancel() const
            {
                event_loop::remove_fd(m_fd);
            }

        private:
            int m_fd;
            const void* m_buf;
            size_t m_len;
        };

        return send_awaitable(m_fd.get(), buf, len);
    }

    auto send(iovec* iov, size_t iovlen) 
    {
        struct send_awaitable
        {
            send_awaitable(int fd, iovec* iov, size_t len) : m_fd(fd), m_iov(iov), m_len(len) {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> handle) const
            {
                event_loop::add_writer(m_fd, handle);
            }
            ssize_t await_resume()
            {
                msghdr hdr{nullptr, 0, m_iov, m_len, nullptr, 0, 0};
                return sendmsg(m_fd, &hdr, 0);
            }
            void await_cancel() const
            {
                event_loop::remove_fd(m_fd);
            }

        private:
            int m_fd;
            iovec* m_iov;
            size_t m_len;
        };

        return send_awaitable(m_fd.get(), iov, iovlen);
    }

protected:
    socket(int fd) : m_fd(fd) {}
    socket(int family, int type)
    {
        m_fd = detail::check_fd(::socket(family, type | SOCK_NONBLOCK, 0));
    }

private:
    detail::unique_fd m_fd;
};

class ipv4_socket : public socket
{
    friend listen_socket;

public:
    static auto tcp() { return ipv4_socket(SOCK_STREAM); }
    static auto udp() { return ipv4_socket(SOCK_DGRAM); }
    static auto bound_udp(ipv4_address addr, uint16_t port)
    {
        ipv4_socket sock(SOCK_DGRAM);
        sock.bind(detail::to_sockaddr(addr, port));
        return sock;
    }
    static auto bound_udp(uint16_t port) { return bound_udp(ipv4_address::any(), port); }
    auto connect(ipv4_address addr, uint16_t port) { return socket::base_connect(detail::to_sockaddr(addr, port)); }
    auto recvfrom(void* buf, size_t len) { return socket::base_recvfrom<sockaddr_in>(buf, len); }
    auto sendto(const void* buf, size_t len, ipv4_address addr, uint16_t port)
    {
        return socket::base_sendto(buf, len, detail::to_sockaddr(addr, port));
    }

private:
    ipv4_socket(int type) : socket(AF_INET, type) {}
    ipv4_socket(socket&& sock) : socket(std::move(sock)) {}
};

class ipv6_socket : public socket
{
    friend listen_socket;

public:
    static auto tcp() { return ipv6_socket(SOCK_STREAM); }
    static auto udp() { return ipv6_socket(SOCK_DGRAM); }
    static auto bound_udp(ipv6_address addr, uint16_t port)
    {
        ipv6_socket sock(SOCK_DGRAM);
        sock.bind(detail::to_sockaddr(addr, port));
        return sock;
    }
    static auto bound_udp(uint16_t port) { return bound_udp(ipv6_address::any(), port); }
    auto connect(ipv6_address addr, uint16_t port) { return socket::base_connect(detail::to_sockaddr(addr, port)); }
    auto recvfrom(void* buf, size_t len) { return socket::base_recvfrom<sockaddr_in6>(buf, len); }
    auto sendto(const void* buf, size_t len, ipv6_address addr, uint16_t port)
    {
        return socket::base_sendto(buf, len, detail::to_sockaddr(addr, port));
    }

private:
    ipv6_socket(int type) : socket(AF_INET6, type) {}
    ipv6_socket(socket&& sock) : socket(std::move(sock)) {}
};

class unix_socket : public socket
{
    friend listen_socket;

public:
    static auto stream() { return unix_socket(SOCK_STREAM); }
    static auto dgram() { return unix_socket(SOCK_DGRAM); }
    static auto bound_stream(std::string_view unix_addr)
    {
        unix_socket sock(SOCK_STREAM);
        sock.bind(detail::to_sockaddr(unix_addr));
        return sock;
    }
    static auto bound_dgram(std::string_view unix_addr)
    {
        unix_socket sock(SOCK_DGRAM);
        sock.bind(detail::to_sockaddr(unix_addr));
        return sock;
    }
    auto connect(std::string_view unix_addr) { return socket::base_connect(detail::to_sockaddr(unix_addr)); }
    auto recvfrom(void* buf, size_t len) { return socket::base_recvfrom<sockaddr_un>(buf, len); }
    auto sendto(const void* buf, size_t len, std::string_view unix_addr)
    {
        return socket::base_sendto(buf, len, detail::to_sockaddr(unix_addr));
    }

private:
    unix_socket(int type) : socket(AF_UNIX, type) {}
    unix_socket(socket&& sock) : socket(std::move(sock)) {}
};

class listen_socket
{
protected:
    template <typename T>
    auto base_accept()
    {
        struct accept_awaitable
        {
            accept_awaitable(int fd) : m_fd(fd) {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> handle) const
            {
                event_loop::add_reader(m_fd, handle);
            }
            T await_resume()
            {
                int fd = detail::check_fd(::accept(m_fd, nullptr, nullptr));
                return T(socket(fd));
            }
            void await_cancel() const
            {
                event_loop::remove_fd(m_fd);
            }

        private:
            int m_fd;
        };

        return accept_awaitable(m_fd.get());
    }

public:
    auto accept()
    {
        return base_accept<socket>();
    }

    template <typename Func>
    task<void> serve_forever(Func&& callback)
    {
        while (true) {
            auto sock = co_await accept();

            [](socket s, auto&& f) -> detail::task_executor {
                co_await f(std::move(s));
            }(std::move(sock), std::forward<Func>(callback));
        }
    }

    template <typename Func>
    task<void> serve_forever(Func&& callback, cancellation& cancel)
    {
        while (true) {
            auto sock = co_await with_cancellation(accept(), cancel);
            if (!sock) {
                co_return;
            }

            [](socket s, auto&& f) -> detail::task_executor {
                co_await f(std::move(s));
            }(std::move(*sock), std::forward<Func>(callback));
        }
    }

protected:
    listen_socket(int family) : m_fd(detail::check_fd(::socket(family, SOCK_STREAM | SOCK_NONBLOCK, 0))) {}

    template <typename T>
    void bind(T addr)
    {
        detail::check_error(::bind(m_fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));
    }

    void listen(int queued_connections)
    {
        detail::check_error(::listen(m_fd.get(), queued_connections));
    }

private:
    detail::unique_fd m_fd;
};

class tcpv4_listen_socket : public listen_socket
{
public:
    tcpv4_listen_socket(ipv4_address addr, uint16_t port, int queued_connections = 1) : listen_socket(AF_INET)
    {
        listen_socket::bind(detail::to_sockaddr(addr, port));
        listen_socket::listen(queued_connections);
    }
    tcpv4_listen_socket(uint16_t port, int queued_connections = 1) : tcpv4_listen_socket(ipv4_address::any(), port, queued_connections) {}
    tcpv4_listen_socket(listen_socket&& socket) : listen_socket(std::move(socket)) {}
    auto accept() { return listen_socket::base_accept<ipv4_socket>(); }
};

class tcpv6_listen_socket : public listen_socket
{
public:
    tcpv6_listen_socket(ipv6_address addr, uint16_t port, int queued_connections = 1) : listen_socket(AF_INET6)
    {
        listen_socket::bind(detail::to_sockaddr(addr, port));
        listen_socket::listen(queued_connections);
    }
    tcpv6_listen_socket(uint16_t port, int queued_connections = 1) : tcpv6_listen_socket(ipv6_address::any(), port, queued_connections) {}
    tcpv6_listen_socket(listen_socket&& socket) : listen_socket(std::move(socket)) {}
    auto accept() { return listen_socket::base_accept<ipv4_socket>(); }
};

class unix_listen_socket : public listen_socket
{
public:
    unix_listen_socket(std::string_view unix_addr, int queued_connections = 1) : listen_socket(AF_UNIX)
    {
        listen_socket::bind(detail::to_sockaddr(unix_addr));
        listen_socket::listen(queued_connections);
    }
    unix_listen_socket(listen_socket&& socket) : listen_socket(std::move(socket)) {}
    auto accept() { return listen_socket::base_accept<unix_socket>(); }
};


}