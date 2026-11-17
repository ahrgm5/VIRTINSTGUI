#include "remoteinputmanager.h"
#include <QTime>
#include <QStringList>
#include <cmath>
#include <algorithm>

namespace {
constexpr Sint16 kStickDeadzone = 6500;
}

RemoteInputManager::RemoteInputManager(
    std::function<void(int&, int&)> motionParamsCallback,
    std::function<bool()> comboPopupHandlerCallback,
    QObject *parent)
    : QObject(parent)
    , m_motionParamsCallback(std::move(motionParamsCallback))
    , m_comboPopupHandlerCallback(std::move(comboPopupHandlerCallback))
{
    m_inputThrottleTimer.start();
    m_reconnectElapsedTimer.start();
    m_guiThrottleTimer.start();
    m_motionThrottleTimer.start();
    m_jogUpdateThrottleTimer.start();

    m_controllerTimer = new QTimer(this);
    connect(m_controllerTimer, &QTimer::timeout, this, &RemoteInputManager::pollControllerInput);
    m_controllerTimer->start(16);
}

RemoteInputManager::~RemoteInputManager()
{
    if (m_controllerTimer) {
        m_controllerTimer->stop();
    }
    if (m_controller) {
        SDL_CloseGamepad(m_controller);
        m_controller = nullptr;
    }
    if (m_isInitialized) {
        SDL_Quit();
        m_isInitialized = false;
    }
}

bool RemoteInputManager::openAvailableGamepad()
{
    if (!m_isInitialized) {
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            return false;
        }
        m_isInitialized = true;
    }

    int numJoysticks = 0;
    SDL_JoystickID *joysticks = SDL_GetJoysticks(&numJoysticks);
    if (joysticks) {
        for (int i = 0; i < numJoysticks; ++i) {
            SDL_JoystickID jid = joysticks[i];
            if (SDL_IsGamepad(jid)) {
                m_controller = SDL_OpenGamepad(jid);
                if (m_controller) {
                    m_connected = true;
                    SDL_free(joysticks);
                    return true;
                }
            }
        }
        SDL_free(joysticks);
    }
    return false;
}

bool RemoteInputManager::initializeController()
{
    if (openAvailableGamepad()) {
        m_wasConnected = true;
        emit connectionStateChanged(true, QString::fromStdString(getControllerName()));
        return true;
    }
    return false;
}

void RemoteInputManager::pollSdlEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            m_connected = false;
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (m_controller) {
                SDL_CloseGamepad(m_controller);
                m_controller = nullptr;
            }
            m_connected = false;
            break;
        default:
            break;
        }
    }
}

bool RemoteInputManager::isConnected() const
{
    return m_connected;
}

std::string RemoteInputManager::getControllerName() const
{
    if (m_controller) {
        const char *name = SDL_GetGamepadName(m_controller);
        return name ? name : "Unknown";
    }
    return "None";
}

bool RemoteInputManager::isButtonPressed(SDL_GamepadButton button) const
{
    if (m_controller && m_connected) {
        return SDL_GetGamepadButton(m_controller, button);
    }
    return false;
}

Sint16 RemoteInputManager::getAxisValue(SDL_GamepadAxis axis) const
{
    if (m_controller && m_connected) {
        return SDL_GetGamepadAxis(m_controller, axis);
    }
    return 0;
}

void RemoteInputManager::setTargetMotor(std::shared_ptr<IMotorAxis> motor, ControllerProfile profile)
{
    stopActiveJog();
    m_activeMotor = std::move(motor);
    m_activeProfile = m_activeMotor ? profile : ControllerProfile::GuiWide;
}

void RemoteInputManager::bindTargetDevice(const QString& devName, std::shared_ptr<Instrument> inst)
{
    auto motor = std::dynamic_pointer_cast<IMotorAxis>(inst);
    if (!inst || !inst->isOpen() || !motor) {
        clearTargetMotor();
        return;
    }

    QString devUpper = devName.toUpper();
    if (devUpper.contains("ZABER")) {
        setTargetMotor(motor, ControllerProfile::Zaber4Axis);
    } else if (devUpper.contains("VELMEX") || devUpper.contains("VXM")) {
        setTargetMotor(motor, ControllerProfile::LaserWeld3Axis);
    } else {
        clearTargetMotor();
    }
}

