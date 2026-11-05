#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "zaber.hpp"
#include "velmex.hpp"
#include "ldc.hpp"
#include "pm100D.hpp"
#include <instrument.hpp>

#include <QMessageBox>
#include <QDebug>
#include <QTime>
#include <QTreeWidgetItem>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QBrush>
#include <QColor>
#include <QTextCursor>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isStreaming(false)
{
    ui->setupUi(this);

    // --- Setup Dynamic Controller Command Labels ---
    setupControllerLabels();
    updateControllerOverlayLabels("");

    // --- Enforce Initial Splitter Geometry from .ui File ---
    ui->verticalMainSplitter->setSizes({550, 230});

    // --- UI Setup for Video Feed & Stream Button ---
    if (ui->labelVideoPlaceholder) {
        delete ui->labelVideoPlaceholder;
    }

    m_videoContainer = new QWidget(this);
    m_videoContainer->setStyleSheet("background-color: black;");
    m_videoContainer->setMinimumSize(320, 240);
    ui->verticalLayout->addWidget(m_videoContainer, 1);

    m_btnToggleStream = new QPushButton("Start Stream", this);
    m_btnToggleStream->setMinimumHeight(40);
    m_btnToggleStream->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; font-weight: bold; border-radius: 4px; }");
    ui->verticalLayout->addWidget(m_btnToggleStream);

    connect(m_btnToggleStream, &QPushButton::clicked, this, &MainWindow::toggleStreaming);

    // --- Connect Remote Target Combo Box & Home Button ---
    connect(ui->comboRemoteTarget, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onControllerTargetChanged);

    ui->motorHomeBtn->setVisible(false);
    connect(ui->motorHomeBtn, &QPushButton::clicked,
            this, &MainWindow::onHomeTargetClicked);

    // --- Context Menu for Clearing Text Controller Stream ---
    ui->textControllerStream->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->textControllerStream, &QTextEdit::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu *menu = ui->textControllerStream->createStandardContextMenu();
        menu->addSeparator();
        QAction *clearAction = menu->addAction("Clear Log Window");
        connect(clearAction, &QAction::triggered, ui->textControllerStream, &QTextEdit::clear);
        menu->exec(ui->textControllerStream->mapToGlobal(pos));
        delete menu;
    });

    // --- Connect Execution Button ---
    connect(ui->btnExecuteSequence, &QPushButton::clicked, this, &MainWindow::onExecuteSequenceClicked);

    // --- Initialize Camera Safely After Window Creation ---
    QTimer::singleShot(100, this, [this]() {
        initCamera();
    });

    // --- Timers ---
    m_cameraCheckTimer = new QTimer(this);
    connect(m_cameraCheckTimer, &QTimer::timeout, this, &MainWindow::checkCameraConnection);
    m_cameraCheckTimer->start(2000); // Slowed down from 1s to 2s to reduce overhead

    m_deviceCheckTimer = new QTimer(this);
    connect(m_deviceCheckTimer, &QTimer::timeout, this, &MainWindow::checkDeviceConnections);
    m_deviceCheckTimer->start(3000); // Slowed down slightly to prevent bus congestion

    m_positionUpdateTimer = new QTimer(this);
    connect(m_positionUpdateTimer, &QTimer::timeout, this, &MainWindow::updateDeviceTreePositions);
    m_positionUpdateTimer->start(400); // Slight bump to 400ms to free up UI thread for video frames

    // --- Initialize Controller ---
    if (m_remoteController.initialize()) {
        qDebug() << "Connected to controller:" << QString::fromStdString(m_remoteController.getControllerName());
    }

    m_inputThrottleTimer.start();
    m_reconnectElapsedTimer.start();

    m_controllerTimer = new QTimer(this);
    connect(m_controllerTimer, &QTimer::timeout, this, &MainWindow::pollControllerInput);
    m_controllerTimer->start(16);


    // --- Setup Station Selection Dropdown ---

    connect(ui->comboStation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onStationSelected);

    ui->comboStation->blockSignals(true);
    ui->comboStation->clear();

    // Add the default item
    ui->comboStation->addItem("-- Select Station --", "");

    // 1. Define the correct local path
    QString configDirectoryPath = "C:/Users/aron.rezene/Documents/Work/Weld_Room_Projects/SFW/FORCE/VIRTINSTGUI"; // Make sure your path goes here
    QDir configDir(configDirectoryPath);
    QStringList filters;
    filters << "*.ini";

    // 2. Grab the files
    QFileInfoList iniFiles = configDir.entryInfoList(filters, QDir::Files);

    // 3. The crucial loop: Extract the names and inject them into the dropdown
    for (const QFileInfo& fileInfo : iniFiles) {
        QString filePath = fileInfo.absoluteFilePath();
        QSettings settings(filePath, QSettings::IniFormat);

        // Grab the name from the INI file, or fallback to the raw filename
        QString stationName = settings.value("General/StationName", fileInfo.completeBaseName()).toString();

        // Push it into the GUI combobox
        ui->comboStation->addItem(stationName, filePath);

        // Log it to the console to prove the loop ran
        qDebug() << "Added to dropdown:" << stationName;
    }

    ui->comboStation->setCurrentIndex(0);
    ui->comboStation->blockSignals(false);



}

