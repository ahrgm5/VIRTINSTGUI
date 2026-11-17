#include "hardwaremanager.h"
#include "zaber.hpp"
#include "velmex.hpp"
#include "ldc.hpp"
#include "pm100D.hpp"
#include <QTime>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

HardwareManager::HardwareManager(QObject *parent)
    : QObject(parent)
{
    m_cameraCheckTimer = new QTimer(this);
    connect(m_cameraCheckTimer, &QTimer::timeout, this, &HardwareManager::checkCameraConnection);
    m_cameraCheckTimer->start(2000);

    m_deviceCheckTimer = new QTimer(this);
    connect(m_deviceCheckTimer, &QTimer::timeout, this, &HardwareManager::checkDeviceConnections);
    m_deviceCheckTimer->start(3000);

    m_positionUpdateTimer = new QTimer(this);
    connect(m_positionUpdateTimer, &QTimer::timeout, this, &HardwareManager::pollDevicePositions);
    m_positionUpdateTimer->start(400);
}

HardwareManager::~HardwareManager()
{
    m_cameraCheckTimer->stop();
    m_deviceCheckTimer->stop();
    m_positionUpdateTimer->stop();

    if (m_isStreaming) {
        try { m_grabber.streamStop(); } catch (...) {}
    }
    clearDevices();
}

void HardwareManager::clearDevices()
{
    for (auto& [name, inst] : m_stationDevices) {
        if (inst && inst->isOpen()) {
            inst->close();
        }
    }
    m_stationDevices.clear();
    m_deviceResourceStrings.clear();
    m_deviceAxes.clear();
}

bool HardwareManager::connectAndSetupDevice(const DeviceConfigInfo& devConfig, std::map<int, std::pair<int, int>>& ioDefaultLimits)
{
    QString devName = devConfig.name;
    QString resourceString = devConfig.resource;
    QString devUpper = devName.toUpper();

    std::shared_ptr<Instrument> instrument;
    if (devUpper.contains("ZABER")) {
        instrument = std::make_shared<Zaber>(m_vrm.handle());
    } else if (devUpper.contains("VELMEX") || devUpper.contains("VXM")) {
        instrument = std::make_shared<VelmexVXM>(m_vrm.handle());
    } else if (devUpper.contains("LDC")) {
        instrument = std::make_shared<LDC502>(m_vrm.handle());
    } else if (devUpper.contains("PM100")) {
        instrument = std::make_shared<PM100D>(m_vrm.handle());
    } else {
        instrument = std::make_shared<Instrument>(m_vrm.handle());
    }

    m_deviceResourceStrings[devName] = resourceString;
    bool isConnected = false;

    try {
        isConnected = instrument->open(resourceString.toStdString(), instrument->defaultSettings());
        if (devUpper.contains("VELMEX") || devUpper.contains("VXM")) {
            try {
                instrument->write("F");
#ifdef Q_OS_WIN
                Sleep(20);
#endif
                instrument->write("C");
#ifdef Q_OS_WIN
                Sleep(50);
#endif
            } catch (...) {}
        }
        if (isConnected && devUpper.contains("ZABER")) {
            for (int devId = 1; devId <= 4; ++devId) {
                try { instrument->query("/" + std::to_string(devId) + " activate"); } catch (...) {}
                try { instrument->query("/" + std::to_string(devId) + " 1 driver enable"); } catch (...) {}
            }
        }
    } catch (...) {
        isConnected = false;
    }

    m_stationDevices[devName] = instrument;
    auto motor = std::dynamic_pointer_cast<IMotorAxis>(instrument);

    for (const auto& ax : devConfig.axes) {
        m_deviceAxes[devName].push_back(ax.hwIndex);
        ioDefaultLimits[ax.hwIndex] = { ax.defaultMin, ax.defaultMax };

        if (motor) {
            motor->setAxisLimits(ax.hwIndex, ax.defaultMin, ax.defaultMax, ax.defaultMaxSpeed);
        }
    }

    return isConnected;
}

std::shared_ptr<Instrument> HardwareManager::getDevice(const QString& devName) const
{
    if (m_stationDevices.count(devName)) {
        return m_stationDevices.at(devName);
    }
    return nullptr;
}

QStringList HardwareManager::getConnectedMotors() const
{
    QStringList list;
    for (const auto& [devName, inst] : m_stationDevices) {
        if (inst && inst->isOpen() && std::dynamic_pointer_cast<IMotorAxis>(inst)) {
            list.append(devName);
        }
    }
    return list;
}

void HardwareManager::setAxisLimits(const QString& devName, int axisIndex, int minPos, int maxPos, int maxSpeed)
{
    auto inst = getDevice(devName);
    if (auto motor = std::dynamic_pointer_cast<IMotorAxis>(inst)) {
        motor->setAxisLimits(axisIndex, minPos, maxPos, maxSpeed);
    }
}

