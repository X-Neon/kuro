project(
    'kuro',
    'cpp',
    default_options : ['cpp_std=c++20']
)

add_global_arguments('-fcoroutines', language : 'cpp')
include = include_directories('include')

test_sources = files(
    'test/main.cpp',
    'test/task.cpp'
)

test_exe = executable(
    'kuro_test', 
    test_sources,
    include_directories : include
)

test('test name', test_exe)