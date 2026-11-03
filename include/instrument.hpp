#pragma once

#include <optional>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>

#include <visa.h>
#include <vrm.hpp>

class Instrument {
public:
    // Unopened session constructor
    explicit Instrument(ViSession defaultRM);

    // Primary constructor: safely accepts concrete settings
    Instrument(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings = {});

    virtual ~Instrument();

    Instrument(const Instrument&) = delete;
    Instrument& operator=(const Instrument&) = delete;

    Instrument(Instrument&& other) noexcept;
    Instrument& operator=(Instrument&& other) noexcept;

    // Session Management
    bool open(const std::string& resourceName, const ChannelSettings& settings = {});
    void close(); // Non-virtual to prevent virtual call in destructor
    bool isOpen() const;

    VisaInterface interfaceType() const;
    std::vector<ChannelAttribute> supportedAttributes() const;

    // Communication
    virtual void write(const std::string& command);
    virtual std::string read(size_t bufferSize = 1024);
    virtual std::string query(const std::string& command, size_t bufferSize = 1024);

    // Virtual hook for runtime queries after the object is fully constructed
    virtual ChannelSettings defaultSettings() const;

    // String cleanup helper
    static std::string trim(std::string str);

    // Generic typed query helper (must remain in header for template instantiation)
    template <typename T>
    T queryAs(const std::string& command) {
        std::string raw = trim(query(command));
        std::istringstream iss(raw);
        T val;
        if (!(iss >> val)) {
            throw std::runtime_error("Failed to parse response '" + raw + "' for command: " + command);
        }
        return val;
    }

protected:
    ViSession m_defaultRM = VI_NULL;
    ViSession m_session = VI_NULL;
    std::string m_resourceName;
    VisaInterface m_interface = VisaInterface::Unknown;
    bool m_isOpen = false;
    std::string m_writeTermination = "\n";

    void applySettings(const ChannelSettings& settings);
    void setAttribute(ViAttr attribute, ViAttrState value, const std::string& description);
    bool supports(ChannelAttribute attribute) const;
    void checkStatus(ViStatus status, const std::string& action) const;
};