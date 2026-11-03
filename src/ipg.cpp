#include<instrument.hpp>
#include <ipg.hpp>
#include <stdexcept>

const ChannelSettings ipgYLR::defaultipgYLRSettings() {
    ChannelSettings settings;
    settings.timeoutMs = 5000;
    settings.terminationCharacter = '\r'; // YLR uses Carriage Return
    settings.tcpNoDelay = true;           // Recommended for TCP/IP socket control
    settings.tcpKeepAlive = true;
    return settings;
}

ipgYLR::ipgYLR(ViSession defaultRM): Instrument(defaultRM) {}

ipgYLR::ipgYLR(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings) : Instrument(defaultRM)
{
    if(!resourceName.empty()){ open(resourceName, settings);}
}





std::string ipgYLR::setIpAddress(const std::string& newIp) {
    // Remotely configure laser IP address via socket command
    return query("SIP " + newIp);
}

void ipgYLR::setEmission(bool enable) {
    write(enable ? "ENA 1" : "ENA 0");
}

bool ipgYLR::isEmissionEnabled() {
    std::string response = query("ENA?");
    return response.find("1") != std::string::npos;
}

std::string ipgYLR::getStatus() {
    return query("STA");
}