MainWindow::~MainWindow()
{
    if (m_controllerTimer) m_controllerTimer->stop();
    if (m_cameraCheckTimer) m_cameraCheckTimer->stop();
    if (m_deviceCheckTimer) m_deviceCheckTimer->stop();

    if (m_isStreaming) {
        try {
            m_grabber.streamStop();
        } catch (...) {}
    }

    // Safely close active VISA sessions
    for (auto& [name, inst] : m_stationDevices) {
        if (inst && inst->isOpen()) {
            inst->close();
        }
    }

    delete ui;
}

void MainWindow::onStationSelected(int index)
{
    QString iniPath = ui->comboStation->itemData(index).toString();

    if (!iniPath.isEmpty()) {
        loadStationConfig(iniPath);
    } else {
        ui->treeWidget->clear();
        ui->comboSequence->clear();
        m_stationDevices.clear();
        m_deviceResourceStrings.clear();
        m_axisTreeItems.clear();
        m_remoteController.clearTargetMotor();
        updateControllerTargetCombo();
    }
}

void MainWindow::loadStationConfig(const QString& iniFilePath)
{
    QSettings settings(iniFilePath, QSettings::IniFormat);

    ui->treeWidget->clear();
    ui->comboSequence->clear();
    m_stationDevices.clear();
    m_deviceResourceStrings.clear();
    m_axisTreeItems.clear();

    settings.beginGroup("Devices");
    QStringList deviceNames = settings.childKeys();

    for (const QString& devName : deviceNames) {
        QString resourceString = settings.value(devName).toString();
        std::shared_ptr<Instrument> instrument;

        QString devUpper = devName.toUpper();

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
                    instrument->write("F"); // Turns off the serial echo
                    Sleep(20);
                    instrument->write("C");   // Clears the buffer
                    Sleep(50);
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

        QTreeWidgetItem* item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, devName);

        if (isConnected) {
            item->setText(1, "● Connected");
            item->setForeground(1, QBrush(QColor("#2ecc71")));
        } else {
            item->setText(1, "● Disconnected");
            item->setForeground(1, QBrush(QColor("#e74c3c")));
        }

        QTreeWidgetItem* childItem = new QTreeWidgetItem(item);
        childItem->setText(0, "Resource");
        childItem->setText(1, resourceString);

        struct AxisDisplay {
            int hwIndex;
            QString label;
        };

        if (devUpper.contains("ZABER")) {
            const AxisDisplay zaberAxes[] = {
                { 3, "X" },
                { 4, "Y" },
                { 2, "Z" },
                { 1, "Gripper" }
            };

            for (const auto& ax : zaberAxes) {
                QTreeWidgetItem* axisItem = new QTreeWidgetItem(item);
                axisItem->setText(0, ax.label);
                axisItem->setText(1, "0 steps");
                m_axisTreeItems[devName][ax.hwIndex] = axisItem;
            }
        }
        else if (devUpper.contains("VELMEX") || devUpper.contains("VXM")) {
            const AxisDisplay velmexAxes[] = {
                { 1, "X" },
                { 2, "Y" },
                { 3, "Z" }
            };

            for (const auto& ax : velmexAxes) {
                QTreeWidgetItem* axisItem = new QTreeWidgetItem(item);
                axisItem->setText(0, ax.label);
                axisItem->setText(1, "0 steps");
                m_axisTreeItems[devName][ax.hwIndex] = axisItem;
            }
        }
    }
    settings.endGroup();

    settings.beginGroup("Products");
    QStringList generalKeys = settings.childKeys();
    for (const QString& key : generalKeys) {
        ui->comboSequence->addItem(settings.value(key).toString(), key);
    }
    settings.endGroup();

    ui->treeWidget->expandAll();
    updateControllerTargetCombo();
}



