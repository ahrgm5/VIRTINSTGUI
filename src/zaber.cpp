#include "zaber.hpp"
#include <sstream>
#include <stdexcept>

ChannelSettings Zaber::defaultZaberSettings() {
    ChannelSettings settings;
    settings.timeoutMs = 1000;
    settings.terminationCharacter = '\n';
    settings.writeTermination = "\n";
    settings.baudRate = 115200;
    settings.dataBits = 8;
    settings.parity = VI_ASRL_PAR_NONE;
    settings.stopBits = VI_ASRL_STOP_ONE;
    settings.flowControl = VI_ASRL_FLOW_NONE;
    return settings;
}


Zaber::Zaber(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings = defaultZaberSettings()) : Instrument(defaultRM) {
    if (!resourceName.empty()) {
        open(resourceName, settings);
    }
}

void Zaber::stop(int axis) {
    std::string response = query("/" + std::to_string(axis) + " stop");
    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber stop rejected: " + response);
    }
}

void Zaber::setSpeed(int axis, int speed) {
    if (speed <= 2000) {
        return; // Ignore low default speeds meant for Velmex
    }
    std::string response = query("/" + std::to_string(axis) + " set maxspeed " + std::to_string(speed));
    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber setSpeed rejected: " + response);
    }
}

void Zaber::moveRelative(int axis, int distance) {
    std::string response = query("/" + std::to_string(axis) + " move rel " + std::to_string(distance));
    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber moveRelative rejected (" + std::to_string(axis) + "): " + response);
    }
}

void Zaber::moveAbsolute(int axis, int position) {
    std::string response = query("/" + std::to_string(axis) + " move abs " + std::to_string(position));
    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber moveAbsolute rejected (" + std::to_string(axis) + "): " + response);
    }
}



int32_t Zaber::getPosition(int axis) {
    if (!isOpen()) return 0;

    std::string raw = trim(query("/" + std::to_string(axis) + " get pos"));

    if (raw.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber getPosition rejected: " + raw);
    }

    // Look for the '--' data separator indicator in the response
    auto delimPos = raw.find("--");
    if (delimPos == std::string::npos) {
        // Fallback: If no '--' found, try to parse the last numeric token in the string
        std::istringstream fallbackIss(raw);
        std::string token;
        int32_t lastVal = 0;
        bool foundNumber = false;
        while (fallbackIss >> token) {
            try {
                lastVal = std::stoi(token);
                foundNumber = true;
            } catch (...) {}
        }
        if (foundNumber) return lastVal;

        throw std::runtime_error("Invalid Zaber position response: " + raw);
    }

    std::string valueStr = trim(raw.substr(delimPos + 2));
    // Isolate just the first token after '--' (ignoring trailing status words if any)
    std::istringstream iss(valueStr);
    int32_t position = 0;
    if (iss >> position) {
        return position;
    }

    throw std::runtime_error("Failed to parse Zaber position integer from: '" + valueStr + "' (Raw: " + raw + ")");
}

void Zaber::home(int axis) {
    try { query("/" + std::to_string(axis) + " activate"); } catch (...) {}
    try { query("/" + std::to_string(axis) + " driver enable"); } catch (...) {}

    std::string response = query("/" + std::to_string(axis) + " home");
    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber home rejected: " + response);
    }
}

// Adapters
ZaberAdapter::ZaberAdapter(std::shared_ptr<Zaber> zaber) : m_zaber(std::move(zaber)) {}
void ZaberAdapter::moveRelative(int axis, int distance) { if (m_zaber && m_zaber->isOpen()) m_zaber->moveRelative(axis, distance); }
void ZaberAdapter::moveAbsolute(int axis, int position) { if (m_zaber && m_zaber->isOpen()) m_zaber->moveAbsolute(axis, position); }
void ZaberAdapter::stop(int axis) { if (m_zaber && m_zaber->isOpen()) m_zaber->stop(axis); }
void ZaberAdapter::setSpeed(int axis, int speed) { if (m_zaber && m_zaber->isOpen()) m_zaber->setSpeed(axis, speed); }

int32_t ZaberAdapter::getPosition(int axis) { return (m_zaber && m_zaber->isOpen()) ? m_zaber->getPosition(axis) : 0; }
void ZaberAdapter::home(int axis) { if (m_zaber && m_zaber->isOpen()) m_zaber->home(axis); }
