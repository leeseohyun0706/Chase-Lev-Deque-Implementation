
#include "runtime_fixture.h++"
#include <pure/runtime.h++>
#include "dual.h++"

inline void burn1(int work) {
    double acc = 1.0;
    for (int i = 0; i < work; ++i) acc = acc * 1.0000001 + 1e-9;
}

Dual term(Dual x, int k) {
    burn1(10000);
    return {
        std::sin(k * x.value) / k,
            std::cos(k * x.value) * x.derivative
        };
}


TEST_P(RuntimeParamFixture, MassiveDAG) {

    auto fn = runtime.func_register(term);
    auto add = runtime.func_register(Dual::add);
    Dual x{0.0, 1.0};
    // outputs.reserve(GetParam());
    std::optional<OutputRef<Dual>> result;
    {
        std::vector<OutputRef<Dual>> outputs;
        Runtime::InvokeScope invoke_scope(runtime);
        for (int i = 1; i <= GetParam(); ++i) {
            auto r = invoke_scope.invoke(fn, x, i);
            outputs.push_back(r);
        }

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
        result = outputs.front();
        // for (auto& t : tmps) runtime.submit(t);
    }

    EXPECT_NEAR(result->wait_and_get()->value, 0.0, 1e-10);
    EXPECT_NEAR(result->wait_and_get()->derivative, double(GetParam()), 1e-8);

}



Dual testFunc(Dual x) {
    return Dual::mul(Dual::add(Dual::pow(x, 2), Dual::sin(x)), Dual::add(Dual::exp(x), Dual::pow(x, 3)));
}


inline void burn(int work) {
    double acc = 1.0;
    for (int i = 0; i < work; ++i) acc = acc * 1.0000001 + 1e-9;
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
double simpsons_(double(func)(double) , double ll, double ul, int n)
{
    double h = (ul - ll) / n;

    double x[n], fx[n];

    for (int i = 0; i <= n; i++) {
        x[i] = ll + i * h;
        fx[i] = func(x[i]);
    }

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
    return std::log(d);
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
        for (int i = 0; i <= n; ++i) {
            x.push_back(runtime.invoke(part1, ll, i, h));
        }

        double res = 0;
        for (int i = 0; i <= n; i++) {
            rx.push_back(runtime.invoke(part2, func, x[i], i, n));
        }
        for (auto& d : rx) res += *d.wait_and_get();
        res = res * (h / 3);
        return res;
    };
    
}

INSTANTIATE_TEST_SUITE_P(Sizes, RuntimeParamFixture, ::testing::Values(1, 10, 1000));
INSTANTIATE_TEST_SUITE_P(Sizes, DualParamFixture, ::testing::Values(1, 10, 1000, 10000));