void MainWindow::updateControllerTargetCombo()
{
    ui->comboRemoteTarget->blockSignals(true);
    ui->comboRemoteTarget->clear();
    ui->comboRemoteTarget->addItem("-- Select Target Device --", "");

    for (const auto& [devName, instrument] : m_stationDevices) {
        QString devUpper = devName.toUpper();
        if ((devUpper.contains("ZABER") || devUpper.contains("VELMEX") || devUpper.contains("VXM"))
            && instrument && instrument->isOpen())
        {
            ui->comboRemoteTarget->addItem(devName, devName);
        }
    }

    ui->comboRemoteTarget->blockSignals(false);
    ui->comboRemoteTarget->setCurrentIndex(0);
    m_remoteController.clearTargetMotor();
    ui->motorHomeBtn->setVisible(false);
}

void MainWindow::onControllerTargetChanged(int index)
{


    ui->textControllerStream->clear();
    ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());

    QString devName = ui->comboRemoteTarget->itemData(index).toString();

    // Update the controller graphic text overlays dynamically
    updateControllerOverlayLabels(devName);

    if (devName.isEmpty() || !m_stationDevices.count(devName) || !m_stationDevices[devName]) {
        m_remoteController.clearTargetMotor();
        ui->motorHomeBtn->setVisible(false);
        return;
    }

    auto instrument = m_stationDevices[devName];

    if (!instrument->isOpen()) {
        ui->textControllerStream->append(
            QString("[%1] <span style='color: #e74c3c;'>Cannot bind target %2: Serial session closed.</span>")
                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName)
            );
        ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
        m_remoteController.clearTargetMotor();
        ui->motorHomeBtn->setVisible(false);
        return;
    }

    bool isMotorTarget = false;

    if (auto zaberInst = std::dynamic_pointer_cast<Zaber>(instrument)) {
        m_remoteController.bindZaber(zaberInst);
        isMotorTarget = true;
    } else if (auto velmexInst = std::dynamic_pointer_cast<VelmexVXM>(instrument)) {
        m_remoteController.bindVelmex(velmexInst);
        isMotorTarget = true;
    } else {
        m_remoteController.clearTargetMotor();
    }

    ui->motorHomeBtn->setVisible(isMotorTarget);

    QString instructionsHtml = QString(
                                   "[%1] <span style='color: #3498db;'><b>Target Bound: %2</b></span><br>"
                                   "<b>Instructions / Controls:</b><br>"
                                   "• Use mapped controller thumbsticks or triggers to adjust position.<br>"
                                   "• Click the <b>Home</b> button above to zero out target axes.<br>"
                                   "• Monitor real-time step increments in the <b>Devices</b> tab."
                                   ).arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName);

    ui->textControllerStream->append(instructionsHtml);
    ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
}

