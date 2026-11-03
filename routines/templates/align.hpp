#pragma once

#include "routine.hpp"
#include <iostream>

class lsrWldRoutine : public Routine<lsrWldRoutine> {
public:
    bool validateDevices(const std::unordered_map<std::string, std::shared_ptr<Instrument>>& devices) {
        // Ensure required hardware handles are present in the station map
        return devices.count("LSR") && devices.count("MCA_ZABER");
    }

    void runRoutine(const std::unordered_map<std::string, std::shared_ptr<Instrument>>& devices) {
        std::cout << "Executing lsrWldRoutine..." << std::endl;
        // Access devices using devices.at("LSR"), etc.
    }

    void stop() override {
        std::cout << "Stopping lsrWldRoutine safely..." << std::endl;
    }
};