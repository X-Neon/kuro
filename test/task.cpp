#include <memory>

#include "catch.hpp"
#include "kuro/kuro.hpp"

kuro::task<int> return_10()
{
    co_return 10;
}

kuro::task<std::unique_ptr<int>> return_unique_ptr()
{
    co_return std::unique_ptr<int>(new int(20));
}

TEST_CASE("Task value type", "[task]")
{
    REQUIRE(kuro::event_loop::run(return_10()) == 10);

    auto ptr = kuro::event_loop::run(return_unique_ptr());
    REQUIRE(*ptr == 20);
}