void MainWindow::onHomeTargetClicked()
{
    QString devName = ui->comboRemoteTarget->currentData().toString();

    if (devName.isEmpty() || !m_stationDevices.count(devName) || !m_stationDevices[devName]) {
        ui->textControllerStream->append(
            QString("[%1] <span style='color: #e74c3c;'><b>Homing Failed:</b> No motor target selected.</span>")
                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
            );
        ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
        return;
    }

    auto instrument = m_stationDevices[devName];

    if (!instrument->isOpen()) {
        ui->textControllerStream->append(
            QString("[%1] <span style='color: #e74c3c;'><b>Homing Failed:</b> Session for %2 is closed.</span>")
                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName)
            );
        ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
        return;
    }

    ui->textControllerStream->append(
        QString("[%1] Initiating home command for %2...")
            .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName)
        );

    try {
        if (auto zaber = std::dynamic_pointer_cast<Zaber>(instrument)) {
            for (int axis = 1; axis <= 4; ++axis) {
                zaber->home(axis);
            }
        } else if (auto velmex = std::dynamic_pointer_cast<VelmexVXM>(instrument)) {
            velmex->write("G,C,S1M800,S2M800,I1M-0,IA1M-0,I2M-0,IA2M-0,R");
        }

        ui->textControllerStream->append(
            QString("[%1] <span style='color: #2ecc71;'><b>Home Command Sent:</b> %2 is homing.</span>")
                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName)
            );
    }
    catch (const std::exception& ex) {
        ui->textControllerStream->append(
            QString("[%1] <span style='color: #e74c3c;'><b>Homing Error (%2):</b> %3</span>")
                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName, QString::fromUtf8(ex.what()))
            );
    }

    ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
    QTextCursor cursor = ui->textControllerStream->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textControllerStream->setTextCursor(cursor);
}



// 2. Replace updateDeviceTreePositions() in mainwindow.cpp with this:
void MainWindow::updateDeviceTreePositions()
{
    // Do not clog the serial port with position queries while the controller is actively sending motion commands
    if (!m_lastLoggedInputs.isEmpty()) {
        return;
    }

    for (auto& [devName, axisMap] : m_axisTreeItems) {
        if (!m_stationDevices.count(devName) || !m_stationDevices[devName]) continue;

        auto inst = m_stationDevices[devName];
        if (!inst->isOpen()) continue;

        if (auto zaber = std::dynamic_pointer_cast<Zaber>(inst)) {
            for (auto& [axisIndex, treeItem] : axisMap) {
                try {
                    int32_t pos = zaber->getPosition(axisIndex);
                    treeItem->setText(1, QString("%1 steps").arg(pos));
                } catch (...) {
                    treeItem->setText(1, "Error");
                    inst->close(); // Stop querying remaining axes if device unplugged/timed out
                    break;
                }
            }
        }

        else if (auto velmex = std::dynamic_pointer_cast<VelmexVXM>(inst)) {
            try {

                for (auto& [axisIndex, treeItem] : axisMap) {
                    int32_t pos = velmex->getPosition(axisIndex);
                    treeItem->setText(1, QString("%1 steps").arg(pos));
                }
            } catch (...) {
                // Remove inst->close(); so the device stays connected
                for (auto& [axisIndex, treeItem] : axisMap) {
                    treeItem->setText(1, "Read Error");
                }
            }
        }
    }
}


