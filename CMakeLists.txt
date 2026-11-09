cmake_minimum_required(VERSION 4.2)
project(Concal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


#include(FetchContent)
#FetchContent_Declare(
#        googletest
#        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
#)
find_package(GTest REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
enable_testing()
add_executable(runtime_tests src/runtime/scheduler.cpp
        src/runtime/task.cpp
        src/concurrency/deque.h++
        src/runtime/worker.h++
        include/pure/task.h++
        include/pure/scheduler.h++
        src/tracing/tracer.h++
        src/tracing/tracer.cpp
        include/pure/func.h++
        src/runtime/schedulerpolicy.h++
        src/runtime/worker.cpp
        include/pure/core/id.h++
        include/pure/runtime/runtime.h++
        tests/integration/invoke_test.cpp
        src/runtime/schedulerpolicy.cpp
        include/pure/core/outputslot.h++
        src/tool/dual.h++
        include/pure/core/configuration.h++
)


target_include_directories(runtime_tests
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
        /opt/homebrew/include
)

target_link_directories(runtime_tests PRIVATE /opt/homebrew/lib)

target_link_libraries(runtime_tests
        PRIVATE
        gtest
        gtest_main
        nlohmann_json
)

include(GoogleTest)
gtest_discover_tests(
        runtime_tests
        PROPERTIES TIMEOUT 15
)
