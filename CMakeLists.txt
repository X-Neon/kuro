cmake_minimum_required(VERSION 3.0)
project(Kuro)

find_package(Catch2 CONFIG REQUIRED)

add_executable(
    kuro_test 
    test/gather.cpp
    test/shared_task.cpp
    test/task.cpp
    test/with_cancellation.cpp
)

target_include_directories(kuro_test PRIVATE include)
target_link_libraries(kuro_test PRIVATE Catch2::Catch2WithMain)
target_compile_features(kuro_test PRIVATE cxx_std_20)
target_compile_options(kuro_test PRIVATE -fcoroutines)

enable_testing()
add_test(NAME kuro_test COMMAND kuro_test)