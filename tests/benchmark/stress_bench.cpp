#include <vector>
#include "math_helper.h++"
#include <benchmark/benchmark.h>
#include <memory>
#include <optional>

inline void burn(int work) {
    double acc = 1.0;
    for (int i = 0; i < work; ++i) acc = acc * 1.0000001 + 1e-9;
    benchmark::DoNotOptimize(acc);
}


template<typename R, typename... A>
auto busy_func(Runtime& rt, R (*f)(A...), int work) {
    return rt.func_register([f, work](A... a) -> R {
        burn(work);
        return f(a...);
    });
}
struct BusyMath {
    PureFunc<Dual(Dual, Dual)> add, mul;
    PureFunc<Dual(Dual, int)>  pow;
    PureFunc<Dual(Dual)>       exp, sin;
    BusyMath(Runtime& rt, int w)
        : add(busy_func(rt, GenericMath::add<Dual>, w)),
          mul(busy_func(rt, GenericMath::mul<Dual>, w)),
          pow(busy_func(rt, GenericMath::pow<Dual>, w)),
          exp(busy_func(rt, GenericMath::exp<Dual>, w)),
          sin(busy_func(rt, GenericMath::sin<Dual>, w)) {}
};


inline Dual term(Dual x, int k) {
    return { std::sin(k * x.value) / k, std::cos(k * x.value) * x.derivative };
}

inline Dual testFunc(Dual x) {
    return Dual::mul(Dual::add(Dual::pow(x, 2), Dual::sin(x)),
                     Dual::add(Dual::exp(x), Dual::pow(x, 3)));
}

template<typename TermFn, typename AddFn>
OutputRef<Dual> build_massive_dag(Runtime& rt, const TermFn& fn, const AddFn& add, size_t N) {
    std::vector<OutputRef<Dual>> level;
    level.reserve(N);
    Runtime::InvokeScope scope(rt);
    const Dual x{0.0, 1.0};
    for (int k = 1; k <= static_cast<int>(N); ++k)
        level.push_back(scope.invoke(fn, x, k));

    while (level.size() > 1) {
        std::vector<OutputRef<Dual>> next;
        next.reserve((level.size() + 1) / 2);
        for (size_t i = 0; i < level.size(); i += 2)
            next.push_back(i + 1 < level.size() ? scope.invoke(add, level[i], level[i + 1])
                                                 : level[i]);
        level = std::move(next);
    }
    return level.front();
}


template<typename M>
inline std::vector<OutputRef<Dual>> build_diff_dags(Runtime& rt, M& m, size_t N) {
    std::vector<OutputRef<Dual>> out;
    out.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        Dual x{i * 0.001, 1.0};
        auto a = rt.invoke(m.pow, x, 2);
        auto b = rt.invoke(m.sin, x);
        auto c = rt.invoke(m.exp, x);
        auto d = rt.invoke(m.pow, x, 3);
        out.push_back(rt.invoke(m.mul, rt.invoke(m.add, a, b), rt.invoke(m.add, c, d)));
    }
    return out;
}

class RuntimeBench : public benchmark::Fixture {
protected:
    std::unique_ptr<Runtime> runtime;
    std::optional<MathFuncs<Dual>> math;
public:
    void SetUp(benchmark::State& st) override {
        RuntimeConfiguration cfg;
        cfg.tracing.enable        = false;
        cfg.scheduler.worker_size = static_cast<uint32_t>(st.range(1));
        runtime = std::make_unique<Runtime>(cfg);
        math.emplace(*runtime);
        runtime->start();
    }
    void TearDown(benchmark::State&) override {
        runtime->shutdown(); math.reset(); runtime.reset();
    }
};

BENCHMARK_DEFINE_F(RuntimeBench, MassiveDAG)(benchmark::State& st) {
    const size_t N = st.range(0);
    const int work = static_cast<int>(st.range(2));
    auto fn  = busy_func(*runtime, term, work);
    auto add = busy_func(*runtime, Dual::add, work);
    for (auto _ : st)
        benchmark::DoNotOptimize(build_massive_dag(*runtime, fn, add, N).wait_and_get());
    st.SetItemsProcessed(st.iterations() * (2 * N - 1));
}
BENCHMARK_REGISTER_F(RuntimeBench, MassiveDAG)
    ->ArgsProduct({{1000, 10000}, {1, 2, 3, 4}, {0, 100, 1000, 10000}})
    ->ArgNames({"N", "workers", "work"})
    ->UseRealTime()->MeasureProcessCPUTime()->Unit(benchmark::kMicrosecond);



BENCHMARK_DEFINE_F(RuntimeBench, DiffTests)(benchmark::State& st) {
    const size_t N = st.range(0);
    BusyMath m(*runtime, static_cast<int>(st.range(2)));
    for (auto _ : st)
        for (auto& o : build_diff_dags(*runtime, m, N))
            benchmark::DoNotOptimize(o.wait_and_get());
    st.SetItemsProcessed(st.iterations() * 7 * N);
}
BENCHMARK_REGISTER_F(RuntimeBench, DiffTests)
    ->ArgsProduct({{1000, 10000}, {1, 2, 3, 4}, {0, 100, 1000, 10000}})
    ->ArgNames({"N", "workers", "work"})
    ->UseRealTime()->MeasureProcessCPUTime()->Unit(benchmark::kMicrosecond);

inline Dual testFunc_busy(Dual x, int w) {
    burn(w); auto a = Dual::pow(x, 2);
    burn(w); auto b = Dual::sin(x);
    burn(w); auto c = Dual::exp(x);
    burn(w); auto d = Dual::pow(x, 3);
    burn(w); auto l = Dual::add(a, b);
    burn(w); auto r = Dual::add(c, d);
    burn(w); return Dual::mul(l, r);
}

static void serial_difftests(benchmark::State& st) {
    const size_t N = st.range(0); const int w = static_cast<int>(st.range(1));
    for (auto _ : st)
        for (size_t i = 0; i < N; ++i)
            benchmark::DoNotOptimize(testFunc_busy(Dual{i * 0.001, 1.0}, w));
    st.SetItemsProcessed(st.iterations() * 7 * N);
}


BENCHMARK(serial_difftests)
    ->ArgsProduct({{1000, 10000}, {0, 100, 1000, 10000}})->ArgNames({"N", "work"})
    ->Unit(benchmark::kMicrosecond);


static void BM_Burn(benchmark::State& st) {
    for (auto _ : st) burn(static_cast<int>(st.range(0)));
}
BENCHMARK(BM_Burn)->Arg(100)->Arg(1000)->Arg(10000);