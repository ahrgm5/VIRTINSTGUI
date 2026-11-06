// remoteControl.hpp
#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <memory>
#include <imotoraxis.hpp>
#include <zaber.hpp>
#include <velmex.hpp>

enum class ControllerProfile {
    None,
    GuiWide,
    Zaber4Axis,
    LaserWeld3Axis
};

class RemoteControl {
public:
    RemoteControl();
    ~RemoteControl();

    bool initialize();
    void pollEvents();
    bool isConnected() const;
    std::string getControllerName() const;

    bool isButtonPressed(SDL_GamepadButton button) const;
    Sint16 getAxisValue(SDL_GamepadAxis axis) const;

    void setTargetMotor(std::shared_ptr<IMotorAxis> motor);
    void bindZaber(std::shared_ptr<Zaber> zaber);
    void bindVelmex(std::shared_ptr<VelmexVXM> velmex);
    void clearTargetMotor();

    void moveRelative(int axis, int distance);
    void moveAbsolute(int axis, int position);
    void setSpeed(int axis, int speed);
    void indexIncremental(int axis, int steps);
    void stop(int axis = 0);

    void processMotionCommands(int stepDistance = 200, int speed = 2000 );
    void setProfile(ControllerProfile profile) { m_activeProfile = profile; }
    ControllerProfile getActiveProfile() const { return m_activeProfile; }

private:
    void processZaberCommands(int stepDistance, int speed);
    void processVelmexCommands(int stepDistance, int speed);
    void processLaserWeldCommands(int stepDistance, int speed);

    SDL_Gamepad* m_controller;
    bool m_connected;
    bool m_isInitialized = false; // Added to track SDL subsystem initialization state
    std::shared_ptr<IMotorAxis> m_activeMotor;
    ControllerProfile m_activeProfile = ControllerProfile::GuiWide; // Default to GuiWide
};