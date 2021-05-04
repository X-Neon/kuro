# General

* kuro is not thread-safe. The kuro API must be used from a single thread.
* All kuro awaitable operations are cancellable, with the exception of awaiting a `task` or `shared_task`

# Task

## task

```cpp
template <typename T>
class task
{
    bool done() const;
    co_await operator co_await() -> T;
};
```

A lazy task type. Only starts when awaited. Can only be awaited by a single coroutine.

## shared_task

```cpp
template <typename T>
class shared_task
{
    bool done() const;
    co_await operator co_await() -> T&;
};
```

A lazy task type. Only starts when awaited. Can be awaited by multiple coroutines simultaneously.

# Event Loop

```cpp
T run(task<T> base_task);
T run(shared_task<T> base_task);
```

Execute a task and block until its completion. Note this does not return an awaitable - this is the entry point for coroutine execution.

```cpp
co_await create_task(task<T> concurrent_task) -> active_task<T>;
co_await create_task(shared_task<T> concurrent_task) -> shared_task<T>;
```

Execute a task concurrently with all other coroutines. Returns a task type that can be awaited to block until completion.

```cpp
void add_signal_handler(int signal, std::function<void()> handler);
void remove_signal_handler(int signal);
```

Add/remove a signal handler that will be executed on receipt of a given signal.

```cpp
void add_reader(int fd, std::coroutine_handle<> handle);
void add_writer(int fd, std::coroutine_handle<> handle);
void remove_fd(int fd);
```

Add/remove a file descriptor that will trigger the continuation of a coroutine.

# IO

## sleep_for

```cpp
co_await sleep_for(duration d) -> void;
```

Sleep for a given duration.

## socket

```cpp
class socket
{
    co_await recv(void* buffer, size_t length) -> ssize_t;
    co_await recv(iovec* iov, size_t iovlen) -> ssize_t;
    co_await send(void* buffer, size_t length) -> ssize_t;
    co_await send(iovec* iov, size_t iovlen) -> ssize_t;
};

class ipv4_socket : public socket
{
    static ipv4_socket tcp();
    static ipv4_socket udp();
    static ipv4_socket bound_udp(ipv4_address addr, uint16_t port);
    static ipv4_socket bound_udp(uint16_t port);

    co_await connect(ipv4_address addr, uint16_t port) -> int;
    co_await recvfrom(void* buffer, size_t length) -> std::pair<ssize_t, std::pair<ipv4_address, uint16_t>>;
    co_await sendto(void* buffer, size_t length, ipv4_address addr, uint16_t port) -> ssize_t;
};

class ipv6_socket : public socket
{
    static ipv6_socket tcp();
    static ipv6_socket udp();
    static ipv6_socket bound_udp(ipv6_address addr, uint16_t port);
    static ipv6_socket bound_udp(uint16_t port);

    co_await connect(ipv6_address addr, uint16_t port) -> int;
    co_await recvfrom(void* buffer, size_t length) -> std::pair<ssize_t, std::pair<ipv6_address, uint16_t>>;
    co_await sendto(void* buffer, size_t length, ipv6_address addr, uint16_t port) -> ssize_t;
};

class unix_socket : public socket
{
    static unix_socket stream();
    static unix_socket dgram();
    static unix_socket bound_stream(std::string_view unix_addr);
    static unix_socket bound_dgram(std::string_view unix_addr);

    co_await connect(std::string_view unix_addr) -> int;
    co_await recvfrom(void* buffer, size_t length) -> std::pair<ssize_t, std::string_view>;
    co_await sendto(void* buffer, size_t length, std::string_view unix_addr) -> ssize_t;
};
```

Socket types, representing either a connectionless socket (e.g. UDP) or a connected socket (e.g. a TCP connection). The API largely follows the Linux socket API.

## listen_socket

```cpp
class listen_socket
{
    co_await accept() -> socket;
    co_await serve_forever(Func&& callback) -> void;
    co_await serve_forever(Func&& callback, cancellation& cancel) -> void;
};

class tcpv4_listen_socket : public listen_socket
{
    tcpv4_listen_socket(ipv4_address addr, uint16_t port, int queued_connections = 1);
    tcpv4_listen_socket(uint16_t port, int queued_connections = 1);
    co_await accept() -> ipv4_socket;
};

class tcpv6_listen_socket : public listen_socket
{
    tcpv6_listen_socket(ipv6_address addr, uint16_t port, int queued_connections = 1);
    tcpv6_listen_socket(uint16_t port, int queued_connections = 1);
    co_await accept() -> ipv6_socket;
};

class unix_listen_socket : public listen_socket
{
    unix_listen_socket(std::string_view unix_addr, int queued_connections = 1);
    co_await accept() -> unix_socket;
};
```

Socket types, representing a listen socket on a connection-based protocol (e.g. a TCP server). 

# Synchronization

## continuation_container

```cpp
class multi_continuation;
class single_continuation;
```

Most of the synchronization types accept a template parameter for the underlying continuation container. This determines whether they can be awaited by multiple coroutines simultaneously (`multi_continuation`) or only a single coroutine (`single_continuation`). Each awaiter of a `multi_continuation` grows a `std::vector`, whereas a `single_continuation` only needs to store a single coroutine handle, which is potentially more performant.

## cancellation

```cpp
class cancellation
{
    void trigger();
    bool is_set() const;
    co_await wait() -> void;
};
```

A single-use cancellation source. Awaiting `wait()` suspends the current coroutine until the `cancellation` is triggered.

## event

```cpp
template <continuation_container Continuation = multi_continuation>
class event
{
    void set();
    bool is_set() const;
    co_await wait() -> void;
};
```

A single-use event. Awaiting `wait()` suspends the current coroutine until the `event` is triggered.

## mutex

```cpp
template <continuation_container Continuation = multi_continuation>
class mutex
{
    bool is_locked() const;
    co_await acquire() -> locked_mutex;
};
```

An asynchronous mutex. Awaiting `acquire()` suspends the current coroutines until the mutex can be acquired. The mutex is released when the `locked_mutex` is destroyed.

## queuelike

```cpp
template <typename Container, continuation_container Continuation = multi_continuation>
class queuelike
{
    using T = Container::value_type;

    void push(const T& item);
    void push(T&& item);
    bool empty() const;
    co_await pop() -> T;
};

template <std::movable T>
using queue = queuelike<std::queue<T>>;

template <std::movable T>
using stack = queuelike<std::stack<T>>;

template <std::movable T>
using priority_queue = queuelike<std::priority_queue<T>>;
```

Queue-like containers. Awaiting `pop()` suspends the current coroutine until an item becomes available in the container.

# Other

## gather

```cpp
template <awaitable... Awaitable>
co_await gather(Awaitable&& await) -> std::tuple<decltype(co_await await)...>;
```

Await all the awaitables concurrently, and suspend until all the awaitables are complete. Returns a tuple of the results of awaiting the individual awaitables. Awaitables that would return `void` are replaced by `kuro::void_t`. `gather` is cancellable if all of the input awaitables are cancellable.

## with_cancellation / with_timeout

```cpp
template <awaitable... Awaitable>
co_await with_cancellation(Awaitable&& await, cancellation& cancel) -> std::optional<decltype(co_await await)>;

template <awaitable... Awaitable>
co_await with_timeout(Awaitable&& await, duration d) -> std::optional<decltype(co_await await)>;
```

Await the awaitable until either the the awaitable completes, or the cancellation/timeout is triggered. If the awaitable would return `void`, the inner type of the `std::optional` is replaced by `kuro::void_t`.