void HardwareManager::moveAxisAbsolute(const QString& devName, int axisIndex, int targetPos)
{
    auto inst = getDevice(devName);
    if (!inst || !inst->isOpen()) return;

    if (auto motor = std::dynamic_pointer_cast<IMotorAxis>(inst)) {
        try {
            motor->moveAbsolute(axisIndex, targetPos);
        } catch (const std::exception& ex) {
            emit logMessage(QString("[%1] <span style='color: #e74c3c;'><b>Move Blocked (%2):</b> %3</span>")
                                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName, QString::fromUtf8(ex.what())));
        } catch (...) {}
    }
}

void HardwareManager::homeDevice(const QString& devName)
{
    auto inst = getDevice(devName);
    if (!inst) {
        emit logMessage(QString("[%1] <span style='color: #e74c3c;'><b>Homing Failed:</b> No motor target selected.</span>")
                            .arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
        return;
    }
    if (!inst->isOpen()) {
        emit logMessage(QString("[%1] <span style='color: #e74c3c;'><b>Homing Failed:</b> Session for %2 is closed.</span>")
                            .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName));
        return;
    }

    emit logMessage(QString("[%1] Initiating home command for %2...")
                        .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName));

    try {
        if (auto zaber = std::dynamic_pointer_cast<Zaber>(inst)) {
            for (int axis = 1; axis <= 4; ++axis) {
                zaber->home(axis);
            }
        } else if (auto velmex = std::dynamic_pointer_cast<VelmexVXM>(inst)) {
            velmex->write("G,C,S1M800,S2M800,I1M-0,IA1M-0,I2M-0,IA2M-0,R");
        }
        emit logMessage(QString("[%1] <span style='color: #2ecc71;'><b>Home Command Sent:</b> %2 is homing.</span>")
                            .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName));
    } catch (const std::exception& ex) {
        emit logMessage(QString("[%1] <span style='color: #e74c3c;'><b>Homing Error (%2):</b> %3</span>")
                            .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName, QString::fromUtf8(ex.what())));
    }
}

void HardwareManager::initCamera(quintptr videoContainerWinId)
{
    m_videoWinId = videoContainerWinId;
    try {
        auto devList = ic4::DeviceEnum::enumDevices();
        if (devList.empty()) return;
        m_grabber.deviceOpen(devList.front());

        // High-speed, low-latency sensor configuration for real-time focusing
        ic4::Error propErr;
        auto propMap = m_grabber.devicePropertyMap(propErr);
        if (propMap.is_valid()) {
            propMap.setValue(ic4::PropId::ExposureAuto, "Off", propErr);
            propMap.setValue(ic4::PropId::ExposureTime, 5000.0, propErr);
            propMap.setValue(ic4::PropId::GainAuto, "Continuous", propErr);

            auto fpsProp = propMap.findFloat("AcquisitionFrameRate", propErr);
            if (fpsProp.is_valid()) {
                double maxFps = fpsProp.maximum(propErr);
                if (maxFps > 0.0) {
                    fpsProp.setValue(maxFps, propErr);
                }
            }
        }

#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_videoWinId);
        ic4::Error err;
        m_ic4Display = ic4::Display::create(
            ic4::DisplayType::Default,
            reinterpret_cast<ic4::WindowHandle>(hwnd),
            err
            );

        if (m_ic4Display) {
            m_grabber.streamSetup(m_ic4Display);
            m_isStreaming = true;

            RECT rc{0, 0, 0, 0};
            GetClientRect(hwnd, &rc);
            updateCameraDisplayRect(m_videoWinId, rc.right - rc.left, rc.bottom - rc.top);

            emit cameraStreamStateChanged(true);
        }
#endif
    } catch (...) {}
}

void HardwareManager::toggleStreaming(quintptr videoContainerWinId)
{
    m_videoWinId = videoContainerWinId;
    if (!m_grabber.isDeviceOpen()) {
        initCamera(m_videoWinId);
        return;
    }
    if (m_isStreaming) {
        try { m_grabber.streamStop(); } catch (...) {}
        m_isStreaming = false;
        emit cameraStreamStateChanged(false);
    } else if (m_ic4Display) {
        try {
            m_grabber.streamSetup(m_ic4Display);
            m_isStreaming = true;
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(m_videoWinId);
            RECT rc{0, 0, 0, 0};
            GetClientRect(hwnd, &rc);
            updateCameraDisplayRect(m_videoWinId, rc.right - rc.left, rc.bottom - rc.top);
#endif
            emit cameraStreamStateChanged(true);
        } catch (...) {}
    }
}