void MainWindow::pollControllerInput()
{
    try {
        m_remoteController.pollEvents();

        if (!m_remoteController.isConnected()) {
            ui->lblRemoteStatus->setText("<span style='color: #e74c3c; font-size: 16pt;'>&#9679;</span>");
            ui->comboRemoteTarget->setEnabled(false);

            if (m_reconnectElapsedTimer.hasExpired(1000)) {
                if (m_remoteController.initialize()) {
                    QString logMsg = QString("[%1] Controller connected: %2")
                    .arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
                        .arg(QString::fromStdString(m_remoteController.getControllerName()));
                    ui->textControllerStream->append(logMsg);
                    ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
                }
                m_reconnectElapsedTimer.restart();
            }
            return;
        }

        ui->lblRemoteStatus->setText("<span style='color: #2ecc71; font-size: 16pt;'>&#9679;</span>");
        ui->comboRemoteTarget->setEnabled(true);


        // FIX: Removed the 50 ms m_inputThrottleTimer block.
        // Commands now fire directly on the 16 ms timer tick.// FIX: Removed the 50 ms m_inputThrottleTimer block.
        // Commands now fire directly on the 16 ms timer tick.
        QString target = ui->comboRemoteTarget->currentText().toUpper();
        int stepDistance = target.contains("ZABER") ? 10000 : 100;

        // 1. Create a static timer to track time specifically for motion commands
        static QElapsedTimer motionThrottleTimer;
        if (!motionThrottleTimer.isValid()) {
            motionThrottleTimer.start();
        }

        // 2. Define how long the system should wait before sending the next command.
        // Adjust the 300ms depending on how long your Z and Gripper actually take to move 10,000 steps.
        int requiredDelay =  256 ;

        // 3. Only send the motion command if the required time has passed
        if (motionThrottleTimer.hasExpired(requiredDelay)) {
            m_remoteController.processMotionCommands(stepDistance, 1000);
            motionThrottleTimer.restart(); // Reset the timer after sending
        }
        struct ButtonNamePair {
            SDL_GamepadButton button;
            QString name;
        };

        ButtonNamePair buttonsToTrack[] = {
            { SDL_GAMEPAD_BUTTON_SOUTH, "A" },
            { SDL_GAMEPAD_BUTTON_EAST, "B" },
            { SDL_GAMEPAD_BUTTON_WEST, "X" },
            { SDL_GAMEPAD_BUTTON_NORTH, "Y" },
            { SDL_GAMEPAD_BUTTON_BACK, "View" },
            { SDL_GAMEPAD_BUTTON_GUIDE, "Xbox" },
            { SDL_GAMEPAD_BUTTON_START, "Menu" },
            { SDL_GAMEPAD_BUTTON_LEFT_STICK, "L-Stick" },
            { SDL_GAMEPAD_BUTTON_RIGHT_STICK, "R-Stick" },
            { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, "LB" },
            { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, "RB" },
            { SDL_GAMEPAD_BUTTON_DPAD_UP, "DPad-Up" },
            { SDL_GAMEPAD_BUTTON_DPAD_DOWN, "DPad-Down" },
            { SDL_GAMEPAD_BUTTON_DPAD_LEFT, "DPad-Left" },
            { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, "DPad-Right" }
        };

        QStringList activeInputs;
        for (const auto& pair : buttonsToTrack) {
            if (m_remoteController.isButtonPressed(pair.button)) {
                activeInputs.append(pair.name);
            }
        }

        Sint16 leftTrigger = m_remoteController.getAxisValue(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        Sint16 rightTrigger = m_remoteController.getAxisValue(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

        if (leftTrigger > 8000)  activeInputs.append(QString("LT(%1)").arg(leftTrigger));
        if (rightTrigger > 8000) activeInputs.append(QString("RT(%1)").arg(rightTrigger));

        QString currentInputStr = activeInputs.join(", ");

        if (!activeInputs.isEmpty()) {
            bool timeElapsed = m_inputThrottleTimer.hasExpired(250);
            bool inputChanged = (currentInputStr != m_lastLoggedInputs);

            if (timeElapsed || inputChanged) {
                QString timestampedStream = QString("[%1] Active Inputs: %2")
                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
                    .arg(currentInputStr);

                ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
                ui->textControllerStream->append(timestampedStream);

                QTextCursor cursor = ui->textControllerStream->textCursor();
                cursor.movePosition(QTextCursor::End);
                ui->textControllerStream->setTextCursor(cursor);

                m_lastLoggedInputs = currentInputStr;
            }
        } else {
            m_lastLoggedInputs.clear();
        }
    }
    catch (const std::exception& ex) {
        QString errorMsg = QString("[%1] <span style='color: #e74c3c;'><b>VISA/Motion Error:</b> %2</span>")
        .arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
            .arg(QString::fromUtf8(ex.what()));

        ui->textControllerStream->append(errorMsg);
        ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());

        QTextCursor cursor = ui->textControllerStream->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->textControllerStream->setTextCursor(cursor);
    }
    catch (...) {
        QString errorMsg = QString("[%1] <span style='color: #e74c3c;'><b>Hardware Error:</b> Unknown runtime exception occurred.</span>")
        .arg(QTime::currentTime().toString("hh:mm:ss.zzz"));

        ui->textControllerStream->append(errorMsg);
        ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());

        QTextCursor cursor = ui->textControllerStream->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->textControllerStream->setTextCursor(cursor);
    }
}

void MainWindow::initCamera()
{
    try {
        auto devList = ic4::DeviceEnum::enumDevices();
        if (devList.empty()) return;

        m_grabber.deviceOpen(devList.front());

#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_videoContainer->winId());
        ic4::Error err;

        m_ic4Display = ic4::Display::create(
            static_cast<ic4::DisplayType>(0),
            reinterpret_cast<ic4::WindowHandle>(hwnd),
            err
            );

        if (m_ic4Display) {
            m_grabber.streamSetup(m_ic4Display);
            m_isStreaming = true;
            m_btnToggleStream->setText("Stop Stream");
            m_btnToggleStream->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 4px; }");
        }
#endif
    } catch (...) {}
}

