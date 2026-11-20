

#ifndef CONCAL_INPUT_H
#define CONCAL_INPUT_H

#include "pure/output_ref.h++"

template<typename T>
struct Input {

    using SlotRef = std::shared_ptr<OutputSlot<T>>;

    std::variant<T, SlotRef> source;

    explicit Input(OutputRef<T> source) : source(source.getSlot()) { }
    explicit Input(SlotRef source) : source(std::move(source)) {}
    explicit Input(const T& source) : source(source) {}

    // If the source is SlotRef, return its value by using read_ready() (because task dependency is managed by DAG, it is not required to wait until ready when the worker tries to resolve the input).
    // Otherwise, return the source.
    T resolve() const {
        if (auto* val = std::get_if<SlotRef>(&source)) return (*val)->read_ready(); // read_ready() is only once read (whereas OutputSlot::get() wait and read until it's ready, possibly deadlocked).
        return std::get<T>(source);
    }

    // Return an error if the source is SlotRef type and contains an error, otherwise nullptr.
    std::exception_ptr error() const {
        if (auto* slot = std::get_if<SlotRef>(&source)) return (*slot)->error();
        return nullptr;
    }
};

#endif //CONCAL_INPUT_H
