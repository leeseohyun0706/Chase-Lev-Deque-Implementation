
#include "runtime_fixture.h++"
#include "pure/runtime/runtime.h++"
#include "tool/dual.h++"


Dual term(Dual x, int k) {
    return {
        std::sin(k * x.value) / k,
            std::cos(k * x.value) * x.derivative
        };
}


TEST_P(RuntimeParamFixture, MassiveDAG) {
    // constexpr int N = 1000;

    auto fn = runtime.func_register(term);
    auto add = runtime.func_register(Dual::add);
    Dual x{0.0, 1.0};
    std::vector<OutputRef<Dual>> outputs;
    outputs.reserve(GetParam());
    {
        Runtime::InvokeScope invoke_scope(runtime);
        for (int i = 1; i <= GetParam(); ++i) {
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
    EXPECT_NEAR(result->derivative, double(GetParam()), 1e-8);

}



Dual testFunc(Dual x) {
    // std::this_thread::sleep_for(std::chrono::milliseconds(int(100 * x.value) % 100));
    return Dual::mul(Dual::add(Dual::pow(x, 2), Dual::sin(x)), Dual::add(Dual::exp(x), Dual::pow(x, 3)));
}


TEST_P(DualParamFixture, DiffTests) {
    std::vector<Dual> inputs;
    std::vector<OutputRef<Dual>> outputs;
    for (size_t i = 0; i < GetParam(); ++i) {
        Dual x{i * 0.001};
        inputs.push_back(x);
        auto taskA = this->runtime.invoke(this->math.pow, x, 2);
        auto taskB = this->runtime.invoke(this->math.sin, x);


        auto taskC = this->runtime.invoke(this->math.exp, x);
        auto taskD = this->runtime.invoke(this->math.pow, x, 3);

        auto taskE = this->runtime.invoke(this->math.add, taskA, taskB);
        auto taskF = this->runtime.invoke(this->math.add, taskC, taskD);

        auto taskG = this->runtime.invoke(this->math.mul, taskE, taskF);
        outputs.push_back(taskG);
    }
    for (size_t i = 0; i < GetParam(); ++i) {
        Dual expected = testFunc(inputs[i]);
        auto got = outputs[i].wait_and_get();
        if constexpr (std::is_same_v<Dual, Dual>) {
            EXPECT_NEAR(got->value, expected.value, 1e-9);
            EXPECT_NEAR(got->derivative, expected.derivative, 1e-9);
        }
    }
}



// Function for approximate integral
// template<typename F>
double simpsons_(double(func)(double) , double ll, double ul, int n)
{
    // Calculating the value of h
    double h = (ul - ll) / n;

    // Array for storing value of x and f(x)
    double x[n], fx[n];

    // Calculating values of x and f(x)
    for (int i = 0; i <= n; i++) {
        x[i] = ll + i * h;
        fx[i] = func(x[i]);
    }

    // Calculating result
    double res = 0;
    for (int i = 0; i <= n; i++) {
        if (i == 0 || i == n)
            res += fx[i];
        else if (i % 2 != 0)
            res += 4 * fx[i];
        else
            res += 2 * fx[i];
    }
    res = res * (h / 3);
    return res;
}

double dlog(double d) {
    return std::sin(d * 100);
}

TEST_F(DoubleFixture, SimpsonIntegral) {
    auto integral = runtime.func_register(simpsons_);

    // auto res = runtime.invoke(integral, dlog, 4, 5.2, 64);
    auto part1 = runtime.func_register([](double ll, int i, double h) {
        return ll + i * h;
    });

    auto part2 = runtime.func_register([](double(func)(double), double xi, int i, int n) {
        if (i == 0 || i == n)
            return func(xi);
        else if (i % 2 != 0)
            return 4 * func(xi);
        else
            return 2 * func(xi);
    });

    auto c_simpsons = [&] (double(func)(double) , double ll, double ul, int n) {

        double h = (ul - ll) / n;
        std::vector<OutputRef<double>> x, rx;
        x.reserve(n + 1);
        for (int i = 0; i <= n; ++i) {
            x[i] = runtime.invoke(part1, ll, i, h);
        }

        double res = 0;
        for (int i = 0; i <= n; i++) {
            rx.push_back(runtime.invoke(part2, func, x[i], i, n));
        }
        for (auto& d : rx) res += *d.wait_and_get();
        res = res * (h / 3);
        return res;
    };
    // auto crimson = runtime.func_register(c_simpsons);
    // auto res = runtime.invoke(crimson, dlog, 0, 1, 64);
    std::chrono::high_resolution_clock clock;
    auto s1 = clock.now();
    auto r_res = c_simpsons(dlog, 0, 1, 1024 * 16);
    auto e1 = clock.now();
    printf("%lf\n", r_res);
    std::cout << std::chrono::high_resolution_clock::duration(e1 - s1).count() << std::endl;
    auto s2 = clock.now();
    auto v_res = simpsons_(dlog, 0, 1, 1024 * 16);
    auto e2 = clock.now();
    printf("%lf\n", v_res);
    std::cout << std::chrono::high_resolution_clock::duration(e2 - s2).count() << std::endl;
}

INSTANTIATE_TEST_SUITE_P(Sizes, RuntimeParamFixture, ::testing::Values(1, 10, 1000));
INSTANTIATE_TEST_SUITE_P(Sizes, DualParamFixture, ::testing::Values(1, 10, 1000, 10000));