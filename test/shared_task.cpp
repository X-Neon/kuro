#include <memory>

#include "catch.hpp"
#include "kuro/kuro.hpp"

static kuro::shared_task<int> return_10()
{
    co_return 10;
}

static kuro::shared_task<std::unique_ptr<int>> return_unique_ptr()
{
    co_return std::unique_ptr<int>(new int(20));
}

static kuro::shared_task<int> return_nested()
{
    co_return co_await return_10();
}

static int global = 30;

static kuro::shared_task<int&> return_ref()
{
    co_return global;
}

static kuro::shared_task<void> modify_global_return_void()
{
    global = 50;
    co_return;
}

static kuro::shared_task<int> increment_global_return_value()
{
    int val = global;
    global += 10;
    co_return val;
}

static kuro::task<int> await_shared(kuro::shared_task<int>& shared)
{
    co_return co_await shared;    
}

static kuro::task<std::pair<int, int>> await_shared_many()
{
    auto shared = increment_global_return_value();
    auto handle = await_shared(shared);
    co_return {co_await shared, co_await handle};
}

TEST_CASE("Shared task")
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

    auto int_pair = kuro::event_loop::run(await_shared_many());
    REQUIRE(int_pair.first == int_pair.second);
    REQUIRE(int_pair.first == 50);
    REQUIRE(global == 60);
}