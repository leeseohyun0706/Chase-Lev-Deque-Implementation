
#ifndef CONCAL_FUNC_H
#define CONCAL_FUNC_H
#include <variant>

#include "core/outputslot.h++"
#include "core/id.h++"


template<typename T>
struct Input {

    using SlotRef = const OutputSlot<T>*;

    std::variant<T, SlotRef> source;

    explicit Input(OutputRef<T> source) : source(source.getOutputSlot()) { }
    explicit Input(SlotRef source) : source(source) {}
    explicit Input(const T& source) : source(source) {}

    T resolve() const {
        if (auto* val = std::get_if<SlotRef>(&source)) return (*val)->read_ready(); // read_ready() is only once read (whereas get() read until it's ready, possibly deadlocked).
        return std::get<T>(source);
    }

};

template<typename F>
concept TaskCallable = std::invocable<F>;

template<typename Signature>
class PureFunc;

template<typename R, typename... Args>
class PureFunc<R(Args...)> {
private:
    struct Callable {
        virtual ~Callable() = default;
        virtual R invoke(Args... args) = 0;
        virtual std::unique_ptr<Callable> clone() const = 0;
    };

    template<typename F>
    struct CallableModel : public Callable {
        F func;
        R invoke(Args... args) override { return func(std::forward<Args>(args)...); }
        std::unique_ptr<Callable> clone() const override { return std::make_unique<CallableModel>(func); }

        explicit CallableModel(F&& func) : func(func) {}
        explicit CallableModel(const F& func) : func(func) {} // For Lambda
    };
    FuncId _id;
    std::unique_ptr<Callable> _callable;

public:

    // PureFunc() = default;

    template<typename F>
    PureFunc(FuncId id, F func) : _id(id), _callable(std::make_unique<CallableModel<F>>(std::forward<F>(func))) {}


    PureFunc(const PureFunc& other) { if (other._callable){ _callable = other._callable->clone(); _id = other._id; } }

    PureFunc(PureFunc&& other) = default;

    PureFunc& operator=(const PureFunc& other)   {
        if (this != &other) {
            PureFunc tmp(other);
            std::swap(this->_id, tmp._id);
            std::swap(this->_callable, tmp._callable);
        }
        return *this;
    }
    PureFunc& operator=(PureFunc&& other) = default;

    R operator()(Args... args) const {
        if (!_callable) {
            throw std::bad_function_call();
        }
        return _callable->invoke(std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept {
        return _callable != nullptr;
    }

    auto invoke(Args... args) const {
        return operator()(std::forward<Args>(args)...);
    }
};

// Type Deduction Specialization
template <typename R, typename... Args>
PureFunc(FuncId, R(*)(Args...)) -> PureFunc<R(Args...)>;

template <typename F, typename Op = decltype(&F::operator())>
struct deduce_signature;
// For Lambda, but IDK how R(ClassType::*)(Args...) works.
template <typename F, typename R, typename ClassType, typename... Args>
struct deduce_signature<F, R(ClassType::*)(Args...) const> {
    using type = R(Args...);
};
// When Lambda is received, its template type is deduced to R(Args...) which is a function type removed Lambda Class.
// <lambda R(Args...)> -> deduce_signature<lambda R(Args...)>
// deduce_signature<lambda R(Args...)>::type -> R(Args...)
template <typename F>
PureFunc(FuncId, F) -> PureFunc<typename deduce_signature<F>::type>;

#endif //CONCAL_FUNC_H
