#include "velmex.hpp"
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <iostream>
#include <unordered_map>

namespace {
std::unordered_map<const VelmexVXM*, bool> s_isJogging;
}

const ChannelSettings VelmexVXM::defaultVelmexSettings() {
    ChannelSettings settings;
    settings.timeoutMs = 150;
    settings.terminationCharacter = '\r';
    settings.writeTermination = "\r";
    settings.baudRate = 38400;
    settings.dataBits = 8;
    settings.parity = VI_ASRL_PAR_NONE;
    settings.stopBits = VI_ASRL_STOP_ONE;
    settings.flowControl = VI_ASRL_FLOW_NONE;
    return settings;
}

VelmexVXM::VelmexVXM(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM) {
    topSpeed = 6000;
    if (!resourceName.empty()) {
        open(resourceName, settings);
    }
}

void VelmexVXM::stop(int axis) {
    (void)axis;
    if (!isOpen()) return;

    if (!s_isJogging[this]) {
        if (m_session != VI_NULL) {
            viFlush(m_session, VI_READ_BUF);
        }
        return;
    }

    write("D");

    if (m_session != VI_NULL) {
        // Short 25 ms read timeout while draining stop completion so video stream stays responsive
        viSetAttribute(m_session, VI_ATTR_TMO_VALUE, 25);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start).count() < 250) {
            try {
                std::string resp = trim(read());
                if (resp.find('^') != std::string::npos || resp.find('O') != std::string::npos) {
                    break;
                }
            } catch (...) {
                break;
            }
        }

        viSetAttribute(m_session, VI_ATTR_TMO_VALUE, 150);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        viFlush(m_session, VI_READ_BUF);
    }

    s_isJogging[this] = false;
}

void VelmexVXM::setSpeed(int axis, int speed) {
    if (!isOpen()) return;

    int safeSpeed = validateSpeed(axis, speed);
    int spd = std::clamp(safeSpeed, 1, 6000);
    if (m_session != VI_NULL) {
        viFlush(m_session, VI_READ_BUF);
    }
    std::string cmd = "G,C,S" + std::to_string(axis) + "M" + std::to_string(spd) + ",R";
    write(cmd);
}

void VelmexVXM::moveRelative(int axis, int distance) {
    if (!isOpen() || distance == 0) return;

    if (s_isJogging[this]) {
        stop(axis);
    }

    int32_t currentPos = getPosition(axis);
    validatePosition(axis, currentPos + distance);

    int maxSpd = topSpeed;
    auto it = axisLimits.find(axis);
    if (it != axisLimits.end() && it->second.enabled && it->second.maxSpeed > 0) {
        maxSpd = std::clamp(it->second.maxSpeed, 1, 6000);
    }

    if (m_session != VI_NULL) {
        viFlush(m_session, VI_READ_BUF);
    }
    std::string cmd = "G,C,S" + std::to_string(axis) + "M" + std::to_string(maxSpd) +
                      ",I" + std::to_string(axis) + "M" + std::to_string(distance) + ",R";
    write(cmd);
    m_cachedPositions[axis] = currentPos + distance;
}

void VelmexVXM::moveAbsolute(int axis, int position) {
    if (!isOpen()) return;

    if (s_isJogging[this]) {
        stop(axis);
    }

    int32_t validatedPos = validatePosition(axis, position);

    int maxSpd = topSpeed;
    auto it = axisLimits.find(axis);
    if (it != axisLimits.end() && it->second.enabled && it->second.maxSpeed > 0) {
        maxSpd = std::clamp(it->second.maxSpeed, 1, 6000);
    }

    if (m_session != VI_NULL) {
        viFlush(m_session, VI_READ_BUF);
    }
    std::string cmd = "G,C,S" + std::to_string(axis) + "M" + std::to_string(maxSpd) +
                      ",IA" + std::to_string(axis) + "M" + std::to_string(validatedPos) + ",R";
    write(cmd);
    m_cachedPositions[axis] = validatedPos;
}

void VelmexVXM::jogSingleAxis(int axis, int direction, int speed) {
    if (!isOpen()) return;
    if (direction == 0 || speed <= 0) {
        stop(axis);
        return;
    }

    if (s_isJogging[this]) {
        stop(axis);
    }

    int spd = std::clamp(validateSpeed(axis, speed), 1, 6000);
    auto [limMin, limMax] = getAxisBounds(axis);
    int32_t targetPos = (direction > 0) ? limMax : limMin;

    int32_t curPos = 0;
    try {
        curPos = getPosition(axis);
    } catch (...) {
        curPos = m_cachedPositions.count(axis) ? m_cachedPositions[axis] : 0;
    }

    int32_t delta = targetPos - curPos;
    if ((direction > 0 && delta <= 0) || (direction < 0 && delta >= 0)) {
        return;
    }

    if (m_session != VI_NULL) {
        viFlush(m_session, VI_READ_BUF);
    }
    std::string cmd = "G,C,S" + std::to_string(axis) + "M" + std::to_string(spd) +
                      ",IA" + std::to_string(axis) + "M" + std::to_string(targetPos) + ",R";
    write(cmd);
    s_isJogging[this] = true;
}

