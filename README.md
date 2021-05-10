<p align="center">
    <img height=120 src="img/logo.png"/>  
</p>

# kuro

A C++20 coroutine library, somewhat modelled on Python's `asyncio`

## Requirements

Kuro requires a C++20 compliant compiler and a Linux OS. Tested on GCC 10.2 / Ubuntu 20.04.

## Example

Start a TCP server, and add all the integers received until the user stops the program.

```cpp
#include <iostream>
#include <kuro/kuro.hpp>

kuro::cancellation cancel;
kuro::queue<int> sum_queue;

void stop_server()
{
    std::cout << "Stopping server" << std::endl;
    cancel.trigger();
}

kuro::task<void> handle_connection(kuro::ipv4_socket sock)
{
    char msg[100];
    std::optional<ssize_t> n_bytes_recv = co_await kuro::with_timeout(
        sock.recv(msg, 100), 
        std::chrono::seconds(10)
    );

    if (n_bytes_recv) {
        std::string_view reply = "Message received";
        co_await sock.send(reply.data(), reply.size());

        int value = std::stoi(std::string(msg, *n_bytes_recv));
        sum_queue.push(value);
    } else {
        std::cout << "Client timed out" << std::endl;
    }
}

kuro::task<int> sum()
{
    int total = 0;
    while (true) {
        std::optional<int> value = co_await kuro::with_cancellation(
            sum_queue.pop(),
            cancel
        );

        if (!value) {
            co_return total;
        }
        total += *value;
    }
}

kuro::task<void> app_main()
{
    kuro::event_loop::add_signal_handler(SIGINT, stop_server);

    auto sum_task = kuro::event_loop::create_task(sum());

    auto sock = kuro::tcpv4_listen_socket(8001);
    co_await sock.serve_forever(handle_connection, cancel);

    std::cout << "Server stopped" << std::endl;
    std::cout << "Sum is: " << co_await sum_task << std::endl;
}

int main()
{
    kuro::event_loop::run(app_main());
}
```

## Disclaimer

This code is super-alpha quality. It's barely tested beyond writing simple examples, and is basically a hastily thrown together mess. If you want to get started with C++20 coroutines, you'll be better off using [cppcoro](https://github.com/lewissbaker/cppcoro), or waiting until [libunifex](https://github.com/facebookexperimental/libunifex) is more mature. Still, coroutines are a very complex and flexible language feature, and I hope looking at my code will help you understand how to use and implement coroutines in your own code.