void RemoteInputManager::clearTargetMotor()
{
    stopActiveJog();
    m_activeMotor.reset();
    m_activeProfile = ControllerProfile::GuiWide;
}

void RemoteInputManager::moveRelative(int axis, int distance)
{
    if (m_activeMotor) m_activeMotor->moveRelative(axis, distance);
}

void RemoteInputManager::moveAbsolute(int axis, int position)
{
    if (m_activeMotor) m_activeMotor->moveAbsolute(axis, position);
}

void RemoteInputManager::setSpeed(int axis, int speed)
{
    if (m_activeMotor) m_activeMotor->setSpeed(axis, speed);
}

void RemoteInputManager::stop(int axis)
{
    if (m_activeMotor) m_activeMotor->stop(axis);
}

bool RemoteInputManager::isInputActive() const
{
    return !m_lastLoggedInputs.isEmpty() || m_xyJogActive || m_zJogActive;
}

int RemoteInputManager::computeQuantizedSpeed(Sint16 rawVal, int deadzone, int minSpeed, int maxSpeed)
{
    int mag = std::abs(static_cast<int>(rawVal));
    if (mag <= deadzone) return 0;

    double norm = static_cast<double>(mag - deadzone) / static_cast<double>(32767 - deadzone);
    norm = std::clamp(norm, 0.0, 1.0);

    // Quantize into 4 discrete speed tiers so minor thumb jitter does not flood the serial bus
    int bucket = std::clamp(static_cast<int>(std::ceil(norm * 4.0)), 1, 4);
    double bucketNorm = bucket / 4.0;

    int spd = minSpeed + static_cast<int>(bucketNorm * (maxSpeed - minSpeed));
    return std::max(minSpeed, spd);
}

void RemoteInputManager::stopActiveJog()
{
    if (!m_activeMotor) {
        m_xyJogActive = false;
        m_zJogActive = false;
        return;
    }

    if (m_xyJogActive) {
        if (m_activeProfile == ControllerProfile::Zaber4Axis) {
            m_activeMotor->stop(3);
            m_activeMotor->stop(4);
        } else if (m_activeProfile == ControllerProfile::LaserWeld3Axis) {
            m_activeMotor->stop(0);
        }
        m_xyJogActive = false;
        m_lastJogDirX = 0;
        m_lastJogDirY = 0;
        m_lastJogSpeedX = 0;
        m_lastJogSpeedY = 0;
    }

    if (m_zJogActive) {
        if (m_activeProfile == ControllerProfile::Zaber4Axis) {
            m_activeMotor->stop(2);
        } else if (m_activeProfile == ControllerProfile::LaserWeld3Axis) {
            m_activeMotor->stop(2);
        }
        m_zJogActive = false;
        m_lastJogDirZ = 0;
        m_lastJogSpeedZ = 0;
    }
}

