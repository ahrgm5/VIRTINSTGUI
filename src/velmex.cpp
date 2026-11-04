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



VelmexVXM::VelmexVXM(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings = defaultVelmexSettings()) : Instrument(defaultRM) {
    if (!resourceName.empty()) {
        open(resourceName, settings);
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

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::string raw = trim(query(cmd));

    // If a move completion '^' or echoed axis letter was read first, read the next line
    if (raw == "^" || raw == cmd || raw.empty()) {
        raw = trim(read());
    }

    size_t firstValid = raw.find_first_of("+-0123456789");

    if (firstValid == std::string::npos) {
        throw std::runtime_error("Empty or invalid response from Velmex: " + raw);
    }

    raw.erase(0, firstValid);

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
                // Flush buffers and send Clear
                viFlush(m_session, VI_READ_BUF | VI_WRITE_BUF);
                write("C");
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Increased settling time

                // If the baud rate is wrong, this query will throw a timeout exception
                std::string resp = query("X");

                std::cout << "SUCCESS! Hardware replied: [" << resp << "]\n";
                return baud;
            } catch (const std::exception& ex) {
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

    // 1. Tell the controller hardware to transition its UART clock
    write("C," + baudCode + ",R");

    // 2. Allow hardware UART to reconfigure
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 3. Reconnect the PC serial port to match
    std::string currentResource = m_resourceName;
    close();

    ChannelSettings settings = defaultVelmexSettings();
    settings.baudRate = baudRate;
    open(currentResource, settings);

    // 4. Flush spurious electrical noise produced during baud transition
    viFlush(m_session, VI_READ_BUF | VI_WRITE_BUF);

    // 5. Clear the controller state machine
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    write("C");
}





bool VelmexVXM::autoconnect(const std::string& resourceName) {
    int current_Baud = detectBaudRates(resourceName);

    if (current_Baud == 0) {
        return false;
    }

    // The session is already open and confirmed working.
    // Just clear the line one last time before returning control to main.
    try {
        viFlush(m_session, VI_READ_BUF | VI_WRITE_BUF);
        write("C");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return true;
    } catch (...) {
        return false;
    }
}















VelmexAdapter::VelmexAdapter(std::shared_ptr<VelmexVXM> velmex) : m_velmex(std::move(velmex)) {}
void VelmexAdapter::moveRelative(int axis, int distance) { if (m_velmex && m_velmex->isOpen()) m_velmex->moveRelative(axis, distance); }
void VelmexAdapter::moveAbsolute(int axis, int position) { if (m_velmex && m_velmex->isOpen()) m_velmex->moveAbsolute(axis, position); }
void VelmexAdapter::stop(int axis) { if (m_velmex && m_velmex->isOpen()) m_velmex->stop(axis); }
void VelmexAdapter::setSpeed(int axis, int speed) { if (m_velmex && m_velmex->isOpen()) m_velmex->setSpeed(axis, speed); }
int32_t VelmexAdapter::getPosition(int axis) { return (m_velmex && m_velmex->isOpen()) ? m_velmex->getPosition(axis) : 0; }
void VelmexAdapter::home(int axis) { if (m_velmex && m_velmex->isOpen()) m_velmex->home(axis); }