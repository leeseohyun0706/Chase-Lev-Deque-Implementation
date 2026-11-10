#include <gtest/gtest.h>
#include <fstream>

#include "pure/runtime/runtime.h++"
#include "tool/dual.h++"

double _square(double x) { return x * x; }
double _add(double a, double b) { return a + b; }
double _sub(double a, double b) { return a - b; }
double _mul(double a, double b) { return a * b; }
double _div(double a, double b) { return a / b; }


/*
 * Testing N amounts of x^2 tasks through runtime and verify correctness.
 */
TEST(RuntimeTests, SquareTest) {
    size_t N = 1000;
    Runtime runtime;
    runtime.start();

    auto square = runtime.func_register([](double x) { return x * x; });
    std::function lp([](double x) { return x * x; });
    std::vector<OutputRef<double>> outputs;
    for (int i = 0; i < N; ++i) {
        auto r = runtime.invoke(square, static_cast<double>(i));
        outputs.push_back(r);
    }

    for (int i = 0; i < N; ++i) {
        // x^2 = x^2
        EXPECT_DOUBLE_EQ(*outputs[i].wait_and_get(), static_cast<double>(i) * static_cast<double>(i));
    }

    runtime.shutdown();
}

/*
 * Testing if a task in lambda function works
 */
TEST(RuntimeTests, OutputRefLifeTime) {
    Runtime runtime;
    runtime.start();
    auto square = runtime.func_register(_square);

    auto result = [&] {
        auto tmp = runtime.invoke<double(double)>(square, static_cast<double>(5));
        return tmp;
    }();

    EXPECT_DOUBLE_EQ(*result.wait_and_get(), 25.0);

    runtime.shutdown();
}

/*
 * Testing whether the result of a completed task is available, instead of rerun the task.
 */
double _plus_one(double x) { return x + 1; }
TEST(RuntimeTests, ParentAlreadyCompleted) {
    Runtime runtime;
    runtime.start();

    auto square = runtime.func_register(_square);

    auto plus_one = runtime.func_register(_plus_one);

    auto parent = runtime.invoke(square, 4.0);
    EXPECT_DOUBLE_EQ(*parent.wait_and_get(), 16.0);

    auto child = runtime.invoke(plus_one, parent);
    EXPECT_DOUBLE_EQ(*child.wait_and_get(), 17.0);
    runtime.shutdown();
}
uint64_t compute(uint64_t x, uint32_t iterations) {
    for (uint32_t i = 0; i < iterations; ++i) {
        x = x * 6364136223846793005ULL + 1;
    }

    return x;
}
TEST(RuntimeTests, Granuly) {

    Runtime runtime;
    auto func = runtime.func_register(compute);

    runtime.start();
    std::vector<OutputRef<uint64_t>> outputs;
    for (uint32_t i = 0; i < 10; ++i) {
        Runtime::InvokeScope scope(runtime);
        auto r = scope.invoke(func, i, i * 5);
        outputs.push_back(r);
    }

    for (uint32_t i = 0; i < 10; ++i) {
        outputs[i].wait_and_get();
    }
    runtime.shutdown();
}
TEST(RuntimeTests, BasicDAG) {
    Runtime runtime;
    runtime.start();

    Dual x{2.0, 1.0};
    auto ww = PureFunc<Dual(Dual, int)>({1}, Dual::pow);
    auto pow = runtime.func_register(Dual::pow);
    auto sin = runtime.func_register(Dual::sin);
    auto add = runtime.func_register(Dual::add);

    auto a = runtime.invoke(pow, x, 2);
    auto b = runtime.invoke(sin, x);

    auto c = runtime.invoke(add, a, b);

    EXPECT_NEAR(c.wait_and_get()->value, 4.0 + std::sin(2.0), 1e-10);
    EXPECT_NEAR(c.wait_and_get()->derivative, 4.0 + std::cos(2.0), 1e-10);


    runtime.shutdown();
}

TEST(RuntimeTests, Chain) {
    Runtime runtime;
    runtime.start();

    Dual x{1.0, 1.0};
    auto sin = runtime.func_register(Dual::sin);
    const OutputSlot<Dual>* res;
    {
        Runtime::InvokeScope scope(runtime);
        auto a = scope.invoke(sin, x);
        auto b = scope.invoke(sin, a);
        auto c = scope.invoke(sin, b);
        res = c.getOutputSlot();
    }

    const double expected = std::sin(std::sin(std::sin(1.0)));
    const double derivative = std::cos(std::sin(std::sin(1.0))) * std::cos(std::sin(1.0)) * std::cos(1.0);
    EXPECT_NEAR(res->get().value, expected, 1e-10);
    EXPECT_NEAR(res->get().derivative, derivative, 1e-10);
    runtime.shutdown();
}