void RemoteInputManager::processThumbstickJog(int maxUiSpeed)
{
    if (!m_connected || !m_activeMotor || m_activeProfile == ControllerProfile::GuiWide) {
        return;
    }

    // 1. Rising-edge toggle for L-Stick (XY Fine/Coarse) and R-Stick (Z Fine/Coarse)
    bool leftStickBtn = isButtonPressed(SDL_GAMEPAD_BUTTON_LEFT_STICK);
    if (leftStickBtn && !m_prevLeftStickBtn) {
        m_xyFineMode = !m_xyFineMode;
        emit logMessage(QString("[%1] <b>XY Thumbstick Jog Mode:</b> %2")
                            .arg(QTime::currentTime().toString("hh:mm:ss.zzz"),
                                 m_xyFineMode ? "FINE (Precision)" : "COARSE (Normal)"));
    }
    m_prevLeftStickBtn = leftStickBtn;

    bool rightStickBtn = isButtonPressed(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    if (rightStickBtn && !m_prevRightStickBtn) {
        m_zFineMode = !m_zFineMode;
        emit logMessage(QString("[%1] <b>Z Thumbstick Jog Mode:</b> %2")
                            .arg(QTime::currentTime().toString("hh:mm:ss.zzz"),
                                 m_zFineMode ? "FINE (Precision)" : "COARSE (Normal)"));
    }
    m_prevRightStickBtn = rightStickBtn;

    // 2. Read Analog Stick Axes (SDL Up is negative, Down is positive)
    Sint16 lx = getAxisValue(SDL_GAMEPAD_AXIS_LEFTX);
    Sint16 ly = getAxisValue(SDL_GAMEPAD_AXIS_LEFTY);
    Sint16 ry = getAxisValue(SDL_GAMEPAD_AXIS_RIGHTY);

    int dirX = (lx > kStickDeadzone) ? 1 : ((lx < -kStickDeadzone) ? -1 : 0);
    int dirY = (ly < -kStickDeadzone) ? 1 : ((ly > kStickDeadzone) ? -1 : 0);
    int dirZ = (ry < -kStickDeadzone) ? 1 : ((ry > kStickDeadzone) ? -1 : 0);

    bool isZaber = (m_activeProfile == ControllerProfile::Zaber4Axis);
    int coarseMax = std::max(100, maxUiSpeed);
    int fineMax   = isZaber ? std::max(500, coarseMax / 8) : std::clamp(coarseMax / 5, 40, 250);
    int minSpd    = isZaber ? 100 : 15;

    int xyCap = m_xyFineMode ? fineMax : coarseMax;
    int zCap  = m_zFineMode  ? fineMax : coarseMax;

    int spdX = computeQuantizedSpeed(lx, kStickDeadzone, minSpd, xyCap);
    int spdY = computeQuantizedSpeed(ly, kStickDeadzone, minSpd, xyCap);
    int spdZ = computeQuantizedSpeed(ry, kStickDeadzone, minSpd, zCap);

    // 3. XY Dual-Axis State Machine
    int axisX = isZaber ? 3 : 1; // Velmex: Motor 1 (Master) = X; Zaber: Axis 3 = X
    int axisY = isZaber ? 4 : 3; // Velmex: Motor 3 (Slave)  = Y; Zaber: Axis 4 = Y
    int axisZ = 2;               // Both Velmex (Motor 2 Master) & Zaber (Axis 2) = Z

    if (dirX == 0 && dirY == 0) {
        if (m_xyJogActive) {
            if (isZaber) {
                if (m_lastJogDirX != 0) m_activeMotor->stop(axisX);
                if (m_lastJogDirY != 0) m_activeMotor->stop(axisY);
            } else {
                m_activeMotor->stop(0);
            }
            m_xyJogActive = false;
            m_lastJogDirX = 0;
            m_lastJogDirY = 0;
            m_lastJogSpeedX = 0;
            m_lastJogSpeedY = 0;
        }
    } else {
        bool dirChanged = (dirX != m_lastJogDirX) || (dirY != m_lastJogDirY);
        bool spdChanged = (spdX != m_lastJogSpeedX) || (spdY != m_lastJogSpeedY);

        if (!m_xyJogActive || dirChanged || (spdChanged && m_jogUpdateThrottleTimer.hasExpired(180))) {
            if (m_xyJogActive && !isZaber) {
                // On Velmex, decelerate current index before issuing updated coordinated slew
                m_activeMotor->stop(0);
            }
            m_activeMotor->jogDualAxis(axisX, dirX, spdX, axisY, dirY, spdY);
            m_xyJogActive = true;
            m_lastJogDirX = dirX;
            m_lastJogDirY = dirY;
            m_lastJogSpeedX = spdX;
            m_lastJogSpeedY = spdY;
            m_jogUpdateThrottleTimer.restart();
        }
    }

    // 4. Z Single-Axis State Machine (On Velmex, Motor 1 and Motor 2 share the Master VXM, so XY takes priority)
    if (!isZaber && m_xyJogActive) {
        return;
    }

    if (dirZ == 0) {
        if (m_zJogActive) {
            m_activeMotor->stop(axisZ);
            m_zJogActive = false;
            m_lastJogDirZ = 0;
            m_lastJogSpeedZ = 0;
        }
    } else {
        bool dirChanged = (dirZ != m_lastJogDirZ);
        bool spdChanged = (spdZ != m_lastJogSpeedZ);

        if (!m_zJogActive || dirChanged || (spdChanged && m_jogUpdateThrottleTimer.hasExpired(180))) {
            if (m_zJogActive && !isZaber) {
                m_activeMotor->stop(axisZ);
            }
            m_activeMotor->jogSingleAxis(axisZ, dirZ, spdZ);
            m_zJogActive = true;
            m_lastJogDirZ = dirZ;
            m_lastJogSpeedZ = spdZ;
            m_jogUpdateThrottleTimer.restart();
        }
    }
}

void RemoteInputManager::logActiveInputs()
{
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
        if (isButtonPressed(pair.button)) activeInputs.append(pair.name);
    }

    Sint16 lx = getAxisValue(SDL_GAMEPAD_AXIS_LEFTX);
    Sint16 ly = getAxisValue(SDL_GAMEPAD_AXIS_LEFTY);
    Sint16 ry = getAxisValue(SDL_GAMEPAD_AXIS_RIGHTY);
    if (std::abs(lx) > kStickDeadzone || std::abs(ly) > kStickDeadzone) {
        activeInputs.append(QString("L-Joy(%1,%2)").arg(lx).arg(ly));
    }
    if (std::abs(ry) > kStickDeadzone) {
        activeInputs.append(QString("R-JoyY(%1)").arg(ry));
    }

    Sint16 leftTrigger = getAxisValue(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    Sint16 rightTrigger = getAxisValue(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    if (leftTrigger > 8000)  activeInputs.append(QString("LT(%1)").arg(leftTrigger));
    if (rightTrigger > 8000) activeInputs.append(QString("RT(%1)").arg(rightTrigger));

    QString currentInputStr = activeInputs.join(", ");

    if (!activeInputs.isEmpty()) {
        bool timeElapsed = m_inputThrottleTimer.hasExpired(250);
        bool inputChanged = (currentInputStr != m_lastLoggedInputs);

        if (timeElapsed || inputChanged) {
            emit logMessage(QString("[%1] Active Inputs: %2")
                                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), currentInputStr));
            m_lastLoggedInputs = currentInputStr;
            m_inputThrottleTimer.restart();
        }
    } else {
        m_lastLoggedInputs.clear();
    }
}

