#include "catch.hpp"
#include "kuro/kuro.hpp"

static kuro::queue<int> queue;
static kuro::cancellation cancel;

static kuro::task<void> fill_queue()
{
    co_await kuro::sleep_for(std::chrono::milliseconds(1));
    queue.push(10);
}

static kuro::task<std::pair<std::optional<int>, std::optional<int>>> read_queue()
{
    auto fill_task = kuro::event_loop::create_task(fill_queue());
    auto first_attempt = co_await kuro::with_timeout(queue.pop(), std::chrono::nanoseconds(1));
    auto second_attempt = co_await kuro::with_cancellation(queue.pop(), cancel);
    co_await fill_task;
    co_return {first_attempt, second_attempt};
}

TEST_CASE("Cancellation")
{
    auto read_attempts = kuro::event_loop::run(read_queue());
    REQUIRE(!read_attempts.first);
    REQUIRE(read_attempts.second);
    REQUIRE(*read_attempts.second == 10);
}