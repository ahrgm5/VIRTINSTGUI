#pragma once

#include "instrument.hpp"

class Arroyo5240 : public Instrument {
public:
    static ChannelSettings defaultArroyoSettings();

    explicit Arroyo5240(ViSession defaultRM);

    // Safely evaluates defaultArroyoSettings() BEFORE passing it to Instrument base constructor
    Arroyo5240(
        ViSession defaultRM,
        const std::string& resourceName,
        const ChannelSettings& settings = defaultArroyoSettings()
    );

    ~Arroyo5240() override = default;

    // Runtime virtual override (safe after object is fully built)
    ChannelSettings defaultSettings() const override {
        return defaultArroyoSettings();
    }

    // Default open overload using Arroyo settings
    virtual bool open(const std::string& resourceName, const ChannelSettings& settings = defaultArroyoSettings()) {
        return Instrument::open(resourceName, settings);
    }

    std::string getIdentity();
    std::string beep();

    void setTemperatureSetpoint(double tempC);
    double getTemperatureSetpoint();
    double getCurrentTemperature();

    void setOutputEnable(bool enable);
    bool isOutputEnabled();
};