void HardwareManager::updateCameraDisplayRect(quintptr videoContainerWinId, int width, int height)
{
    m_videoWinId = videoContainerWinId;

#ifdef Q_OS_WIN
    HWND parent = reinterpret_cast<HWND>(videoContainerWinId);
    if (!parent) return;

    // Query physical pixel client size of the HWND so DPI scaling never clips or leaves black bars
    RECT rc{0, 0, 0, 0};
    GetClientRect(parent, &rc);
    int physW = rc.right - rc.left;
    int physH = rc.bottom - rc.top;
    if (physW <= 0) physW = width;
    if (physH <= 0) physH = height;
    if (physW <= 0 || physH <= 0) return;

    // Resize all internal IC4 child windows to fill 100% of the physical HWND
    HWND child = GetWindow(parent, GW_CHILD);
    while (child) {
        MoveWindow(child, 0, 0, physW, physH, TRUE);
        SendMessageW(child, WM_SIZE, SIZE_RESTORED, MAKELPARAM(physW, physH));
        child = GetWindow(child, GW_HWNDNEXT);
    }

    // Stretch-to-Fill: pin all 4 corners of the camera's full field of view directly to (0, 0, physW, physH)
    if (m_ic4Display) {
        ic4::Error err;
        m_ic4Display->setRenderPosition(ic4::DisplayRenderPosition::Custom, 0, 0, physW, physH, err);
    }

    InvalidateRect(parent, nullptr, FALSE);
#else
    if (m_ic4Display && width > 0 && height > 0) {
        ic4::Error err;
        m_ic4Display->setRenderPosition(ic4::DisplayRenderPosition::Custom, 0, 0, width, height, err);
    }
#endif
}

void HardwareManager::setPositionPollingPaused(bool paused)
{
    m_pausePositionPolling = paused;
}

void HardwareManager::checkCameraConnection()
{
    if (m_grabber.isDeviceOpen() && m_isStreaming) return;
    if (!m_grabber.isDeviceOpen() && m_videoWinId != 0) {
        initCamera(m_videoWinId);
    }
}

void HardwareManager::checkDeviceConnections()
{
    if (m_stationDevices.empty() || m_pausePositionPolling) return;

    std::vector<QString> devNames;
    for (const auto& [name, _] : m_stationDevices) {
        devNames.push_back(name);
    }

    for (const QString& devName : devNames) {
        auto dev = m_stationDevices[devName];
        bool currentlyConnected = dev && dev->isOpen();
        QString resStr = m_deviceResourceStrings[devName];

        if (currentlyConnected) {
            ViSession testSession = VI_NULL;
            std::string resStd = resStr.toStdString();
            ViStatus status = viOpen(m_vrm.handle(), const_cast<char*>(resStd.c_str()), VI_NULL, 50, &testSession);
            if (status >= VI_SUCCESS) {
                viClose(testSession);
            } else if (status != VI_ERROR_RSRC_LOCKED) {
                dev->close();
                currentlyConnected = false;
            }
        }
        emit deviceConnectionChanged(devName, resStr, currentlyConnected);
    }

    static size_t reconnectIndex = 0;
    size_t count = devNames.size();
    for (size_t checked = 0; checked < count; ++checked) {
        size_t idx = (reconnectIndex + checked) % count;
        QString devName = devNames[idx];
        auto dev = m_stationDevices[devName];

        if (dev && !dev->isOpen()) {
            reconnectIndex = (idx + 1) % count;
            QString resStr = m_deviceResourceStrings[devName];
            try {
                if (dev->open(resStr.toStdString(), dev->defaultSettings())) {
                    if (devName.toUpper().contains("ZABER")) {
                        for (int devId = 1; devId <= 4; ++devId) {
                            try { dev->query("/" + std::to_string(devId) + " activate"); } catch (...) {}
                            try { dev->query("/" + std::to_string(devId) + " 1 driver enable"); } catch (...) {}
                        }
                    }
                    emit deviceConnectionChanged(devName, resStr, true);
                }
            } catch (...) {}
            break;
        }
    }
}

void HardwareManager::pollDevicePositions()
{
    if (m_pausePositionPolling) return;

    for (const auto& [devName, axes] : m_deviceAxes) {
        auto inst = getDevice(devName);
        auto motor = std::dynamic_pointer_cast<IMotorAxis>(inst);
        if (!inst || !inst->isOpen() || !motor) continue;

        bool isZaber = devName.toUpper().contains("ZABER");
        for (int axisIndex : axes) {
            try {
                int32_t pos = motor->getPosition(axisIndex);
                emit axisPositionUpdated(devName, axisIndex, QString::number(pos));
            } catch (...) {
                emit axisPositionUpdated(devName, axisIndex, isZaber ? "Error" : "Read Error");
                if (isZaber) {
                    inst->close();
                    break;
                }
            }
        }
    }
}
