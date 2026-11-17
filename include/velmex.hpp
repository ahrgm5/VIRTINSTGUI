#pragma once

#include "instrument.hpp"
#include "imotoraxis.hpp"
#include <string>
#include <cstdint>

class VelmexVXM : public Instrument, public IMotorAxis {
public:
    static const ChannelSettings defaultVelmexSettings();

    ChannelSettings defaultSettings() const override {
        return defaultVelmexSettings();
    }

    explicit VelmexVXM(
        ViSession defaultRM,
        const std::string& resourceName = "",
        const ChannelSettings& settings = defaultVelmexSettings()
        );

    ~VelmexVXM() override = default;

    // IMotorAxis Interface Implementation (Limit-Enforced Natively)
    void stop(int axis = 0) override;
    void setSpeed(int axis, int speed) override;
    void moveRelative(int axis, int distance) override;
    void moveAbsolute(int axis, int position) override;
    int32_t getPosition(int axis) override;
    void home(int axis) override;

    // Continuous Joystick Jog Overrides (Master/Slave Coordinated XY & Single-Axis Slew)
    void jogSingleAxis(int axis, int direction, int speed) override;
    void jogDualAxis(int axisMaster, int dirMaster, int speedMaster,
                     int axisSlave, int dirSlave, int speedSlave) override;

    // Velmex-Specific Serial Utilities
    int detectBaudRates(const std::string& resourceName);
    void setBaudRate(int baudRate);
    bool autoconnect(const std::string& resourceName);

private:
    std::unordered_map<int, int32_t> m_cachedPositions;
};
