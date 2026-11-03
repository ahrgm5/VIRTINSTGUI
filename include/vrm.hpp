#pragma once
#include <visa.h>
#include <optional>
#include <string>
#include <vector>

enum class VisaInterface {
    Unknown,
    Gpib,
    Serial,
    Usb,
    TcpipInstrument,
    TcpipSocket,
    Vxi,
    Pxi
};

enum class ChannelAttribute {
    Timeout,
    TerminationCharacter,
    SerialBaudRate,
    SerialDataBits,
    SerialParity,
    SerialStopBits,
    SerialFlowControl,
    TcpipNoDelay,
    TcpipKeepAlive
};

struct ChannelSettings {
    std::optional<ViUInt32> timeoutMs;
    std::optional<ViUInt8> terminationCharacter;
    std::optional<std::string> writeTermination;

    // ASRL (serial) resources
    std::optional<ViUInt32> baudRate;
    std::optional<ViUInt16> dataBits;
    std::optional<ViUInt16> parity;
    std::optional<ViUInt16> stopBits;
    std::optional<ViUInt16> flowControl;

    // TCPIP...::SOCKET resources
    std::optional<bool> tcpNoDelay;
    std::optional<bool> tcpKeepAlive;

    ChannelSettings() = default;
    explicit ChannelSettings(ViUInt32 timeout) : timeoutMs(timeout) {}
};

// RAII Wrapper for managing the root VISA Resource Manager session lifetime and interface utilities
class VisaResourceManager {
public:
    VisaResourceManager();
    ~VisaResourceManager();

    VisaResourceManager(const VisaResourceManager&) = delete;
    VisaResourceManager& operator=(const VisaResourceManager&) = delete;

    VisaResourceManager(VisaResourceManager&& other) noexcept;
    VisaResourceManager& operator=(VisaResourceManager&& other) noexcept;

    ViSession handle() const { return m_rmSession; }
    bool isValid() const { return m_rmSession != VI_NULL; }

    static VisaInterface parseInterface(const std::string& resourceName);

private:
    ViSession m_rmSession = VI_NULL;
};