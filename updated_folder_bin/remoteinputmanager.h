#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QString>
#include <SDL3/SDL.h>
#include <functional>
#include <memory>
#include <string>

#include <imotoraxis.hpp>
#include <instrument.hpp>

enum class ControllerProfile {
    None,
    GuiWide,
    Zaber4Axis,
    LaserWeld3Axis
};

class RemoteInputManager : public QObject {
    Q_OBJECT

public:
    explicit RemoteInputManager(
        std::function<void(int&, int&)> motionParamsCallback,
        std::function<bool()> comboPopupHandlerCallback,
        QObject *parent = nullptr
        );
    ~RemoteInputManager() override;

    // Controller Connection & State
    bool initializeController();
    bool isConnected() const;
    std::string getControllerName() const;
    bool isInputActive() const;

    bool isButtonPressed(SDL_GamepadButton button) const;
    Sint16 getAxisValue(SDL_GamepadAxis axis) const;

    // Target Motor Binding (Device enforces its own limits natively via IMotorAxis)
    void bindTargetDevice(const QString& devName, std::shared_ptr<Instrument> inst);
    void setTargetMotor(std::shared_ptr<IMotorAxis> motor, ControllerProfile profile);
    void clearTargetMotor();

    // Direct Motion Helpers
    void moveRelative(int axis, int distance);
    void moveAbsolute(int axis, int position);
    void setSpeed(int axis, int speed);
    void stop(int axis = 0);

    void setProfile(ControllerProfile profile) { m_activeProfile = profile; }
    ControllerProfile getActiveProfile() const { return m_activeProfile; }

signals:
    void connectionStateChanged(bool connected, const QString& controllerName);
    void logMessage(const QString& msg);

    // Global & GUI Wide Actions
    void targetPopupRequested();
    void toggleStreamRequested();
    void toggleSplitterViewRequested();
    void activeTabPopupRequested();
    void switchTabRequested(int tabIndex);

    // Active Pull-Down Menu (QComboBox Popup) Navigation
    void comboPopupNavigateRequested(int delta);
    void comboPopupSelectRequested();
    void comboPopupCloseRequested();

private slots:
    void pollControllerInput();

private:
    bool openAvailableGamepad();
    void pollSdlEvents();
    void logActiveInputs();
    void processGuiWideCommands();
    void processMotionCommands(int stepDistance = 200, int speed = 2000);
    void processZaberCommands(int stepDistance, int speed);
    void processVelmexCommands(int stepDistance, int speed);
    void processLaserWeldCommands(int stepDistance, int speed);
    void processThumbstickJog(int maxUiSpeed);
    void stopActiveJog();

    static int computeQuantizedSpeed(Sint16 rawVal, int deadzone, int minSpeed, int maxSpeed);

    // SDL3 Gamepad State
    SDL_Gamepad *m_controller = nullptr;
    bool m_connected = false;
    bool m_isInitialized = false;
    std::shared_ptr<IMotorAxis> m_activeMotor;
    ControllerProfile m_activeProfile = ControllerProfile::GuiWide;

    // Continuous Thumbstick Jog State Machine
    bool m_xyFineMode = false;
    bool m_zFineMode = false;
    bool m_prevLeftStickBtn = false;
    bool m_prevRightStickBtn = false;

    bool m_xyJogActive = false;
    int m_lastJogDirX = 0;
    int m_lastJogDirY = 0;
    int m_lastJogSpeedX = 0;
    int m_lastJogSpeedY = 0;

    bool m_zJogActive = false;
    int m_lastJogDirZ = 0;
    int m_lastJogSpeedZ = 0;

    // Polling & Throttling Timers
    QTimer *m_controllerTimer = nullptr;
    QElapsedTimer m_inputThrottleTimer;
    QElapsedTimer m_reconnectElapsedTimer;
    QElapsedTimer m_guiThrottleTimer;
    QElapsedTimer m_motionThrottleTimer;
    QElapsedTimer m_jogUpdateThrottleTimer;

    QString m_lastLoggedInputs;
    bool m_wasConnected = false;

    std::function<void(int&, int&)> m_motionParamsCallback;
    std::function<bool()> m_comboPopupHandlerCallback;
};
