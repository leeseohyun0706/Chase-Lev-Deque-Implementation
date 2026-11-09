
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
        if (auto* val = std::get_if<SlotRef>(&source)) return (*val)->get();
        return std::get<T>(source);
    }

};

template<typename Signature>
class PureFunc;


template<typename R, typename... Args>
class PureFunc<R(Args...)> {
public:
    using FunctionPtr = R (&)(Args...);
    PureFunc(FuncId _id, FunctionPtr ptr) : _id(_id), _ptr(ptr) {}

    R invoke(Args... args) {
        return (_ptr)(std::forward<Args>(args) ...);
    }

private:
    FuncId _id;
    FunctionPtr _ptr;

};
#endif //CONCAL_FUNC_H
