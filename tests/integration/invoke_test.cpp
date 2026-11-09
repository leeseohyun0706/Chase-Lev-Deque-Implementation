#include </opt/homebrew/include/gtest/gtest.h>

#include "pure/runtime/runtime.h++"
#include "tool/dual.h++"

double _square(double x) { return x * x; }

TEST(RuntimeTests, DeferredGet) {
    Runtime runtime;
    runtime.start();
    auto square = runtime.func_register(_square);

    std::vector<OutputRef<double>> outputs;

    for (int i = 0; i < 1000; ++i) {
        auto r = runtime.invoke(square, static_cast<double>(i));
        runtime.submit(r);
        outputs.push_back(r);
    }

    for (int i = 0; i < 1000; ++i) {
        EXPECT_DOUBLE_EQ(*outputs[i].get(), static_cast<double>(i) * static_cast<double>(i));
    }

    runtime.shutdown();
}

TEST(RuntimeTests, OutputRefLifeTime) {
    Runtime runtime;
    runtime.start();
    auto square = runtime.func_register(_square);

    auto result = [&] {
        auto tmp = runtime.invoke(square, static_cast<double>(5));
        runtime.submit(tmp);
        return tmp;
    }();

    EXPECT_DOUBLE_EQ(*result.get(), 25.0);

    runtime.shutdown();
}

double _plus_one(double x) { return x + 1; }
TEST(RuntimeTests, ParentAlreadyCompleted) {
    Runtime runtime;
    runtime.start();

    auto square = runtime.func_register(_square);

    auto plus_one = runtime.func_register(_plus_one);

    auto parent = runtime.invoke(square, 4.0);
    runtime.submit(parent);
    EXPECT_DOUBLE_EQ(*parent.get(), 16.0);

    auto child = runtime.invoke(plus_one, parent);
    runtime.submit(child);
    EXPECT_DOUBLE_EQ(*child.get(), 17.0);
    runtime.shutdown();
}


TEST(RuntimeTests, BasicDAG) {
    Runtime runtime;
    runtime.start();

    Dual x{2.0, 1.0};
    auto pow = runtime.func_register(Dual::pow);
    auto sin = runtime.func_register(Dual::sin);
    auto add = runtime.func_register(Dual::add);

    auto a = runtime.invoke(pow, x, 2);
    auto b = runtime.invoke(sin, x);

    auto c = runtime.invoke(add, a, b);
    runtime.submit(a, b);

    EXPECT_NEAR(c.get()->value, 4.0 + std::sin(2.0), 1e-10);

    EXPECT_NEAR(c.get()->derivative, 4.0 + std::cos(2.0), 1e-10);
    runtime.shutdown();
}

TEST(RuntimeTests, Chain) {
    Runtime runtime;
    runtime.start();

    Dual x{1.0, 1.0};
    auto sin = runtime.func_register(Dual::sin);

    auto a = runtime.invoke(sin, x);
    auto b = runtime.invoke(sin, a);
    auto c = runtime.invoke(sin, b);

    runtime.submit(a);

    const double expected = std::sin(std::sin(std::sin(1.0)));
    const double derivative = std::cos(std::sin(std::sin(1.0))) * std::cos(std::sin(1.0)) * std::cos(1.0);
    EXPECT_NEAR(c.get()->value, expected, 1e-10);
    EXPECT_NEAR(c.get()->derivative, derivative, 1e-10);
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
    auto result = outputs.front().get();


    EXPECT_NEAR(result->value, 0.0, 1e-10);
    EXPECT_NEAR(result->derivative, double(N), 1e-8);
    runtime.shutdown();

}