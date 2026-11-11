cmake_minimum_required(VERSION 4.2)
project(Concal LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# Package Requirements GTest and nlohmann_json
find_package(GTest CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

option(CONCAL_TSAN "Enable ThreadSanitizer" OFF)
if (CONCAL_TSAN)
    add_compile_options(-fsanitize=thread -g)
    add_link_options(-fsanitize=thread)
endif()


add_library(concal STATIC
        src/runtime/scheduler.cpp
        src/runtime/schedulerpolicy.cpp
        src/runtime/task.cpp
        src/runtime/worker.cpp
        src/tracing/tracer.cpp)

target_include_directories(concal PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(concal PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /Zc:__cplusplus /wd4324>   # 4324 = alignas padding note
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>)


target_link_libraries(concal
        PRIVATE
        nlohmann_json::nlohmann_json
        Threads::Threads
)

#add_executable(runtime_tests
#        tests/integration/invoke_test.cpp
#        tests/integration/dag_execution_test.cpp
#        tests/stress/massive_dag_test.cpp
#        tests/unit/concurrency/deque_tests.cpp)
#
#target_link_libraries(runtime_tests PRIVATE concal GTest::gtest_main)
#include(GoogleTest)
#gtest_discover_tests(
#        runtime_tests
#        PROPERTIES TIMEOUT 15
#)


option(CONCAL_BUILD_TESTS "Build test" ON)
if (CONCAL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

