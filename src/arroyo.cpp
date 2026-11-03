#include <arroyo.hpp>
#include <string>

ChannelSettings Arroyo5240::defaultArroyoSettings() {
    ChannelSettings settings;
    settings.timeoutMs = 2000;
    settings.terminationCharacter = '\n';
    settings.writeTermination = "\n";
    settings.baudRate = 38400;
    settings.dataBits = 8;
    settings.parity = VI_ASRL_PAR_NONE;
    settings.stopBits = VI_ASRL_STOP_ONE;
    settings.flowControl = VI_ASRL_FLOW_NONE;
    return settings;
}

Arroyo5240::Arroyo5240(ViSession defaultRM)
    : Instrument(defaultRM) {
}

Arroyo5240::Arroyo5240(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM, resourceName, settings) {
}

std::string Arroyo5240::getIdentity() {
    return trim(query("*IDN?"));
}

std::string Arroyo5240::beep() {
    return trim(query("BEEP?"));
}

void Arroyo5240::setTemperatureSetpoint(double tempC) {
    write("TEC:T " + std::to_string(tempC));
}

double Arroyo5240::getTemperatureSetpoint() {
    return queryAs<double>("TEC:SET:T?");
}

double Arroyo5240::getCurrentTemperature() {
    return queryAs<double>("TEC:T?");
}

void Arroyo5240::setOutputEnable(bool enable) {
    write(enable ? "TEC:OUT 1" : "TEC:OUT 0");
}

bool Arroyo5240::isOutputEnabled() {
    return queryAs<int>("TEC:OUT?") != 0;
}