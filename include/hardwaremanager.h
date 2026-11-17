#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QStringList>

#include <map>
#include <memory>
#include <vector>
#include <utility>

#include <ic4/ic4.h>
#include <vrm.hpp>
#include <instrument.hpp>
#include <imotoraxis.hpp>
#include "configloader.h"

class HardwareManager : public QObject {
    Q_OBJECT

public:
    explicit HardwareManager(QObject *parent = nullptr);
    ~HardwareManager();

    void clearDevices();
    bool connectAndSetupDevice(const DeviceConfigInfo& devConfig, std::map<int, std::pair<int, int>>& ioDefaultLimits);

    std::shared_ptr<Instrument> getDevice(const QString& devName) const;
    QStringList getConnectedMotors() const;

    void setAxisLimits(const QString& devName, int axisIndex, int minPos, int maxPos, int maxSpeed = 0);
    void moveAxisAbsolute(const QString& devName, int axisIndex, int targetPos);
    void homeDevice(const QString& devName);

    void initCamera(quintptr videoContainerWinId);
    void toggleStreaming(quintptr videoContainerWinId);
    void updateCameraDisplayRect(quintptr videoContainerWinId, int width, int height);
    void setPositionPollingPaused(bool paused);

signals:
    void cameraStreamStateChanged(bool isStreaming);
    void deviceConnectionChanged(const QString& devName, const QString& resource, bool connected);
    void axisPositionUpdated(const QString& devName, int axisIndex, const QString& posText);
    void logMessage(const QString& htmlMsg);

private slots:
    void checkCameraConnection();
    void checkDeviceConnections();
    void pollDevicePositions();

private:
    VisaResourceManager m_vrm;
    std::map<QString, std::shared_ptr<Instrument>> m_stationDevices;
    std::map<QString, QString> m_deviceResourceStrings;
    std::map<QString, std::vector<int>> m_deviceAxes;

    ic4::Grabber m_grabber;
    std::shared_ptr<ic4::Display> m_ic4Display;
    bool m_isStreaming = false;
    quintptr m_videoWinId = 0;

    QTimer *m_cameraCheckTimer;
    QTimer *m_deviceCheckTimer;
    QTimer *m_positionUpdateTimer;
    bool m_pausePositionPolling = false;
};