void RemoteInputManager::pollControllerInput()
{
    try {
        pollSdlEvents();

        if (!m_connected) {
            if (m_wasConnected) {
                stopActiveJog();
                m_wasConnected = false;
                emit connectionStateChanged(false, "");
            }

            if (m_reconnectElapsedTimer.hasExpired(1000)) {
                if (openAvailableGamepad()) {
                    m_wasConnected = true;
                    QString cName = QString::fromStdString(getControllerName());
                    emit logMessage(QString("[%1] Controller connected: %2")
                                        .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), cName));
                    emit connectionStateChanged(true, cName);
                }
                m_reconnectElapsedTimer.restart();
            }
            return;
        }

        if (!m_wasConnected) {
            m_wasConnected = true;
            emit connectionStateChanged(true, QString::fromStdString(getControllerName()));
        }

        logActiveInputs();

        // Intercept D-Pad and A/B when any pull-down menu (QComboBox popup) is open
        if (m_comboPopupHandlerCallback && m_comboPopupHandlerCallback()) {
            stopActiveJog();
            if (m_guiThrottleTimer.hasExpired(200)) {
                bool popupActionTaken = false;

                if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
                    emit comboPopupNavigateRequested(-1);
                    popupActionTaken = true;
                } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
                    emit comboPopupNavigateRequested(1);
                    popupActionTaken = true;
                } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_SOUTH)) {
                    emit comboPopupSelectRequested();
                    popupActionTaken = true;
                } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_EAST)) {
                    emit comboPopupCloseRequested();
                    popupActionTaken = true;
                }

                if (popupActionTaken) {
                    m_guiThrottleTimer.restart();
                }
            }
            return;
        }

        int stepDistance = 100;
        int speed = 1000;
        if (m_motionParamsCallback) {
            m_motionParamsCallback(stepDistance, speed);
        }

        // Continuous thumbstick jogging (XY on Left Stick, Z on Right Stick)
        processThumbstickJog(speed);

        // Discrete step indexing (D-Pad & Buttons) only when thumbsticks are idle
        if (!m_xyJogActive && !m_zJogActive && m_motionThrottleTimer.hasExpired(256)) {
            processMotionCommands(stepDistance, speed);
            m_motionThrottleTimer.restart();
        }

        if (m_guiThrottleTimer.hasExpired(300)) {
            bool globalActionTaken = false;
            if (isButtonPressed(SDL_GAMEPAD_BUTTON_NORTH)) {
                emit targetPopupRequested();
                globalActionTaken = true;
            }
            if (isButtonPressed(SDL_GAMEPAD_BUTTON_WEST)) {
                emit toggleStreamRequested();
                globalActionTaken = true;
            }
            if (isButtonPressed(SDL_GAMEPAD_BUTTON_BACK)) {
                emit toggleSplitterViewRequested();
                globalActionTaken = true;
            }
            if (globalActionTaken) {
                m_guiThrottleTimer.restart();
            }
        }

        if (m_activeProfile == ControllerProfile::GuiWide) {
            processGuiWideCommands();
        }
    }
    catch (const std::exception& ex) {
        emit logMessage(QString("[%1] <span style='color: #e74c3c;'><b>VISA/Motion Error:</b> %2</span>")
                            .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), QString::fromUtf8(ex.what())));
    }
    catch (...) {
        emit logMessage(QString("[%1] <span style='color: #e74c3c;'><b>Hardware Error:</b> Unknown runtime exception occurred.</span>")
                            .arg(QTime::currentTime().toString("hh:mm:ss.zzz")));
    }
}

