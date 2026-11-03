#pragma once
#include <instrument.hpp>
#include <string>

class ipgYLR : public Instrument {
public:

    const static ChannelSettings defaultipgYLRSettings();
    ChannelSettings defaultSettings() const override{ return defaultipgYLRSettings();}

    explicit ipgYLR(ViSession defaultRM);

    ipgYLR(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings = defaultipgYLRSettings());
    ~ipgYLR() override = default;


    // YLR-Series Specific Commands
    std::string getIdentity();
    std::string setIpAddress(const std::string& newIp);

    void setEmission(bool enable);
    bool isEmissionEnabled();
    std::string getStatus();
};
