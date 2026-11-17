#pragma once

#include "instrument.hpp"
#include "imotoraxis.hpp"
#include <string>
#include <cstdint>

class Zaber : public Instrument, public IMotorAxis {
public:
    static ChannelSettings defaultZaberSettings();
    ChannelSettings defaultSettings() const override { return defaultZaberSettings(); }

    explicit Zaber(ViSession defaultRM);
    Zaber(
        ViSession defaultRM,
        const std::string& resourceName,
        const ChannelSettings& settings = defaultZaberSettings()
        );
    ~Zaber() override = default;

    // IMotorAxis Interface Implementation (Limit-Enforced Natively)
    void stop(int axis = 0) override;
    void setSpeed(int axis, int speed) override;
    void moveRelative(int axis, int distance) override;
    void moveAbsolute(int axis, int position) override;
    int32_t getPosition(int axis) override;
    void home(int axis) override;

    // Continuous Joystick Jog Overrides
    void jogSingleAxis(int axis, int direction, int speed) override;
    void jogDualAxis(int axisMaster, int dirMaster, int speedMaster,
                     int axisSlave, int dirSlave, int speedSlave) override;

    // Zaber-Specific Queries
    int32_t getMaxLimit(int axis);
};