void RemoteInputManager::processGuiWideCommands()
{
    if (!m_guiThrottleTimer.hasExpired(300)) return;

    bool actionTaken = false;
    bool menuPressed = isButtonPressed(SDL_GAMEPAD_BUTTON_START);

    if (menuPressed || isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        emit activeTabPopupRequested();
        actionTaken = true;
    }
    if (isButtonPressed(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
        emit switchTabRequested(0);
        actionTaken = true;
    }
    if (isButtonPressed(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        emit switchTabRequested(1);
        actionTaken = true;
    }

    if (actionTaken) {
        m_guiThrottleTimer.restart();
    }
}

void RemoteInputManager::processMotionCommands(int stepDistance, int speed)
{
    if (!m_connected || !m_activeMotor) return;

    switch (m_activeProfile) {
    case ControllerProfile::Zaber4Axis:
        processZaberCommands(stepDistance, speed);
        break;
    case ControllerProfile::LaserWeld3Axis:
        processLaserWeldCommands(stepDistance, speed);
        break;
    default:
        break;
    }
}

void RemoteInputManager::processZaberCommands(int stepDistance, int speed)
{
    if (isButtonPressed(SDL_GAMEPAD_BUTTON_SOUTH)) {
        setSpeed(1, speed);
        moveRelative(1, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_EAST)) {
        setSpeed(1, speed);
        moveRelative(1, -stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        setSpeed(2, speed);
        moveRelative(2, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
        setSpeed(2, speed);
        moveRelative(2, -stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        setSpeed(3, speed);
        moveRelative(3, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        setSpeed(3, speed);
        moveRelative(3, -stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        setSpeed(4, speed);
        moveRelative(4, -stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        setSpeed(4, speed);
        moveRelative(4, stepDistance);
    }
}

void RemoteInputManager::processVelmexCommands(int stepDistance, int speed)
{
    // Hardware layout: Motor 1 = X Axis, Motor 3 = Y Axis, Motor 2 = Z Axis
    if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        setSpeed(1, speed);
        moveRelative(1, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        setSpeed(1, speed);
        moveRelative(1, -stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        setSpeed(3, speed);
        moveRelative(3, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        setSpeed(3, speed);
        moveRelative(3, -stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_SOUTH)) {
        setSpeed(2, speed);
        moveRelative(2, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_EAST)) {
        setSpeed(2, speed);
        moveRelative(2, -stepDistance);
    }
}

void RemoteInputManager::processLaserWeldCommands(int stepDistance, int speed)
{
    processVelmexCommands(stepDistance, speed);

    if (isButtonPressed(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        // TODO: Call ipgYLR trigger emission function here once implemented
    }
}
