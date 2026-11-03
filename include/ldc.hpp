#pragma once
#include "instrument.hpp"
#include <string>


class LDC502 : public Instrument {
public:
    // Override the base class virtual defaultSettings() function


    static const ChannelSettings defaultLDC502Settings();
    ChannelSettings defaultSettings() const override {return defaultLDC502Settings();}

    explicit LDC502(ViSession defaultRM);

    LDC502(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings = defaultLDC502Settings());
    ~LDC502() override = default;



    // Laser Diode Current / Power Controls
    void setLaserOutput(bool enable);
    bool isLaserOutputEnabled();

    void setCurrentSetpoint(double currentmA);
    double getCurrentSetpoint();
    double getActualCurrent();

    // Status / Error Handling
    std::string getStatus();
    std::string getError();
};