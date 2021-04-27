#include <memory>

#include "catch.hpp"
#include "kuro/kuro.hpp"

static kuro::task<int> return_10()
{
    co_return 10;
}

static kuro::task<std::unique_ptr<int>> return_unique_ptr()
{
    co_return std::unique_ptr<int>(new int(20));
}

static int global = 30;

static kuro::task<int&> return_ref()
{
    co_return global;
}

static kuro::task<void> return_void()
{
    co_return;
}

static kuro::task<int> sleep_and_return()
{
    co_await kuro::sleep_for(std::chrono::nanoseconds(1));
    co_return 40;
}

static kuro::task<std::tuple<int, std::unique_ptr<int>, int&, kuro::void_t, int, kuro::void_t>> gather()
{
    auto t = sleep_and_return();
    co_return co_await kuro::gather(
        return_10(),
        return_unique_ptr(),
        return_ref(),
        return_void(),
        std::move(t),
        kuro::sleep_for(std::chrono::nanoseconds(10))
    );
}

TEST_CASE("Gather")
{
    auto&& [a, b, c, d, e, f] = kuro::event_loop::run(gather());
    REQUIRE(a == 10);
    REQUIRE(*b == 20);
    REQUIRE(&c == &global);
    REQUIRE(e == 40);
}