#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include <visa.h>
#include "instrument.hpp"
#include "imotoraxis.hpp"

class VelmexVXM : public Instrument {
public:
    static const ChannelSettings defaultVelmexSettings();
    ChannelSettings defaultSettings() const override { return defaultVelmexSettings(); }

    explicit VelmexVXM(ViSession defaultRM) : Instrument(defaultRM) {}
    VelmexVXM(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings);
    ~VelmexVXM() override = default;

    void stop(int axis);
    void setSpeed(int axis, int speed);
    void moveRelative(int axis, int distance);
    void moveAbsolute(int axis, int position);
    int32_t getPosition(int axis);
    void home(int axis);
};

class VelmexAdapter : public IMotorAxis {
public:
    explicit VelmexAdapter(std::shared_ptr<VelmexVXM> velmex);
    void moveRelative(int axis, int distance) override;
    void moveAbsolute(int axis, int position) override;
    void stop(int axis) override;
    void setSpeed(int axis, int speed) override;
    int32_t getPosition(int axis) override;
    void home(int axis) override;
private:
    std::shared_ptr<VelmexVXM> m_velmex;
};