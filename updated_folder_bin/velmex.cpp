#include "velmex.hpp"
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <iostream>

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
    write("D");
}

void VelmexVXM::setSpeed(int axis, int speed) {
    if (!isOpen()) return;

    int safeSpeed = validateSpeed(axis, speed);
    int spd = std::clamp(safeSpeed, 1, 6000);
    std::string cmd = "G,C,S" + std::to_string(axis) + "M" + std::to_string(spd) + ",R";
    write(cmd);
}

void VelmexVXM::moveRelative(int axis, int distance) {
    if (!isOpen() || distance == 0) return;

    int32_t currentPos = getPosition(axis);
    validatePosition(axis, currentPos + distance);

    std::string cmd = "G,C,I" + std::to_string(axis) + "M" + std::to_string(distance) + ",R";
    write(cmd);
}

void VelmexVXM::moveAbsolute(int axis, int position) {
    if (!isOpen()) return;

    int32_t validatedPos = validatePosition(axis, position);
    std::string cmd = "G,C,IA" + std::to_string(axis) + "M" + std::to_string(validatedPos) + ",R";
    write(cmd);
}

void VelmexVXM::jogSingleAxis(int axis, int direction, int speed) {
    if (!isOpen()) return;
    if (direction == 0 || speed <= 0) {
        stop(axis);
        return;
    }

    int spd = std::clamp(validateSpeed(axis, speed), 1, 6000);
    auto [limMin, limMax] = getAxisBounds(axis);
    int32_t targetPos = (direction > 0) ? limMax : limMin;

    int32_t curPos = m_cachedPositions.count(axis) ? m_cachedPositions[axis] : 0;
    int32_t delta = targetPos - curPos;
    if ((direction > 0 && delta <= 0) || (direction < 0 && delta >= 0)) {
        stop(axis);
        return;
    }

    std::string cmd = "G,C,S" + std::to_string(axis) + "M" + std::to_string(spd) +
                      ",I" + std::to_string(axis) + "M" + std::to_string(delta) + ",R";
    write(cmd);
}

void VelmexVXM::jogDualAxis(int axisMaster, int dirMaster, int speedMaster,
                            int axisSlave, int dirSlave, int speedSlave) {
    if (!isOpen()) return;

    // Single-axis fallback if only one axis is deflected
    if (dirSlave == 0 || speedSlave <= 0) {
        jogSingleAxis(axisMaster, dirMaster, speedMaster);
        return;
    }
    if (dirMaster == 0 || speedMaster <= 0) {
        jogSingleAxis(axisSlave, dirSlave, speedSlave);
        return;
    }

    int spdMaster = std::clamp(validateSpeed(axisMaster, speedMaster), 1, 6000);
    int spdSlave  = std::clamp(validateSpeed(axisSlave, speedSlave), 1, 6000);

    auto [minM, maxM] = getAxisBounds(axisMaster);
    auto [minS, maxS] = getAxisBounds(axisSlave);

    int32_t curM = m_cachedPositions.count(axisMaster) ? m_cachedPositions[axisMaster] : 0;
    int32_t curS = m_cachedPositions.count(axisSlave) ? m_cachedPositions[axisSlave] : 0;

    int32_t deltaM = ((dirMaster > 0) ? maxM : minM) - curM;
    int32_t deltaS = ((dirSlave > 0) ? maxS : minS) - curS;

    bool validM = (dirMaster > 0 && deltaM > 0) || (dirMaster < 0 && deltaM < 0);
    bool validS = (dirSlave > 0 && deltaS > 0) || (dirSlave < 0 && deltaS < 0);

    if (!validM && !validS) {
        stop();
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

    // Per VXM Manual Appendix G (p. 47-48): Slave motor (3 or 4) MUST precede Master motor (1 or 2) inside parentheses
    std::string cmd = "G,C,S" + std::to_string(axisSlave) + "M" + std::to_string(spdSlave) +
                      ",S" + std::to_string(axisMaster) + "M" + std::to_string(spdMaster) +
                      ",(I" + std::to_string(axisSlave) + "M" + std::to_string(deltaS) +
                      ",I" + std::to_string(axisMaster) + "M" + std::to_string(deltaM) + ",)R";
    write(cmd);
}

int32_t VelmexVXM::getPosition(int axis) {
    if (!isOpen()) return 0;

    std::string cmd;
    switch (axis) {
    case 1: cmd = "X"; break;
    case 2: cmd = "Y"; break;
    case 3: cmd = "Z"; break;
    case 4: cmd = "T"; break;
    default: return 0;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::string raw = trim(query(cmd));

    if (raw == "^" || raw == cmd || raw.empty()) {
        raw = trim(read());
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

    std::string cmd = "G,C,S" + std::to_string(axis) + "M800" +
                      ",I" + std::to_string(axis) + "M-0" +
                      ",IA" + std::to_string(axis) + "M-0,R";
    write(cmd);
    m_cachedPositions[axis] = 0;
}

int VelmexVXM::detectBaudRates(const std::string& resourceName) {
    const std::vector<int> bdRts = {38400, 19200, 9600};
    ChannelSettings settings = defaultVelmexSettings();

    std::cout << "\n--- Starting Baud Rate Scan on " << resourceName << " ---\n";

    for (int baud : bdRts) {
        std::cout << "Testing " << baud << " baud... ";
        settings.baudRate = baud;

        if (isOpen()) close();

        if (open(resourceName, settings)) {
            try {
                viFlush(m_session, VI_READ_BUF | VI_WRITE_BUF);
                write("C");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                std::string resp = query("X");
                std::cout << "SUCCESS! Hardware replied: [" << resp << "]\n";
                return baud;
            } catch (const std::exception&) {
                std::cout << "Failed (Timeout/Garbage)\n";
            }
        }
    }

    if (isOpen()) close();
    std::cout << "Scan complete: No valid baud rate found.\n";
    return 0;
}

void VelmexVXM::setBaudRate(int baudRate) {
    if (!isOpen()) {
        throw std::runtime_error("Cannot change baud rate: VISA session is closed.");
    }

    std::string baudCode;
    if (baudRate == 9600) {
        baudCode = "setB9";
    } else if (baudRate == 19200) {
        baudCode = "setB19";
    } else if (baudRate == 38400) {
        baudCode = "setB38";
    } else {
        throw std::invalid_argument("Invalid Velmex baud rate. Must be 9600, 19200, or 38400.");
    }

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
