#include <vrm.hpp>
#include <algorithm>
#include <cctype>
#include <stdexcept>

VisaResourceManager::VisaResourceManager() {
    ViStatus status = viOpenDefaultRM(&m_rmSession);
    if (status < VI_SUCCESS) {
        m_rmSession = VI_NULL;
        throw std::runtime_error("Could not open the VISA Resource Manager.");
    }
}

VisaResourceManager::~VisaResourceManager() {
    if (m_rmSession != VI_NULL) {
        viClose(m_rmSession);
        m_rmSession = VI_NULL;
    }
}

VisaResourceManager::VisaResourceManager(VisaResourceManager&& other) noexcept
    : m_rmSession(other.m_rmSession) {
    other.m_rmSession = VI_NULL;
}

VisaResourceManager& VisaResourceManager::operator=(VisaResourceManager&& other) noexcept {
    if (this != &other) {
        if (m_rmSession != VI_NULL) {
            viClose(m_rmSession);
        }
        m_rmSession = other.m_rmSession;
        other.m_rmSession = VI_NULL;
    }
    return *this;
}

VisaInterface VisaResourceManager::parseInterface(const std::string& resourceName) {
    const auto separator = resourceName.find("::");
    if (separator == std::string::npos) return VisaInterface::Unknown;
    std::string interfaceName = resourceName.substr(0, separator);
    std::transform(interfaceName.begin(), interfaceName.end(), interfaceName.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    const auto startsWith = [&interfaceName](const char* prefix) {
        return interfaceName.rfind(prefix, 0) == 0;
    };
    if (startsWith("ASRL")) return VisaInterface::Serial;
    if (startsWith("GPIB")) return VisaInterface::Gpib;
    if (startsWith("USB")) return VisaInterface::Usb;
    if (startsWith("VXI")) return VisaInterface::Vxi;
    if (startsWith("PXI")) return VisaInterface::Pxi;
    if (startsWith("TCPIP")) {
        std::string upper = resourceName;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return upper.size() >= 8 && upper.compare(upper.size() - 8, 8, "::SOCKET") == 0
                   ? VisaInterface::TcpipSocket : VisaInterface::TcpipInstrument;
    }
    return VisaInterface::Unknown;
}