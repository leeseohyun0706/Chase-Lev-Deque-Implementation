
#ifndef CONCAL_RUNTIME_FIXTURE_H
#define CONCAL_RUNTIME_FIXTURE_H

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include "math_helper.h++"
#include "tool/dual.h++"
#include "pure/runtime/runtime.h++"

class RuntimeFixture : public ::testing::Test {
protected:


    static RuntimeConfiguration makeConfig() {
        RuntimeConfiguration cfg;
        cfg.tracing.enable = false;
        cfg.tracing.buffer_size = 1 << 8;
        return cfg;
    }

    Runtime runtime{makeConfig()};
    static const char* traceDir() { return "./traces"; }


    void SetUp() override {
        runtime.start();
    }
    void TearDown() override {
        runtime.shutdown();

        auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string name = std::string(info->test_suite_name()) + "." + info->name();
        std::ranges::replace(name, '/', '_');

        std::filesystem::create_directories(traceDir());
        std::ofstream out(std::filesystem::path(traceDir()) / (name + ".json"));
        runtime.exportTrace(out);
    }
};

template<typename T>
class MathRuntimeFixture : public RuntimeFixture {
protected:
    MathFuncs<T> math{runtime};
};

template<typename T>
class MathParamFixture : public MathRuntimeFixture<T>, public ::testing::WithParamInterface<size_t>{};

using MathTypes = ::testing::Types<double, Dual>;

class DoubleFixture : public MathRuntimeFixture<double> {};

class DualParamFixture : public MathRuntimeFixture<Dual>, public ::testing::WithParamInterface<size_t> {};
class DoubleParamFixture : public MathRuntimeFixture<double>, public ::testing::WithParamInterface<size_t> {};


class RuntimeParamFixture : public RuntimeFixture, public ::testing::WithParamInterface<size_t> {};

#endif //CONCAL_RUNTIME_FIXTURE_H
