#include <ldc.hpp>
#include <visa.h>


const ChannelSettings LDC502::defaultLDC502Settings() {
    ChannelSettings settings;
    settings.timeoutMs = 3000;
    settings.terminationCharacter = '\n'; // Standard SCPI terminator
    return settings;
}

LDC502::LDC502(ViSession defaultRM)
    : Instrument(defaultRM) {
}

LDC502::LDC502(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM){

    if(!resourceName.empty()){ open(resourceName, settings);}

}



void LDC502::setLaserOutput(bool enable) {
    write(enable ? "LDON 0" : "LDON 1");
}

bool LDC502::isLaserOutputEnabled() {
    std::string response = query("LDON?");
    return (response.find("1") != std::string::npos || response.find("ON") != std::string::npos);
}

void LDC502::setCurrentSetpoint(double currentmA) {
    write("SILD " + std::to_string(currentmA));
}

double LDC502::getCurrentSetpoint() {
    std::string response = query("SILD?");
    return std::stod(response);
}

double LDC502::getActualCurrent() {
    std::string response = query("RILD?");
    return std::stod(response);
}

std::string LDC502::getStatus() {
    return query("LASer:CONDition?");
}

std::string LDC502::getError() {
    return query("ERR?");
}