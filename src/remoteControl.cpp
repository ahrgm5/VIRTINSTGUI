// remoteControl.cpp
#include <remoteControl.hpp>
#include <iostream>
#include <cstdlib>
#include <cmath>

using namespace std;

RemoteControl::RemoteControl() : m_controller(nullptr), m_connected(false), m_isInitialized(false) {}

RemoteControl::~RemoteControl() {
    if (m_controller) {
        SDL_CloseGamepad(m_controller);
        m_controller = nullptr;
    }
    if (m_isInitialized) {
        SDL_Quit();
        m_isInitialized = false;
    }
}

bool RemoteControl::initialize() {
    // Only initialize SDL subsystems once to prevent reference count overflow (max 255)
    if (!m_isInitialized) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            return false;
        }
        m_isInitialized = true;
    }

    int numJoysticks = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks);
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

void RemoteControl::pollEvents() {
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
        }
    }
}

bool RemoteControl::isConnected() const {
    return m_connected;
}

std::string RemoteControl::getControllerName() const {
    if (m_controller) {
        const char* name = SDL_GetGamepadName(m_controller);
        return name ? name : "Unknown";
    }
    return "None";
}

bool RemoteControl::isButtonPressed(SDL_GamepadButton button) const {
    if (m_controller && m_connected) {
        return SDL_GetGamepadButton(m_controller, button);
    }
    return false;
}

Sint16 RemoteControl::getAxisValue(SDL_GamepadAxis axis) const {
    if (m_controller && m_connected) {
        return SDL_GetGamepadAxis(m_controller, axis);
    }
    return 0;
}

void RemoteControl::moveRelative(int axis, int distance) {
    if (m_activeMotor) m_activeMotor->moveRelative(axis, distance);
}

void RemoteControl::moveAbsolute(int axis, int position) {
    if (m_activeMotor) m_activeMotor->moveAbsolute(axis, position);
}

void RemoteControl::setSpeed(int axis, int speed) {
    if (m_activeMotor) m_activeMotor->setSpeed(axis, speed);
}


void RemoteControl::stop(int axis) {
    if (m_activeMotor) m_activeMotor->stop(axis);
}

void RemoteControl::setTargetMotor(shared_ptr<IMotorAxis> motor) {
    m_activeMotor = motor;
}

void RemoteControl::bindZaber(shared_ptr<Zaber> zaber) {
    if (zaber) {
        m_activeMotor = make_shared<ZaberAdapter>(zaber);
        m_activeProfile = ControllerProfile::Zaber4Axis;
    } else {
        clearTargetMotor();
    }
}

void RemoteControl::bindVelmex(shared_ptr<VelmexVXM> velmex) {
    if (velmex) {
        m_activeMotor = make_shared<VelmexAdapter>(velmex);
        m_activeProfile = ControllerProfile::LaserWeld3Axis;
    } else {
        clearTargetMotor();
    }
}

void RemoteControl::clearTargetMotor() {
    m_activeMotor.reset();
    m_activeProfile = ControllerProfile::None;
}

void RemoteControl::processMotionCommands(int stepDistance, int speed) {
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

void RemoteControl::processZaberCommands(int stepDistance, int speed) {
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

void RemoteControl::processVelmexCommands(int stepDistance, int speed ) {
    if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        setSpeed(1, speed);
        moveRelative(1, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        setSpeed(1, speed);
        moveRelative(1, -stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        setSpeed(2, speed);
        moveRelative(2, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        setSpeed(2, speed);
        moveRelative(2, -stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_SOUTH)) {
        setSpeed(3, speed);
        moveRelative(3, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_EAST)) {
        setSpeed(3, speed);
        moveRelative(3, -stepDistance);
    }
}

void RemoteControl::processLaserWeldCommands(int stepDistance, int speed) {
    processVelmexCommands(stepDistance, speed);

    if (isButtonPressed(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        // TODO: Call ipgYLR trigger emission function here once implemented
    }
}