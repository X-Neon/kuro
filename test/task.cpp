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

static kuro::task<int> return_nested()
{
    co_return co_await return_10();
}

static int global = 30;

static kuro::task<int&> return_ref()
{
    co_return global;
}

static kuro::task<void> modify_global_return_void()
{
    global = 50;
    co_return;
}

TEST_CASE("task")
{
    REQUIRE(kuro::event_loop::run(return_10()) == 10);

    auto ptr = kuro::event_loop::run(return_unique_ptr());
    REQUIRE(*ptr == 20);

    REQUIRE(kuro::event_loop::run(return_nested()) == 10);

    int& global_ref = kuro::event_loop::run(return_ref());
    REQUIRE(global_ref == 30);
    global_ref = 40;
    REQUIRE(global == 40);

    kuro::event_loop::run(modify_global_return_void());
    REQUIRE(global == 50);
}