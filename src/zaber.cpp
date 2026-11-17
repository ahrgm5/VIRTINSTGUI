#include "zaber.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>

ChannelSettings Zaber::defaultZaberSettings() {
    ChannelSettings settings;
    settings.timeoutMs = 150;
    settings.terminationCharacter = '\n';
    settings.writeTermination = "\n";
    settings.baudRate = 115200;
    settings.dataBits = 8;
    settings.parity = VI_ASRL_PAR_NONE;
    settings.stopBits = VI_ASRL_STOP_ONE;
    settings.flowControl = VI_ASRL_FLOW_NONE;
    return settings;
}

Zaber::Zaber(ViSession defaultRM)
    : Instrument(defaultRM) {
    topSpeed = 200000;
}

Zaber::Zaber(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM) {
    topSpeed = 200000;
    if (!resourceName.empty()) {
        open(resourceName, settings);
    }
}

void Zaber::stop(int axis) {
    if (!isOpen()) return;

    if (axis <= 0) {
        query("/0 stop");
        return;
    }

    std::string response = query("/" + std::to_string(axis) + " stop");
    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber stop rejected: " + response);
    }
}

void Zaber::setSpeed(int axis, int speed) {
    if (!isOpen()) return;

    int safeSpeed = validateSpeed(axis, speed);
    if (safeSpeed <= 0) return;

    std::string response = query("/" + std::to_string(axis) + " set maxspeed " + std::to_string(safeSpeed));
    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber setSpeed rejected: " + response);
    }
}

void Zaber::moveRelative(int axis, int distance) {
    if (!isOpen() || distance == 0) return;

    int32_t currentPos = getPosition(axis);
    validatePosition(axis, currentPos + distance);

    int maxSpd = topSpeed;
    auto it = axisLimits.find(axis);
    if (it != axisLimits.end() && it->second.enabled && it->second.maxSpeed > 0) {
        maxSpd = it->second.maxSpeed;
    }
    query("/" + std::to_string(axis) + " set maxspeed " + std::to_string(maxSpd));

    std::string response = query("/" + std::to_string(axis) + " move rel " + std::to_string(distance));
    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber moveRelative rejected (" + std::to_string(axis) + "): " + response);
    }
}

void Zaber::moveAbsolute(int axis, int position) {
    if (!isOpen()) return;

    int32_t validatedPos = validatePosition(axis, position);

    // Ensure full maxspeed is applied for abrupt point-to-point moves from device tree
    int maxSpd = topSpeed;
    auto it = axisLimits.find(axis);
    if (it != axisLimits.end() && it->second.enabled && it->second.maxSpeed > 0) {
        maxSpd = it->second.maxSpeed;
    }
    query("/" + std::to_string(axis) + " set maxspeed " + std::to_string(maxSpd));

    std::string response = query("/" + std::to_string(axis) + " move abs " + std::to_string(validatedPos));
    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber moveAbsolute rejected (" + std::to_string(axis) + "): " + response);
    }
}

void Zaber::jogSingleAxis(int axis, int direction, int speed) {
    if (!isOpen()) return;
    if (direction == 0 || speed <= 0) {
        stop(axis);
        return;
    }

    int safeSpeed = std::max(1, validateSpeed(axis, speed));
    auto [limMin, limMax] = getAxisBounds(axis);
    int32_t target = (direction > 0) ? limMax : limMin;

    query("/" + std::to_string(axis) + " set maxspeed " + std::to_string(safeSpeed));
    query("/" + std::to_string(axis) + " move abs " + std::to_string(target));
}

void Zaber::jogDualAxis(int axisMaster, int dirMaster, int speedMaster,
                        int axisSlave, int dirSlave, int speedSlave) {
    if (!isOpen()) return;
    jogSingleAxis(axisMaster, dirMaster, speedMaster);
    jogSingleAxis(axisSlave, dirSlave, speedSlave);
}

int32_t Zaber::getPosition(int axis) {
    if (!isOpen()) return 0;

    std::string raw = trim(query("/" + std::to_string(axis) + " get pos"));

    if (raw.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber getPosition rejected: " + raw);
    }

    auto delimPos = raw.find("--");
    if (delimPos == std::string::npos) {
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
    std::istringstream iss(valueStr);
    int32_t position = 0;
    if (iss >> position) {
        return position;
    }

    throw std::runtime_error("Failed to parse Zaber position integer from: '" + valueStr + "' (Raw: " + raw + ")");
}

void Zaber::home(int axis) {
    if (!isOpen()) return;

    try { query("/" + std::to_string(axis) + " activate"); } catch (...) {}
    try { query("/" + std::to_string(axis) + " driver enable"); } catch (...) {}

    std::string response = query("/" + std::to_string(axis) + " home");
    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber home rejected: " + response);
    }
}
