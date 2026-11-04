#include <instrument.hpp>
#include <algorithm>
#include <cctype>
#include <utility>
#include <stdexcept>


ChannelSettings Instrument::defaultSettings() const {
    ChannelSettings defaults;
    defaults.timeoutMs = 200;
    defaults.writeTermination = "\n";
    return defaults;
}


Instrument::Instrument(ViSession defaultRM)
    : m_defaultRM(defaultRM), m_session(VI_NULL), m_interface(VisaInterface::Unknown), m_isOpen(false) {
}

Instrument::Instrument(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : m_defaultRM(defaultRM) {
    if (!resourceName.empty()) {
        open(resourceName, settings);
    }
}



Instrument::~Instrument() {
    close();
}

Instrument::Instrument(Instrument&& other) noexcept
    : m_defaultRM(other.m_defaultRM),
    m_session(other.m_session),
    m_resourceName(std::move(other.m_resourceName)),
    m_interface(other.m_interface),
    m_isOpen(other.m_isOpen),
    m_writeTermination(std::move(other.m_writeTermination)) {
    other.m_session = VI_NULL;
    other.m_isOpen = false;
    other.m_interface = VisaInterface::Unknown;
}

Instrument& Instrument::operator=(Instrument&& other) noexcept {
    if (this != &other) {
        close();
        m_defaultRM = other.m_defaultRM;
        m_session = other.m_session;
        m_resourceName = std::move(other.m_resourceName);
        m_interface = other.m_interface;
        m_isOpen = other.m_isOpen;
        m_writeTermination = std::move(other.m_writeTermination);

        other.m_session = VI_NULL;
        other.m_isOpen = false;
        other.m_interface = VisaInterface::Unknown;
    }
    return *this;
}

bool Instrument::open(const std::string& resourceName, const ChannelSettings& settings) {
    if (m_isOpen) {
        close();
    }

    // 1. Fetch the defaults for this specific instrument type
    ChannelSettings effectiveSettings = defaultSettings();

    // 2. Safely merge any user-provided overrides
    if (settings.timeoutMs.has_value()) effectiveSettings.timeoutMs = settings.timeoutMs;
    if (settings.baudRate.has_value()) effectiveSettings.baudRate = settings.baudRate;
    if (settings.dataBits.has_value()) effectiveSettings.dataBits = settings.dataBits;
    if (settings.parity.has_value()) effectiveSettings.parity = settings.parity;
    if (settings.stopBits.has_value()) effectiveSettings.stopBits = settings.stopBits;
    if (settings.flowControl.has_value()) effectiveSettings.flowControl = settings.flowControl;
    if (settings.terminationCharacter.has_value()) effectiveSettings.terminationCharacter = settings.terminationCharacter;
    if (settings.writeTermination.has_value() && !settings.writeTermination->empty()) {
        effectiveSettings.writeTermination = settings.writeTermination;
    }
    if (settings.tcpNoDelay.has_value()) effectiveSettings.tcpNoDelay = settings.tcpNoDelay;
    if (settings.tcpKeepAlive.has_value()) effectiveSettings.tcpKeepAlive = settings.tcpKeepAlive;

    // 3. Proceed with VISA opening
    m_resourceName = resourceName;
    m_interface = VisaResourceManager::parseInterface(resourceName);
    ViStatus status = viOpen(m_defaultRM, const_cast<char*>(m_resourceName.c_str()), VI_NULL, VI_NULL, &m_session);

    if (status < VI_SUCCESS) {
        m_isOpen = false;
        m_session = VI_NULL;
        char errDesc[256];
        viStatusDesc(m_defaultRM, status, errDesc);
        throw std::runtime_error("viOpen failed: " + std::string(errDesc) + " (Code: " + std::to_string(status) + ")");
    }

    m_isOpen = true;
    try {
        applySettings(effectiveSettings);
    }
    catch (...) {
        close();
        throw;
    }
    return true;
}

void Instrument::close() {
    if (m_isOpen && m_session != VI_NULL) {
        viClose(m_session);
        m_session = VI_NULL;
        m_isOpen = false;
        m_interface = VisaInterface::Unknown;
    }
}

bool Instrument::isOpen() const {
    return m_isOpen;
}

VisaInterface Instrument::interfaceType() const {
    return m_interface;
}

void Instrument::write(const std::string& command) {
    if (!m_isOpen) throw std::runtime_error("Cannot write: VISA session is closed.");

    viFlush(m_session, VI_READ_BUF_DISCARD);

    std::string cmd = command;
    while (!cmd.empty() && (cmd.back() == '\r' || cmd.back() == '\n')) {
        cmd.pop_back();
    }
    cmd += m_writeTermination;

    ViUInt32 bytesWritten = 0;
    ViStatus status = viWrite(
        m_session,
        reinterpret_cast<ViBuf>(const_cast<char*>(cmd.c_str())),
        static_cast<ViUInt32>(cmd.size()),
        &bytesWritten
        );
    checkStatus(status, "Write command failed");
}

std::string Instrument::read(size_t bufferSize) {
    if (!m_isOpen) throw std::runtime_error("Cannot read: VISA session is closed.");

    std::vector<char> buffer(bufferSize);
    ViUInt32 bytesRead = 0;

    ViStatus status = viRead(
        m_session,
        reinterpret_cast<ViPBuf>(buffer.data()),
        static_cast<ViUInt32>(buffer.size() - 1),
        &bytesRead
        );
    checkStatus(status, "Read response failed");

    buffer[bytesRead] = '\0';
    return std::string(buffer.data());
}

std::string Instrument::query(const std::string& command, size_t bufferSize) {
    write(command);
    return read(bufferSize);
}


std::string Instrument::trim(std::string str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::vector<ChannelAttribute> Instrument::supportedAttributes() const {
    std::vector<ChannelAttribute> attributes{ ChannelAttribute::Timeout, ChannelAttribute::TerminationCharacter };
    if (m_interface == VisaInterface::Serial) {
        attributes.insert(attributes.end(), {
                                                ChannelAttribute::SerialBaudRate, ChannelAttribute::SerialDataBits,
                                                ChannelAttribute::SerialParity, ChannelAttribute::SerialStopBits,
                                                ChannelAttribute::SerialFlowControl
                                            });
    }
    if (m_interface == VisaInterface::TcpipSocket) {
        attributes.insert(attributes.end(), { ChannelAttribute::TcpipNoDelay, ChannelAttribute::TcpipKeepAlive });
    }
    return attributes;
}

bool Instrument::supports(ChannelAttribute attribute) const {
    const auto attributes = supportedAttributes();
    return std::find(attributes.begin(), attributes.end(), attribute) != attributes.end();
}

void Instrument::setAttribute(ViAttr attribute, ViAttrState value, const std::string& description) {
    checkStatus(viSetAttribute(m_session, attribute, value), description);
}

void Instrument::applySettings(const ChannelSettings& settings) {
    // 1. Configure VISA Timeout
    setAttribute(VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(settings.timeoutMs.value_or(5000)), "Set timeout failed");

    // 2. Configure Write Termination
    m_writeTermination = settings.writeTermination.value_or("\n");

    // 3. Configure Read Termination
    if (settings.terminationCharacter) {
        setAttribute(VI_ATTR_TERMCHAR, static_cast<ViAttrState>(*settings.terminationCharacter), "Failed to set VISA termination character");
        setAttribute(VI_ATTR_TERMCHAR_EN, VI_TRUE, "Failed to enable VISA termination character");
    }

    // 4. Interface-specific attribute validation & application
    const auto require = [this](ChannelAttribute attribute, const char* setting) {
        if (!supports(attribute)) {
            throw std::invalid_argument(std::string(setting) + " is not supported by this VISA resource string.");
        }
    };
    if (settings.baudRate) {
        require(ChannelAttribute::SerialBaudRate, "baudRate");
        setAttribute(VI_ATTR_ASRL_BAUD, static_cast<ViAttrState>(*settings.baudRate), "Set serial baud rate failed");
    }
    if (settings.dataBits) {
        require(ChannelAttribute::SerialDataBits, "dataBits");
        setAttribute(VI_ATTR_ASRL_DATA_BITS, static_cast<ViAttrState>(*settings.dataBits), "Set serial data bits failed");
    }
    if (settings.parity) {
        require(ChannelAttribute::SerialParity, "parity");
        setAttribute(VI_ATTR_ASRL_PARITY, static_cast<ViAttrState>(*settings.parity), "Set serial parity failed");
    }
    if (settings.stopBits) {
        require(ChannelAttribute::SerialStopBits, "stopBits");
        setAttribute(VI_ATTR_ASRL_STOP_BITS, static_cast<ViAttrState>(*settings.stopBits), "Set serial stop bits failed");
    }
    if (settings.flowControl) {
        require(ChannelAttribute::SerialFlowControl, "flowControl");
        setAttribute(VI_ATTR_ASRL_FLOW_CNTRL, static_cast<ViAttrState>(*settings.flowControl), "Set serial flow control failed");
    }
    if (settings.tcpNoDelay) {
        require(ChannelAttribute::TcpipNoDelay, "tcpNoDelay");
        setAttribute(VI_ATTR_TCPIP_NODELAY, *settings.tcpNoDelay ? VI_TRUE : VI_FALSE, "Set TCP_NODELAY failed");
    }
    if (settings.tcpKeepAlive) {
        require(ChannelAttribute::TcpipKeepAlive, "tcpKeepAlive");
        setAttribute(VI_ATTR_TCPIP_KEEPALIVE, *settings.tcpKeepAlive ? VI_TRUE : VI_FALSE, "Set TCP keep-alive failed");
    }
}

void Instrument::checkStatus(ViStatus status, const std::string& action) const {
    if (status < VI_SUCCESS) {
        throw std::runtime_error(action + " [VISA Error Code: " + std::to_string(status) + "]");
    }
}