void MainWindow::toggleStreaming()
{
    if (!m_grabber.isDeviceOpen()) {
        try {
            initCamera();
        } catch (...) {}
        return;
    }

    if (m_isStreaming) {
        try {
            m_grabber.streamStop();
        } catch (...) {}
        m_isStreaming = false;
        m_btnToggleStream->setText("Start Stream");
        m_btnToggleStream->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; font-weight: bold; border-radius: 4px; }");
    } else {
        if (m_ic4Display) {
            try {
                m_grabber.streamSetup(m_ic4Display);
                m_isStreaming = true;
                m_btnToggleStream->setText("Stop Stream");
                m_btnToggleStream->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 4px; }");
            } catch (...) {}
        }
    }
}



void MainWindow::checkCameraConnection()
{
    // If the device is already open and streaming, do nothing
    if (m_grabber.isDeviceOpen() && m_isStreaming) {
        return;
    }

    bool currentlyOpen = m_grabber.isDeviceOpen();

    if (!currentlyOpen) {
        auto devList = ic4::DeviceEnum::enumDevices();
        if (!devList.empty()) {
            try {
                m_grabber.deviceOpen(devList.front());
#ifdef Q_OS_WIN
                HWND hwnd = reinterpret_cast<HWND>(m_videoContainer->winId());
                ic4::Error err;

                m_ic4Display = ic4::Display::create(
                    static_cast<ic4::DisplayType>(0),
                    reinterpret_cast<ic4::WindowHandle>(hwnd),
                    err
                    );

                if (m_ic4Display) {
                    m_grabber.streamSetup(m_ic4Display);
                    m_isStreaming = true;
                    m_btnToggleStream->setText("Stop Stream");
                    m_btnToggleStream->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 4px; }");
                }
#endif
            } catch (...) {}
        }
    }
}




// 3. Replace checkDeviceConnections() in mainwindow.cpp with this non-stalling version:
void MainWindow::checkDeviceConnections()
{
    int count = ui->treeWidget->topLevelItemCount();
    if (count == 0) return;

    // First, quickly update the UI status for all items based on current state
    for (int i = 0; i < count; ++i) {
        QTreeWidgetItem* topItem = ui->treeWidget->topLevelItem(i);
        QString devName = topItem->text(0);

        if (m_stationDevices.count(devName)) {
            auto dev = m_stationDevices[devName];
            bool currentlyConnected = dev && dev->isOpen();

            if (currentlyConnected) {
                // Lightweight check on open session (takes microseconds, does not open new handles)
                ViSession testSession = VI_NULL;
                std::string resStd = m_deviceResourceStrings[devName].toStdString();
                ViStatus status = viOpen(m_vrm.handle(), const_cast<char*>(resStd.c_str()), VI_NULL, 50, &testSession);
                if (status >= VI_SUCCESS) {
                    viClose(testSession);
                } else if (status != VI_ERROR_RSRC_LOCKED) {
                    dev->close();
                    currentlyConnected = false;
                }
            }

            if (currentlyConnected) {
                topItem->setText(1, "● Connected");
                topItem->setForeground(1, QBrush(QColor("#2ecc71")));
            } else {
                topItem->setText(1, "● Disconnected");
                topItem->setForeground(1, QBrush(QColor("#e74c3c")));
            }
        }
    }

    // Only attempt to reconnect ONE disconnected device per timer tick so multiple unplugged
    // devices don't stack their VISA open timeouts and freeze the GUI thread.
    static int reconnectIndex = 0;
    for (int Checked = 0; Checked < count; ++Checked) {
        int idx = (reconnectIndex + Checked) % count;
        QTreeWidgetItem* topItem = ui->treeWidget->topLevelItem(idx);
        QString devName = topItem->text(0);

        if (m_stationDevices.count(devName)) {
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
                        topItem->setText(1, "● Connected");
                        topItem->setForeground(1, QBrush(QColor("#2ecc71")));
                    }
                } catch (...) {}
                break; // Only try one reconnect per 2-second cycle
            }
        }
    }
}


