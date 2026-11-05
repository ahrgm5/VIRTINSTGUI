#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTimer>
#include <QTreeWidgetItem>
#include <QElapsedTimer>
#include <QMenu>
#include <QList>
#include <memory>
#include <map>
#include <ic4/ic4.h>
#include <ic4-interop/interop-Qt.h>
#include <remoteControl.hpp>
#include <vrm.hpp>
#include <instrument.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void executeSequenceRequested(const QString& sequenceKey, const QString& sequenceDisplayName);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onStationSelected(int index);
    void toggleStreaming();
    void pollControllerInput();
    void checkCameraConnection();
    void checkDeviceConnections();
    void loadStationConfig(const QString& iniFilePath);
    void onExecuteSequenceClicked();
    void onControllerTargetChanged(int index);
    void onHomeTargetClicked();

private:
    void initCamera();
    void updateControllerTargetCombo();
    void updateDeviceTreePositions();
    void setupControllerLabels();
    void updateControllerLabelPositions();
    void updateControllerOverlayLabels(const QString& devName); // Add this line

    Ui::MainWindow *ui;
    QWidget *m_videoContainer;
    QPushButton *m_btnToggleStream;

    // Controller Label Overlay Structure
    struct ControllerLabelInfo {
        QLabel *label;
        double normX;
        double normY;
        double normW;
        double normH;
    };
    QList<ControllerLabelInfo> m_controllerLabels;

    // IC4 Camera & Display
    ic4::Grabber m_grabber;
    std::shared_ptr<ic4::Display> m_ic4Display;
    bool m_isStreaming;

    // Camera Hot-Plug Timer
    QTimer *m_cameraCheckTimer;

    // Device Health & Hot-Plug Reconnect Timer
    QTimer *m_deviceCheckTimer;
    QTimer *m_positionUpdateTimer;

    // Xbox Controller & Throttling
    RemoteControl m_remoteController;
    QTimer *m_controllerTimer;
    QElapsedTimer m_inputThrottleTimer;
    QElapsedTimer m_reconnectElapsedTimer;
    QString m_lastLoggedInputs;

    // Hardware & Station Configuration
    VisaResourceManager m_vrm;
    std::map<QString, std::shared_ptr<Instrument>> m_stationDevices;
    std::map<QString, QString> m_deviceResourceStrings;
    std::map<QString, std::map<int, QTreeWidgetItem*>> m_axisTreeItems;
};