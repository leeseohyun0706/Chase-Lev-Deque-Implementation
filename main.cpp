
#include <iostream>
#include <set>
#include <thread>

#include <utility>
// #include <syncstream> // It's not found in my environment
#include <random>

#include "include/pure/core/runtime.h++"
#include <fstream>
#include <cmath>
template<int N>
int num() { return N; }

std::atomic<int> done{0};

double add_two(double a, double b) {
    // printf("Add Two SlotType %ld, %ld\n", inputs[0], inputs[1]);
    // fflush(stdout);
    // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // done.fetch_add(1);
    return a + b;
}

double sub_two(double a, double b) {
    return a - b;
}
double mul_two(double a, double b) {
    return a * b;
}
double div_two(double a, double b) {
    return a / b;
}

template<int A, int B, int C>
double polynomial(double x) {
    return (x * x) * A + x * B + C;
}



double differentiate(double(*fn)(double), double x, double dt=1e-8) {
    auto f0 = fn(x + dt);
    auto f1 = fn(x - dt);
    auto r1 = f0 - f1;
    auto r0 = r1 / (dt * 2);
    return r0;
}

double linear_approximation(double(*fn)(double), double x, double dx=1e-8) {
    auto y0 = fn(x);
    auto diff_y = differentiate(fn, x);
    auto l = y0 + diff_y * (dx);
    return l;
}

enum class FourArithmeticType : uint8_t {
    Add =0,
    Subtract,
    Multiply,
    Divide
};
struct SimpleElementaryArithmetic {
    double lhs;
    double rhs;
    FourArithmeticType type;
    OutputSlot<double>* result;
    Task* task;
};


struct Dual {
    double value;
    double derivative;
};

Dual add(Dual a, Dual b) {
    return Dual{
        a.value + b.value,
        a.derivative + b.derivative
    };
}

Dual sine(Dual x) {
    return Dual{
        sin(x.value),
        cos(x.value) * x.derivative // Chain Rule
    };
}

Dual dexp(Dual p) {
    return Dual{
        exp(p.value),
        exp(p.value) * p.derivative
    };
}

template<int P>
Dual pow(Dual x) {
    return Dual{
        pow(x.value, P),
        (P * pow(x.value, P - 1)) * x.derivative
    };
}

Dual mul(Dual a, Dual b) {
    return Dual{
    a.value * b.value,
        a.derivative * b.value + a.value * b.derivative
    };
}

double test_function(double x) {
    // (x^2 + sin x)(e^x + x^3)
    return (pow(x, 2) + sin(x)) * (exp(x) + pow(x, 3));
}
Dual test_function(Dual x) {
    return mul(add(pow<2>(x), sine(x)), add(dexp(x), pow<3>(x)));
}


Runtime runtime;
PureFunc p_pow_2 = runtime.func_register(pow<2>);
auto p_sine = runtime.func_register(sine);
auto p_exp = runtime.func_register(dexp);
auto p_pow_3 = runtime.func_register(pow<3>);

auto p_add = runtime.func_register(add);
auto p_mul = runtime.func_register(mul);

Dual test_function_concurrency(double i) {
    Dual x{i, 1.0};


    auto taskA = runtime.invoke(p_pow_2, x);
    auto taskB = runtime.invoke(p_sine, x);


    auto taskC = runtime.invoke(p_exp, x);
    auto taskD = runtime.invoke(p_pow_3, x);

    auto taskE = runtime.invoke(p_add, taskA, taskB);
    auto taskF = runtime.invoke(p_add, taskC, taskD);

    auto taskG = runtime.invoke(p_mul, taskE, taskF);
    runtime.submit(taskA, taskB, taskC, taskD);

    return *(taskG.get());
}

auto test_function2_concurrency(double x0) {
    Dual x{x0, 1.0};

    auto a = runtime.invoke(p_pow_2, x);
    auto u = runtime.invoke(p_add, a, Dual{1.0, 0});

    auto c = runtime.invoke(p_pow_2, u);

    auto e = runtime.invoke(p_sine, u);

    auto r = runtime.invoke(p_add, c, e);
    runtime.submit(a);
    return r;
}

auto p_test_function2 = runtime.func_register(test_function2_concurrency);

double test_function2(double x) {
    return pow(pow(x, 2) + 1, 2) + sin(x*x + 1);
}

