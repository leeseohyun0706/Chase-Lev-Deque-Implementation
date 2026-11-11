#include "runtime_fixture.h++"

TEST_F(RuntimeFixture, BasicDAG) {

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
}

TEST_F(RuntimeFixture, Chain) {
    Dual x{1.0, 1.0};
    auto sin = runtime.func_register(Dual::sin);
    OutputRef<Dual> res{nullptr, nullptr};
    {
        Runtime::InvokeScope scope(runtime);
        auto a = scope.invoke(sin, x);
        auto b = scope.invoke(sin, a);
        auto c = scope.invoke(sin, b);
        res = c;
    }

    const double expected = std::sin(std::sin(std::sin(1.0)));
    const double derivative = std::cos(std::sin(std::sin(1.0))) * std::cos(std::sin(1.0)) * std::cos(1.0);
    EXPECT_NEAR(res.wait_and_get()->value, expected, 1e-10);
    EXPECT_NEAR(res.wait_and_get()->derivative, derivative, 1e-10);
}



TEST_F(RuntimeFixture, DiamondDAG) {

    auto add = runtime.func_register(GenericMath::add<double>);
    auto sub = runtime.func_register(GenericMath::sub<double>);
    auto div = runtime.func_register(GenericMath::div<double>);
    auto mul = runtime.func_register(GenericMath::mul<double>);

    OutputRef<double> res{nullptr, nullptr};
    {
        Runtime::InvokeScope scope(runtime);
        auto a = scope.invoke(add, 10, 5);
        auto b = scope.invoke(mul, a, 2);
        auto c = scope.invoke(sub, a, 3);

        auto d = scope.invoke(add, b, c);
        auto e = scope.invoke(div, d, 6);
        res = e;
    }

    EXPECT_EQ(int(*res.wait_and_get()), 7);

}


