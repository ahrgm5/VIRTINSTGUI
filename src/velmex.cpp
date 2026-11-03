#include "velmex.hpp"
#include <algorithm>
#include <stdexcept>

const ChannelSettings VelmexVXM::defaultVelmexSettings() {
    ChannelSettings settings;
    settings.timeoutMs = 1000;
    settings.terminationCharacter = '\r';
    settings.writeTermination = "\r";
    settings.baudRate = 38400;
    settings.dataBits = 8;
    settings.parity = VI_ASRL_PAR_NONE;
    settings.stopBits = VI_ASRL_STOP_ONE;
    settings.flowControl = VI_ASRL_FLOW_NONE;
    return settings;
}



VelmexVXM::VelmexVXM(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings = defaultVelmexSettings()) : Instrument(defaultRM) {
    if (!resourceName.empty()) {
        open(resourceName, settings);
        write("G, C, setBx38");
        setAttribute(VI_ATTR_ASRL_BAUD, 38400, "Set the baud rate to 38400 for Velmex");

    }
}







void VelmexVXM::stop(int axis) {
    (void)axis;
    if (!isOpen()) return;
    write("D");
    write("K");
}


void VelmexVXM::setSpeed(int axis, int speed) {
    if (!isOpen()) return;

    int spd = std::clamp(speed, 1, 6000);
    std::string cmd = "G,C,S" + std::to_string(axis) + "M" + std::to_string(spd) + ",R";
    write(cmd);
}

void VelmexVXM::moveRelative(int axis, int distance) {
    if (!isOpen() || distance == 0) return;

    std::string cmd = "G,C,I" + std::to_string(axis) + "M" + std::to_string(distance) + ",R";
    write(cmd);
}

void VelmexVXM::moveAbsolute(int axis, int position) {
    if (!isOpen()) return;

    std::string cmd = "G,C,IA" + std::to_string(axis) + "M" + std::to_string(position) + ",R";
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

    std::string raw = trim(query(cmd));

    // 1. Find the first character that is NOT a caret or whitespace
    size_t firstValid = raw.find_first_not_of("^ \t\r\n");

    // 2. If the string is empty or only contained a caret, prevent std::stol from crashing
    if (firstValid == std::string::npos) {
        throw std::runtime_error("Empty or invalid response from Velmex");
    }

    raw.erase(0, firstValid);

    // 3. Wrap the string-to-long conversion in a try-catch to guarantee it never crashes the app
    try {
        return static_cast<int32_t>(std::stol(raw));
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
}

VelmexAdapter::VelmexAdapter(std::shared_ptr<VelmexVXM> velmex) : m_velmex(std::move(velmex)) {}
void VelmexAdapter::moveRelative(int axis, int distance) { if (m_velmex && m_velmex->isOpen()) m_velmex->moveRelative(axis, distance); }
void VelmexAdapter::moveAbsolute(int axis, int position) { if (m_velmex && m_velmex->isOpen()) m_velmex->moveAbsolute(axis, position); }
void VelmexAdapter::stop(int axis) { if (m_velmex && m_velmex->isOpen()) m_velmex->stop(axis); }
void VelmexAdapter::setSpeed(int axis, int speed) { if (m_velmex && m_velmex->isOpen()) m_velmex->setSpeed(axis, speed); }
int32_t VelmexAdapter::getPosition(int axis) { return (m_velmex && m_velmex->isOpen()) ? m_velmex->getPosition(axis) : 0; }
void VelmexAdapter::home(int axis) { if (m_velmex && m_velmex->isOpen()) m_velmex->home(axis); }