// #include <gtest/gtest.h>
#include <fstream>

#include "math_helper.h++"
#include "runtime_fixture.h++"
#include "pure/runtime/runtime.h++"
#include "tool/dual.h++"

double _square(double x) { return x * x; }


/*
 * Testing N amounts of x^2 tasks through runtime and verify correctness.
 */
TEST_P(RuntimeParamFixture, SquareTest) {

    auto square = runtime.func_register([](double x) { return x * x; });
    std::function lp([](double x) { return x * x; });
    std::vector<OutputRef<double>> outputs;
    for (int i = 0; i < GetParam(); ++i) {
        auto r = runtime.invoke(square, static_cast<double>(i));
        outputs.push_back(r);
    }

    for (int i = 0; i < GetParam(); ++i) {
        // x^2 = x^2
        EXPECT_DOUBLE_EQ(*outputs[i].wait_and_get(), static_cast<double>(i) * static_cast<double>(i));
    }

}


/*
 * Testing if a task in lambda function works
 */
TEST_F(RuntimeFixture, OutputRefLifeTime) {
    auto square = runtime.func_register(_square);

    auto result = [&] {
        auto tmp = runtime.invoke<double(double)>(square, static_cast<double>(5));
        return tmp;
    }();

    EXPECT_DOUBLE_EQ(*result.wait_and_get(), 25.0);

}

/*
 * Testing whether the result of a completed task is available, instead of rerun the task.
 */
double _plus_one(double x) { return x + 1; }
TEST_F(RuntimeFixture, ParentAlreadyCompleted) {

    auto square = runtime.func_register(_square);

    auto plus_one = runtime.func_register(_plus_one);

    auto parent = runtime.invoke(square, 4.0);
    EXPECT_DOUBLE_EQ(*parent.wait_and_get(), 16.0);

    auto child = runtime.invoke(plus_one, parent);
    EXPECT_DOUBLE_EQ(*child.wait_and_get(), 17.0);
}


uint64_t compute(uint64_t x, uint32_t iterations) {
    for (uint32_t i = 0; i < iterations; ++i) {
        x = x * 6364136223846793005ULL + 1;
    }

    return x;
}
TEST_F(RuntimeFixture, Granuly) {
    auto func = runtime.func_register(compute);

    std::vector<OutputRef<uint64_t>> outputs;
    for (uint32_t i = 0; i < 10; ++i) {
        Runtime::InvokeScope scope(runtime);
        auto r = scope.invoke(func, i, i * 5);
        outputs.push_back(r);
    }

    for (uint32_t i = 0; i < 10; ++i) {
        outputs[i].wait_and_get();
    }
}

INSTANTIATE_TEST_SUITE_P(Sizes, RuntimeParamFixture, ::testing::Values(1, 10, 1000));