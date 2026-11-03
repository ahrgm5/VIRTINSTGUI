#pragma once

#include "instrument.hpp"
#include "imotoraxis.hpp"
#include <string>
#include <cstdint>
#include <memory>

class Zaber : public Instrument {
public:

    static ChannelSettings defaultZaberSettings();
    ChannelSettings defaultSettings() const override { return defaultZaberSettings(); }

    explicit Zaber(ViSession defaultRM) : Instrument(defaultRM) {}
    Zaber(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings );
    ~Zaber() override = default;




    void stop(int axis);
    void setSpeed(int axis, int speed);
    void moveRelative(int axis, int distance);
    void moveAbsolute(int axis, int position);
    void indexIncremental(int axis, int steps);
    int32_t getPosition(int axis);
    void home(int axis);
};

class ZaberAdapter : public IMotorAxis {
public:
    explicit ZaberAdapter(std::shared_ptr<Zaber> zaber);
    void moveRelative(int axis, int distance) override;
    void moveAbsolute(int axis, int position) override;
    void stop(int axis) override;
    void setSpeed(int axis, int speed) override;

    int32_t getPosition(int axis) override;
    void home(int axis) override;
private:
    std::shared_ptr<Zaber> m_zaber;
};