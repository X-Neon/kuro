project(
    'kuro',
    'cpp',
    default_options : ['cpp_std=c++20']
)

add_global_arguments('-fcoroutines', language : 'cpp')
include = include_directories('include')

test_sources = files(
    'test/gather.cpp',
    'test/main.cpp',
    'test/shared_task.cpp',
    'test/task.cpp'
)

kuro_tests = executable(
    'kuro_tests', 
    test_sources,
    include_directories : include
)

test('kuro', kuro_tests)