void VelmexVXM::jogDualAxis(int axisMaster, int dirMaster, int speedMaster,
                            int axisSlave, int dirSlave, int speedSlave) {
    if (!isOpen()) return;

    if (dirSlave == 0 || speedSlave <= 0) {
        jogSingleAxis(axisMaster, dirMaster, speedMaster);
        return;
    }
    if (dirMaster == 0 || speedMaster <= 0) {
        jogSingleAxis(axisSlave, dirSlave, speedSlave);
        return;
    }

    if (s_isJogging[this]) {
        stop(0);
    }

    int spdMaster = std::clamp(validateSpeed(axisMaster, speedMaster), 1, 6000);
    int spdSlave  = std::clamp(validateSpeed(axisSlave, speedSlave), 1, 6000);

    auto [minM, maxM] = getAxisBounds(axisMaster);
    auto [minS, maxS] = getAxisBounds(axisSlave);

    int32_t curM = 0;
    int32_t curS = 0;
    try { curM = getPosition(axisMaster); } catch (...) { curM = m_cachedPositions.count(axisMaster) ? m_cachedPositions[axisMaster] : 0; }
    try { curS = getPosition(axisSlave);  } catch (...) { curS = m_cachedPositions.count(axisSlave)  ? m_cachedPositions[axisSlave]  : 0; }

    int32_t deltaM = ((dirMaster > 0) ? maxM : minM) - curM;
    int32_t deltaS = ((dirSlave > 0) ? maxS : minS) - curS;

    bool validM = (dirMaster > 0 && deltaM > 0) || (dirMaster < 0 && deltaM < 0);
    bool validS = (dirSlave > 0 && deltaS > 0) || (dirSlave < 0 && deltaS < 0);

    if (!validM && !validS) {
        return;
    }
    if (!validS) {
        jogSingleAxis(axisMaster, dirMaster, speedMaster);
        return;
    }
    if (!validM) {
        jogSingleAxis(axisSlave, dirSlave, speedSlave);
        return;
    }

    if (m_session != VI_NULL) {
        viFlush(m_session, VI_READ_BUF);
    }
    std::string cmd = "G,C,S" + std::to_string(axisSlave) + "M" + std::to_string(spdSlave) +
                      ",S" + std::to_string(axisMaster) + "M" + std::to_string(spdMaster) +
                      ",(I" + std::to_string(axisSlave) + "M" + std::to_string(deltaS) +
                      ",I" + std::to_string(axisMaster) + "M" + std::to_string(deltaM) + ",)R";
    write(cmd);
    s_isJogging[this] = true;
}

int32_t VelmexVXM::getPosition(int axis) {
    if (!isOpen()) return 0;

    if (s_isJogging[this] && (axis == 3 || axis == 4)) {
        return m_cachedPositions.count(axis) ? m_cachedPositions[axis] : 0;
    }

    std::string cmd;
    switch (axis) {
    case 1: cmd = "X"; break;
    case 2: cmd = "Y"; break;
    case 3: cmd = "Z"; break;
    case 4: cmd = "T"; break;
    default: return 0;
    }

    if (m_session != VI_NULL) {
        viFlush(m_session, VI_READ_BUF);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    std::string raw = trim(query(cmd));

    int retries = 0;
    while ((raw == "^" || raw == cmd || raw.empty()) && retries < 3) {
        raw = trim(read());
        ++retries;
    }

    size_t firstValid = raw.find_first_of("+-0123456789");
    if (firstValid == std::string::npos) {
        throw std::runtime_error("Empty or invalid response from Velmex: " + raw);
    }

    raw.erase(0, firstValid);

    try {
        int32_t pos = static_cast<int32_t>(std::stol(raw));
        m_cachedPositions[axis] = pos;
        return pos;
    } catch (...) {
        throw std::runtime_error("Failed to parse Velmex position integer: " + raw);
    }
}

void VelmexVXM::home(int axis) {
    if (!isOpen()) return;

    if (s_isJogging[this]) {
        stop(axis);
    }

    if (m_session != VI_NULL) {
        viFlush(m_session, VI_READ_BUF);
    }
    std::string cmd = "G,C,S" + std::to_string(axis) + "M800" +
                      ",I" + std::to_string(axis) + "M-0" +
                      ",IA" + std::to_string(axis) + "M-0,R";
    write(cmd);
    m_cachedPositions[axis] = 0;
}

int VelmexVXM::detectBaudRates(const std::string& resourceName) {
    const std::vector<int> bdRts = {38400, 19200, 9600};
    ChannelSettings settings = defaultVelmexSettings();

    for (int baud : bdRts) {
        settings.baudRate = baud;
        if (isOpen()) close();
        if (open(resourceName, settings)) {
            try {
                viFlush(m_session, VI_READ_BUF | VI_WRITE_BUF);
                write("C");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string resp = query("X");
                return baud;
            } catch (...) {}
        }
    }
    if (isOpen()) close();
    return 0;
}

void VelmexVXM::setBaudRate(int baudRate) {
    if (!isOpen()) return;
    std::string baudCode = (baudRate == 9600) ? "setB9" : (baudRate == 19200) ? "setB19" : "setB38";
    write("C," + baudCode + ",R");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::string currentResource = m_resourceName;
    close();
    ChannelSettings settings = defaultVelmexSettings();
    settings.baudRate = baudRate;
    open(currentResource, settings);
    viFlush(m_session, VI_READ_BUF | VI_WRITE_BUF);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    write("C");
}

bool VelmexVXM::autoconnect(const std::string& resourceName) {
    int current_Baud = detectBaudRates(resourceName);
    if (current_Baud == 0) return false;
    try {
        viFlush(m_session, VI_READ_BUF | VI_WRITE_BUF);
        write("C");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return true;
    } catch (...) {
        return false;
    }
}
