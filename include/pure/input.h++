

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

    T resolve() const {
        if (auto* val = std::get_if<SlotRef>(&source)) return (*val)->read_ready(); // read_ready() is only once read (whereas get() read until it's ready, possibly deadlocked).
        return std::get<T>(source);
    }

    std::exception_ptr error() const {
        if (auto* slot = std::get_if<SlotRef>(&source)) return (*slot)->error();
        return nullptr;
    }
};

#endif //CONCAL_INPUT_H