Dual term(Dual x, int k) {
    return {
    std::sin(k * x.value) / k,
        std::cos(k * x.value) * x.derivative
    };
}



TEST(RuntimeTest, MassiveDAG) {
    constexpr int N = 1000;
    Runtime runtime;
    runtime.start();

    auto fn = runtime.func_register(term);
    auto add = runtime.func_register(Dual::add);
    Dual x{0.0, 1.0};
    std::vector<OutputRef<Dual>> outputs;
    outputs.reserve(N);
    {
        Runtime::InvokeScope invoke_scope(runtime);
        for (int i = 1; i <= N; ++i) {
            auto r = invoke_scope.invoke(fn, x, i);
            outputs.push_back(r);
        }

        // auto tmps(outputs);
        while (outputs.size() > 1) {
            std::vector<OutputRef<Dual>> next;
            next.reserve((outputs.size() + 1) / 2);

            for (size_t i = 0; i < outputs.size(); i += 2) {
                if (i + 1 < outputs.size()) {
                    auto r = invoke_scope.invoke(
                        add,
                        outputs[i],
                        outputs[i + 1]
                    );
                    next.push_back(
                        r
                    );
                } else {
                    next.push_back(outputs[i]);
                }
            }

            outputs = std::move(next);
        }
        // for (auto& t : tmps) runtime.submit(t);
        }
    auto result = outputs.front().wait_and_get();


    EXPECT_NEAR(result->value, 0.0, 1e-10);
    EXPECT_NEAR(result->derivative, double(N), 1e-8);
    runtime.shutdown();

}
Dual testFunc(Dual x) {
    // std::this_thread::sleep_for(std::chrono::milliseconds(int(100 * x.value) % 100));
    return Dual::mul(Dual::add(Dual::pow(x, 2), Dual::sin(x)), Dual::add(Dual::exp(x), Dual::pow(x, 3)));
}
TEST(RuntimeTest, DiffTests) {
    constexpr size_t N = 100;
    Runtime runtime(4);

    auto pow = runtime.func_register(Dual::pow);
    auto sin = runtime.func_register(Dual::sin);
    auto exp = runtime.func_register(Dual::exp);
    auto add = runtime.func_register(Dual::add);
    auto mul = runtime.func_register(Dual::mul);
    runtime.start();
    std::vector<OutputSlot<Dual>*> outputs;
    for (size_t i = 0; i < N; ++i) {
        Dual x{static_cast<double>(i), 1.0};
        {
            Runtime::InvokeScope scope(runtime);

            auto taskA = scope.invoke(pow, x, 2);
            auto taskB = scope.invoke(sin, x);


            auto taskC = scope.invoke(exp, x);
            auto taskD = scope.invoke(pow, x, 3);

            auto taskE = scope.invoke(add, taskA, taskB);
            auto taskF = scope.invoke(add, taskC, taskD);

            auto taskG = scope.invoke(mul, taskE, taskF);
            outputs.push_back(taskG.getOutputSlot());
        }
    }
    for (size_t i = 0; i < N; ++i) {
        outputs[i]->wait();
    }
    runtime.shutdown();

    std::ofstream stream;
    stream.open("output1.json");

    runtime.exportTrace(stream);

    stream.close();
}

TEST(RuntimeTest, DiamondDAG) {
    Runtime runtime(4);
    auto add = runtime.func_register(_add);
    auto sub = runtime.func_register(_sub);
    auto div = runtime.func_register(_div);
    auto mul = runtime.func_register(_mul);

    runtime.start();
    OutputSlot<double>* res;
    {
        Runtime::InvokeScope scope(runtime);
        auto a = scope.invoke(add, 10, 5);
        auto b = scope.invoke(mul, a, 2);
        auto c = scope.invoke(sub, a, 3);

        auto d = scope.invoke(add, b, c);
        auto e = scope.invoke(div, d, 6);
        res = e.getOutputSlot();
    }

    res->wait();
    EXPECT_EQ(int(res->read_ready()), 7);

    runtime.shutdown();
}