void MainWindow::onExecuteSequenceClicked()
{
    QString seqKey = ui->comboSequence->currentData().toString();
    QString seqText = ui->comboSequence->currentText();

    if (seqKey.isEmpty()) {
        QMessageBox::warning(this, "No Sequence Selected", "Please select a product sequence before executing.");
        return;
    }

    emit executeSequenceRequested(seqKey, seqText);
}


void MainWindow::setupControllerLabels()
{
    if (!ui->labelControllerImage) return;

    ui->labelControllerImage->installEventFilter(this);
    m_controllerLabels.clear();

    const double designCanvasWidth =400.0;
    const double designCanvasHeight = 400.0;

    QList<QLabel*> childLabels = ui->labelControllerImage->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);

    for (QLabel* lbl : childLabels) {
        double normX = lbl->x() / designCanvasWidth;
        double normY = lbl->y() / designCanvasHeight;
        double normW = lbl->width() / designCanvasWidth;
        double normH = lbl->height() / designCanvasHeight;

        m_controllerLabels.append({lbl, normX, normY, normW, normH});
    }

    updateControllerLabelPositions();
}


void MainWindow::updateControllerLabelPositions()
{
    if (!ui->labelControllerImage) return;

    int parentW = ui->labelControllerImage->width();
    int parentH = ui->labelControllerImage->height();

    for (const auto& item : m_controllerLabels) {
        int x = static_cast<int>(item.normX * parentW);
        int y = static_cast<int>(item.normY * parentH);
        int w = static_cast<int>(item.normW * parentW);
        int h = static_cast<int>(item.normH * parentH);
        item.label->setGeometry(x, y, w, h);
    }
}



void MainWindow::updateControllerOverlayLabels(const QString& devName)
{
    QString devUpper = devName.toUpper();

    if (devName.isEmpty()) {


        // Fallback for any other instrument type
        ui->lbl_slow->setText("");
        ui->lbl_bump_L->setText("");
        ui->lbl_L_analog->setText("");
        ui->lbl_dpad->setText("");
        ui->lbl_view->setText("");
        ui->lbl_menu->setText("");
        ui->lbl_fast->setText("");
        ui->lbl_bump_R->setText("");
        ui->lbl_Y->setText("");
        ui->lbl_X->setText("");
        ui->lbl_B->setText("");
        ui->lbl_A->setText("");
        ui->lbl_R_analog->setText("");


        // Default / Unselected State

    }
    else if (devUpper.contains("ZABER")) {
        // Zaber Controller Mappings (10,000 step increments)
        ui->lbl_slow->setText("");
        ui->lbl_bump_L->setText("Z - Step");
        ui->lbl_L_analog->setText("XY Jog (Toggle Speed)");
        ui->lbl_dpad->setText("XY Step");
        ui->lbl_view->setText("Target Select");
        ui->lbl_menu->setText("");
        ui->lbl_fast->setText("");
        ui->lbl_bump_R->setText("Z + Step");
        ui->lbl_Y->setText("");
        ui->lbl_X->setText("");
        ui->lbl_B->setText("Gripper Close");
        ui->lbl_A->setText("Grippr Open");
        ui->lbl_R_analog->setText("Z Jog (Toggle Speed)");
    }
    else if (devUpper.contains("VELMEX") || devUpper.contains("VXM")) {
        // Velmex VXM Controller Mappings (100 step increments)
        ui->lbl_slow->setText("");
        ui->lbl_bump_L->setText("");
        ui->lbl_L_analog->setText("XY Jog (Toggle Speed)");
        ui->lbl_dpad->setText("XY Step");
        ui->lbl_view->setText("Target Select");
        ui->lbl_menu->setText("");
        ui->lbl_fast->setText("");
        ui->lbl_bump_R->setText("");
        ui->lbl_Y->setText("");
        ui->lbl_X->setText("");
        ui->lbl_B->setText("Z - Step");
        ui->lbl_A->setText("Z + Step");
        ui->lbl_R_analog->setText("Z Jog (Toggle Speed)");
    }

}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->labelControllerImage && event->type() == QEvent::Resize) {
        updateControllerLabelPositions();
    }
    return QMainWindow::eventFilter(watched, event);
}

