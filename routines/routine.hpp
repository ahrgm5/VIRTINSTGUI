#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include "instrument.hpp"

// 1. The Pure Virtual Interface (Runtime Polymorphism)
class IRoutine {
public:
    virtual ~IRoutine() = default;
    virtual void execute(const std::unordered_map<std::string, std::shared_ptr<Instrument>>& devices) = 0;
    virtual void stop() = 0;
};

// 2. The Templated Routine Class (Compile-Time Polymorphism / CRTP)
template <typename Derived>
class Routine : public IRoutine {
public:
    void execute(const std::unordered_map<std::string, std::shared_ptr<Instrument>>& devices) override {
        // Enforce shared validation before letting the derived routine run
        if (!static_cast<Derived*>(this)->validateDevices(devices)) {
            throw std::runtime_error("Routine validation failed: Required instruments missing.");
        }
        static_cast<Derived*>(this)->runRoutine(devices);
    }
};