void differentiate_test(size_t n = 4) {


    runtime.start();
    // Scheduler scheduler(4);
    // scheduler.start();
    std::vector<OutputRef<Dual>> finals;

    for (int i = 0; i < (n*100); ++i) {

        double i2 = (i / 100.0) - 20;
        // auto r = runtime.invoke(p_test_function2, i2);
        // runtime.submit(r);
        // finals.push_back(r);
        auto res = (test_function2_concurrency(i2));
        finals.push_back(res);
        // printf("AutoDual f(%.3lf)-> result : %.3lf\tdiff : %.3lf\n", i2, res.value, res.derivative);
        // printf("Numeric  f(%.3lf)-> result : %.3lf\tdiff : %.3lf\n", i2, test_function2(i2), differentiate(test_function2, i2));
        // printf("Gap : |%lf|\n", abs(res.derivative - differentiate(test_function2, i2)));
        // fflush(stdout);
    }

    for (int i = 0; i < (n*100); ++i) {

        double i2 = (i / 100.0) - 20;
        auto res = *(finals[i].get());
        printf("AutoDual f(%.3lf)-> result : %.3lf\tdiff : %.3lf\n", i2, res.value, res.derivative);
        printf("Numeric  f(%.3lf)-> result : %.3lf\tdiff : %.3lf\n", i2, test_function2(i2), differentiate(test_function2, i2));
        printf("Gap : |%lf|\n", abs(res.derivative - differentiate(test_function2, i2)));
    }
    fflush(stdout);
    runtime.shutdown();
    std::ofstream output;
    output.open("output.json");

    runtime.exportTrace(output);
    // output.close();
    return;
    for (size_t i = 0; i < (n*10); ++i) {
        printf("f(x)=test(x), f'(%lf)=%lf\n", static_cast<double>(i) / 10.0, differentiate(test_function, static_cast<double>(i) / 10.0));
        printf("f(%lf)=%lf\n",static_cast<double>(i) / 10.0,test_function(static_cast<double>(i) / 10.0));
        printf("L(%lf)=%lf\n", static_cast<double>(i) / 10.0,linear_approximation(test_function, static_cast<double>(i) / 10.0));
        printf("Dual_f'(%lf)=%lf\n", static_cast<double>(i) / 10.0, test_function(Dual{static_cast<double>(i) / 10.0, 1.0}).derivative);
    }

    for (size_t i = 0; i < (n*10); ++i) {
        printf("f(x)=sqrt(x), f'(%lf)=%lf\n", static_cast<double>(i) / 10.0, differentiate(sqrt, static_cast<double>(i) / 10.0));
        printf("f(%lf)=%lf\n",static_cast<double>(i) / 10.0,sqrt(static_cast<double>(i) / 10.0));
        printf("L(%lf)=%lf\n", static_cast<double>(i) / 10.0,linear_approximation(sqrt, static_cast<double>(i) / 10.0));
    }

    // 3x^2 + 4x - 14
    // for (int i = 0; i < 30; i++) {
    //     printf("f'(%d)=%lf\n", i, differentiate(polynomial<3, 4, -14>, i));
    // }
}

void simple_random_elementary_arithmetic_test(size_t n = 1024 * 10, size_t threads = 6) {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<size_t> term(1, 1000);
    std::uniform_int_distribution<size_t> type(0, 3);

    std::vector<SimpleElementaryArithmetic> arithmetics;

    std::vector<OutputSlot<double>> outputs(n);

    for (size_t i = 0; i < n; ++i) {
        arithmetics.push_back(SimpleElementaryArithmetic{
            (double)term(gen),
            (double)term(gen),
            static_cast<FourArithmeticType>(type(gen)),
            &outputs[i],
            nullptr
        });
    }

    PureFunc<double(double, double)> p_add_two({1}, &add_two);
    PureFunc<double(double, double)> p_sub_two({1}, &sub_two);
    PureFunc<double(double, double)> p_mul_two({1}, &mul_two);
    PureFunc<double(double, double)> p_div_two({1}, &div_two);

    static const std::unordered_map<FourArithmeticType, PureFunc<double(double, double)>> ar_type_map = {
        {FourArithmeticType::Add, p_add_two},
        {FourArithmeticType::Subtract, p_sub_two},
        {FourArithmeticType::Multiply, p_mul_two},
        {FourArithmeticType::Divide, p_div_two}
    };
    std::vector<std::unique_ptr<Task>> tasks;

    for (size_t i = 0; i < n; ++i) {
        const std::tuple inputs{
            Input(arithmetics.at(i).lhs), Input(arithmetics.at(i).rhs)
        };

        // tasks.push_back(
        //     std::make_unique<PureFuncTask<double, double, double>>(
        //     ar_type_map.at(arithmetics.at(i).type), inputs, arithmetics.at(i).result
        //     )
        // );

        arithmetics[i].task = ((tasks.end() - 1))->get();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    Scheduler scheduler(threads);
    printf("Run Scheduler\n");
    fflush(stdout);
    scheduler.start();
    for (auto& task: tasks) {
        scheduler.submit(task.get());
    }
    // scheduler.start();

    static const std::unordered_map<FourArithmeticType, std::string> ar_str_map = {
        {FourArithmeticType::Add, "+"},
        {FourArithmeticType::Subtract, "-"},
        {FourArithmeticType::Multiply, "*"},
        {FourArithmeticType::Divide, "/"},
    };

    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // auto ss = scheduler.get_trace_recorder().snapshot();
    //
    //
    // for (auto& event : ss) {
    //     printf("%s\n", event.to_string().c_str());
    // }
    // fflush(stdout);

    // scheduler.submit(tasks.at(0).get());

    for (auto& ar : arithmetics) {
        printf("%lf %s %lf = %lf\n", ar.lhs, ar_str_map.at(ar.type).c_str(), ar.rhs, ar.result->get());
    }

    scheduler.stop_request();
    scheduler.join_worker();

    std::ofstream output;
    output.open("output.json");

    scheduler.exportTrace(output);

    output.close();
}

int main() {
    differentiate_test();
    // simple_random_elementary_arithmetic_test();
    return 0;

}

