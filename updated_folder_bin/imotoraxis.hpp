#pragma once

#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include <string>

struct AxisLimits {
    int32_t minPos = 0;
    int32_t maxPos = 0;
    int32_t maxSpeed = 0;
    bool enabled = false;
};

class IMotorAxis {
public:
    virtual ~IMotorAxis() = default;

    // Default global limits (backward compatibility)
    int topSpeed = 2000;
    int minPos = 0;
    int maxPos = 0;

    // Per-axis limit mapping
    std::unordered_map<int, AxisLimits> axisLimits;

    // Helper to configure limits for a specific axis
    void setAxisLimits(int axis, int32_t minP, int32_t maxP, int32_t maxSpd = 0) {
        axisLimits[axis] = { minP, maxP, maxSpd, true };
    }

    // Returns current configured min/max boundary for an axis
    std::pair<int32_t, int32_t> getAxisBounds(int axis) const {
        auto it = axisLimits.find(axis);
        if (it != axisLimits.end() && it->second.enabled) {
            return { it->second.minPos, it->second.maxPos };
        }
        return { minPos, maxPos };
    }

    // Validates an absolute target position against configured boundaries
    virtual int32_t validatePosition(int axis, int32_t targetPosition) const {
        auto it = axisLimits.find(axis);
        if (it != axisLimits.end() && it->second.enabled) {
            if (targetPosition < it->second.minPos || targetPosition > it->second.maxPos) {
                throw std::out_of_range(
                    "Axis " + std::to_string(axis) + " command target position (" +
                    std::to_string(targetPosition) + ") exceeds mechanical limit [" +
                    std::to_string(it->second.minPos) + ", " +
                    std::to_string(it->second.maxPos) + "]."
                    );
            }
            return targetPosition;
        }

        // Fallback to global limits if configured (minPos != maxPos)
        if (minPos != maxPos) {
            if (targetPosition < minPos || targetPosition > maxPos) {
                throw std::out_of_range(
                    "Axis " + std::to_string(axis) + " command target position (" +
                    std::to_string(targetPosition) + ") exceeds global limit [" +
                    std::to_string(minPos) + ", " +
                    std::to_string(maxPos) + "]."
                    );
            }
        }
        return targetPosition;
    }

    // Validates and clamps speed according to per-axis or global limits
    virtual int validateSpeed(int axis, int speed) const {
        int maxSpd = topSpeed;
        auto it = axisLimits.find(axis);
        if (it != axisLimits.end() && it->second.enabled && it->second.maxSpeed > 0) {
            maxSpd = it->second.maxSpeed;
        }
        if (speed > maxSpd && maxSpd > 0) {
            return maxSpd;
        }
        return speed;
    }

    virtual void stop(int axis = 0) = 0;
    virtual void setSpeed(int axis, int speed) = 0;
    virtual void moveRelative(int axis, int distance) = 0;
    virtual void moveAbsolute(int axis, int position) = 0;
    virtual void home(int axis) = 0;
    virtual int32_t getPosition(int axis) = 0;

    // Continuous Jog Hooks (Override in drivers for smooth thumbstick slewing)
    virtual void jogSingleAxis(int axis, int direction, int speed) {
        if (direction == 0 || speed <= 0) {
            stop(axis);
            return;
        }
        auto [limMin, limMax] = getAxisBounds(axis);
        int32_t target = (direction > 0) ? limMax : limMin;
        setSpeed(axis, speed);
        moveAbsolute(axis, target);
    }

    virtual void jogDualAxis(int axisMaster, int dirMaster, int speedMaster,
                             int axisSlave, int dirSlave, int speedSlave) {
        if (dirMaster != 0 && speedMaster > 0) {
            jogSingleAxis(axisMaster, dirMaster, speedMaster);
        } else {
            stop(axisMaster);
        }
        if (dirSlave != 0 && speedSlave > 0) {
            jogSingleAxis(axisSlave, dirSlave, speedSlave);
        } else {
            stop(axisSlave);
        }
    }
};
