project(
    'kuro',
    'cpp',
    version : '0.1',
    default_options : ['cpp_std=c++20']
)

install_subdir('include/kuro', install_dir : 'include')

if get_option('build-tests')
    add_global_arguments('-fcoroutines', language : 'cpp')

    include = include_directories('include')
    catch_include = include_directories('third_party/catch2')

    test_sources = files(
        'test/gather.cpp',
        'test/main.cpp',
        'test/shared_task.cpp',
        'test/task.cpp',
        'test/with_cancellation.cpp'
    )

    kuro_tests = executable(
        'kuro_tests', 
        test_sources,
        include_directories : [catch_include, include]
    )

    test('kuro', kuro_tests)
endif