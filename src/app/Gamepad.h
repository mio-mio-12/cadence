#pragma once

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace cadence::gamepad {

enum class ControllerType {
    None,
    Xbox,
    DualShock3,
    DualShock4,
    DualSense,
    Generic
};

enum class Button {
    A,              // Cross on PlayStation
    B,              // Circle on PlayStation
    X,              // Square on PlayStation
    Y,              // Triangle on PlayStation
    LeftBumper,     // L1
    RightBumper,    // R1
    Back,           // View / Share / Create / Select
    Start,          // Menu / Options / Start
    Guide,          // Xbox / PS Button
    LeftThumb,      // L3 (Left Stick Click)
    RightThumb,     // R3 (Right Stick Click)
    DpadUp,
    DpadRight,
    DpadDown,
    DpadLeft
};

struct SensitivityPreset {
    const char* name;
    float horizontal;
    float vertical;
};

inline constexpr SensitivityPreset kSensitivityPresets[] = {
    {"Standard (1.0x)", 2.6f, 2.0f},
    {"High (1.5x)", 3.9f, 3.0f},
    {"Very High (2.0x)", 5.2f, 4.0f},
    {"Insane (3.0x)", 7.8f, 6.0f}
};

struct GamepadConfig {
    float horizontalSensitivity{2.6f};
    float verticalSensitivity{2.0f};
    float adsSensitivityMultiplier{0.75f};
    float innerDeadzone{0.14f};
    float outerDeadzone{0.96f};
    float triggerThreshold{0.18f};
    bool invertPitch{false};
    bool sprintToggle{true};
    int sensitivityPresetIndex{0};

    void cycleSensitivityPreset() {
        constexpr int count = static_cast<int>(sizeof(kSensitivityPresets) / sizeof(kSensitivityPresets[0]));
        sensitivityPresetIndex = (sensitivityPresetIndex + 1) % count;
        horizontalSensitivity = kSensitivityPresets[sensitivityPresetIndex].horizontal;
        verticalSensitivity = kSensitivityPresets[sensitivityPresetIndex].vertical;
    }

    void setSensitivityPreset(int index) {
        constexpr int count = static_cast<int>(sizeof(kSensitivityPresets) / sizeof(kSensitivityPresets[0]));
        if (index >= 0 && index < count) {
            sensitivityPresetIndex = index;
            horizontalSensitivity = kSensitivityPresets[sensitivityPresetIndex].horizontal;
            verticalSensitivity = kSensitivityPresets[sensitivityPresetIndex].vertical;
        }
    }
};

struct GamepadState {
    bool connected{false};
    int joystickId{-1};
    std::string name;
    std::string guid;
    ControllerType type{ControllerType::None};

    // Filtered analog stick axes (post-deadzone & response curve, range [-1.0, 1.0])
    float leftStickX{0.0f};
    float leftStickY{0.0f};   // Forward is positive (+1.0), backward is negative (-1.0)
    float rightStickX{0.0f};
    float rightStickY{0.0f};  // Up is positive (+1.0), down is negative (-1.0)

    // Raw analog stick magnitudes for visualizer
    float leftStickMagnitude{0.0f};
    float rightStickMagnitude{0.0f};

    // Analog triggers (range [0.0, 1.0])
    float leftTrigger{0.0f};
    float rightTrigger{0.0f};

    // Button down states
    bool a{false};
    bool b{false};
    bool x{false};
    bool y{false};
    bool leftBumper{false};
    bool rightBumper{false};
    bool back{false};
    bool start{false};
    bool guide{false};
    bool leftThumb{false};
    bool rightThumb{false};
    bool dpadUp{false};
    bool dpadRight{false};
    bool dpadDown{false};
    bool dpadLeft{false};

    // Button edge-triggered states (true for 1 frame only on press)
    bool aPressed{false};
    bool bPressed{false};
    bool xPressed{false};
    bool yPressed{false};
    bool leftBumperPressed{false};
    bool rightBumperPressed{false};
    bool backPressed{false};
    bool startPressed{false};
    bool guidePressed{false};
    bool leftThumbPressed{false};
    bool rightThumbPressed{false};
    bool dpadUpPressed{false};
    bool dpadRightPressed{false};
    bool dpadDownPressed{false};
    bool dpadLeftPressed{false};

    // Button release edge states
    bool bReleased{false};

    // Hold durations & long-press detection
    float bHeldDuration{0.0f};
    bool bLongHeldTriggered{false};  // True when B has been held >= 0.35s
    float rightThumbHeldDuration{0.0f};
    bool rightThumbLongHeldTriggered{false};  // True when R3 has been held >= 0.35s (for Tactical prone toggle)

    // Trigger edge detection
    bool leftTriggerPressed{false};
    bool rightTriggerPressed{false};
    bool previousLeftTriggerDown{false};
    bool previousRightTriggerDown{false};

    // Previous frame down states for edge calculation
    bool prevA{false};
    bool prevB{false};
    bool prevX{false};
    bool prevY{false};
    bool prevLeftBumper{false};
    bool prevRightBumper{false};
    bool prevBack{false};
    bool prevStart{false};
    bool prevGuide{false};
    bool prevLeftThumb{false};
    bool prevRightThumb{false};
    bool prevDpadUp{false};
    bool prevDpadRight{false};
    bool prevDpadDown{false};
    bool prevDpadLeft{false};
};

inline ControllerType identifyController(std::string_view name, std::string_view guid = "") {
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    if (lower.find("dualsense") != std::string::npos || lower.find("ps5") != std::string::npos) {
        return ControllerType::DualSense;
    }
    if (lower.find("dualshock 4") != std::string::npos || lower.find("ps4") != std::string::npos) {
        return ControllerType::DualShock4;
    }
    if (lower.find("dualshock 3") != std::string::npos || lower.find("ps3") != std::string::npos ||
        lower.find("playstation(r)3") != std::string::npos || lower.find("sixaxis") != std::string::npos) {
        return ControllerType::DualShock3;
    }
    if (lower.find("xbox") != std::string::npos || lower.find("xinput") != std::string::npos ||
        lower.find("x-box") != std::string::npos) {
        return ControllerType::Xbox;
    }
    // Check Sony VID 054C in GUID string (e.g. "030000004c05...")
    if (guid.size() >= 12 && guid.substr(8, 4) == "4c05") {
        std::string pid = std::string(guid.substr(16, 4));
        if (pid == "e60c" || pid == "f20d") return ControllerType::DualSense;
        if (pid == "c405" || pid == "cc09" || pid == "a00b") return ControllerType::DualShock4;
        if (pid == "6802") return ControllerType::DualShock3;
        return ControllerType::DualShock4;
    }
    if (lower.find("wireless controller") != std::string::npos) {
        return ControllerType::DualShock4; // Standard Windows Bluetooth name for DS4 / DualSense
    }
    return ControllerType::Generic;
}

inline const char* getButtonLabel(ControllerType type, Button button) {
    const bool isSony = (type == ControllerType::DualShock3 ||
                         type == ControllerType::DualShock4 ||
                         type == ControllerType::DualSense);

    switch (button) {
        case Button::A: return isSony ? "Cross" : "A";
        case Button::B: return isSony ? "Circle" : "B";
        case Button::X: return isSony ? "Square" : "X";
        case Button::Y: return isSony ? "Triangle" : "Y";
        case Button::LeftBumper: return isSony ? "L1" : "LB";
        case Button::RightBumper: return isSony ? "R1" : "RB";
        case Button::Back: return (type == ControllerType::DualSense) ? "Create" : (isSony ? "Share" : "View");
        case Button::Start: return isSony ? "Options" : "Menu";
        case Button::Guide: return isSony ? "PS" : "Xbox";
        case Button::LeftThumb: return isSony ? "L3" : "Left Stick";
        case Button::RightThumb: return isSony ? "R3" : "Right Stick";
        case Button::DpadUp: return "D-Pad Up";
        case Button::DpadRight: return "D-Pad Right";
        case Button::DpadDown: return "D-Pad Down";
        case Button::DpadLeft: return "D-Pad Left";
        default: return "Unknown";
    }
}

inline const char* getControllerTypeName(ControllerType type) {
    switch (type) {
        case ControllerType::Xbox: return "Xbox Controller";
        case ControllerType::DualShock3: return "PlayStation DualShock 3";
        case ControllerType::DualShock4: return "PlayStation DualShock 4";
        case ControllerType::DualSense: return "PlayStation DualSense (PS5)";
        case ControllerType::Generic: return "Generic Gamepad";
        case ControllerType::None:
        default: return "None";
    }
}

// Applies a radial deadzone and smooth response curve to 2D stick axes.
inline void applyRadialDeadzone(float rawX, float rawY, float innerDeadzone, float outerDeadzone,
                                float& outX, float& outY, float& outMagnitude) {
    const float mag = std::sqrt(rawX * rawX + rawY * rawY);
    outMagnitude = std::clamp(mag, 0.0f, 1.0f);

    if (mag <= innerDeadzone) {
        outX = 0.0f;
        outY = 0.0f;
        return;
    }

    const float norm = std::min(1.0f, (mag - innerDeadzone) / std::max(0.001f, outerDeadzone - innerDeadzone));
    // Quadratic response curve for smooth micro-aiming and responsive full-tilt
    const float curved = norm * norm;
    outX = (rawX / mag) * curved;
    outY = (rawY / mag) * curved;
}

// Registers community SDL GameControllerDB mappings for Windows for PlayStation & Xbox controllers.
inline void initializeMappings() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    // Comprehensive Windows mappings for Sony & Xbox controllers:
    // DS3, DS4 v1 & v2 (USB & Bluetooth), DS4 wireless dongle, DualSense & DualSense Edge (USB & Bluetooth)
    static const char* kCustomMappings =
        // DualShock 3 (Sony USB / Bluetooth)
        "030000004c0500006802000000000000,PS3 Controller,a:b14,b:b13,back:b0,dpdown:b6,dpleft:b7,dpright:b5,dpup:b4,guide:b16,leftshoulder:b10,leftstick:b1,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b11,rightstick:b2,righttrigger:b9,rightx:a2,righty:a3,start:b3,x:b15,y:b12,platform:Windows,\n"
        // DualShock 4 v1 (USB: 054c:05c4)
        "030000004c050000c405000000000000,PS4 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b0,y:b3,platform:Windows,\n"
        // DualShock 4 v2 (USB / Bluetooth: 054c:09cc)
        "030000004c050000cc09000000000000,PS4 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b0,y:b3,platform:Windows,\n"
        // DualShock 4 Wireless Adapter (054c:0ba0)
        "030000004c050000a00b000000000000,PS4 Wireless Adapter,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b0,y:b3,platform:Windows,\n"
        // DualSense (USB: 054c:0ce6)
        "030000004c050000e60c000000000000,PS5 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b0,y:b3,platform:Windows,\n"
        // DualSense (Bluetooth / Alternative: 054c:0df2)
        "030000004c050000f20d000000000000,PS5 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,x:b0,y:b3,platform:Windows,\n"
        // Xbox 360 / Xbox One XInput
        "78696e70757401000000000000000000,XInput Controller,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b8,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b9,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Windows,\n";

    glfwUpdateGamepadMappings(kCustomMappings);
}

// Polls active gamepad input, computes deadzones and edge triggers.
inline void poll(GamepadState& state, const GamepadConfig& config, float deltaSeconds) {
    initializeMappings();

    // Preserve previous down states for edge detection
    state.prevA = state.a;
    state.prevB = state.b;
    state.prevX = state.x;
    state.prevY = state.y;
    state.prevLeftBumper = state.leftBumper;
    state.prevRightBumper = state.rightBumper;
    state.prevBack = state.back;
    state.prevStart = state.start;
    state.prevGuide = state.guide;
    state.prevLeftThumb = state.leftThumb;
    state.prevRightThumb = state.rightThumb;
    state.prevDpadUp = state.dpadUp;
    state.prevDpadRight = state.dpadRight;
    state.prevDpadDown = state.dpadDown;
    state.prevDpadLeft = state.dpadLeft;
    state.previousLeftTriggerDown = (state.leftTrigger > config.triggerThreshold);
    state.previousRightTriggerDown = (state.rightTrigger > config.triggerThreshold);

    // Reset edges
    state.aPressed = false;
    state.bPressed = false;
    state.xPressed = false;
    state.yPressed = false;
    state.leftBumperPressed = false;
    state.rightBumperPressed = false;
    state.backPressed = false;
    state.startPressed = false;
    state.guidePressed = false;
    state.leftThumbPressed = false;
    state.rightThumbPressed = false;
    state.dpadUpPressed = false;
    state.dpadRightPressed = false;
    state.dpadDownPressed = false;
    state.dpadLeftPressed = false;
    state.bReleased = false;
    state.leftTriggerPressed = false;
    state.rightTriggerPressed = false;

    // Scan for connected gamepads (prioritize previously active joystick if still connected)
    int targetJid = -1;
    if (state.joystickId >= GLFW_JOYSTICK_1 && state.joystickId <= GLFW_JOYSTICK_LAST &&
        glfwJoystickPresent(state.joystickId)) {
        targetJid = state.joystickId;
    } else {
        for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_4; ++jid) {
            if (glfwJoystickPresent(jid)) {
                targetJid = jid;
                break;
            }
        }
    }

    if (targetJid < 0) {
        state.connected = false;
        state.joystickId = -1;
        state.type = ControllerType::None;
        state.name.clear();
        state.leftStickX = state.leftStickY = 0.0f;
        state.rightStickX = state.rightStickY = 0.0f;
        state.leftTrigger = state.rightTrigger = 0.0f;
        state.bHeldDuration = 0.0f;
        state.bLongHeldTriggered = false;
        state.rightThumbHeldDuration = 0.0f;
        state.rightThumbLongHeldTriggered = false;
        return;
    }

    state.connected = true;
    state.joystickId = targetJid;
    const char* rawName = glfwGetJoystickName(targetJid);
    state.name = rawName ? rawName : "Controller";
    const char* rawGuid = glfwGetJoystickGUID(targetJid);
    state.guid = rawGuid ? rawGuid : "";
    state.type = identifyController(state.name, state.guid);

    GLFWgamepadstate gp{};
    if (glfwGetGamepadState(targetJid, &gp)) {
        // Left stick: GLFW Y is negative forward (-1 = up, +1 = down)
        // Convert to game convention: forward is +1.0, backward is -1.0
        float rawLx = gp.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
        float rawLy = -gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
        applyRadialDeadzone(rawLx, rawLy, config.innerDeadzone, config.outerDeadzone,
                            state.leftStickX, state.leftStickY, state.leftStickMagnitude);

        // Right stick: GLFW Y is negative up (-1 = up, +1 = down)
        // Convert to: up is +1.0, down is -1.0
        float rawRx = gp.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
        float rawRy = -gp.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
        applyRadialDeadzone(rawRx, rawRy, config.innerDeadzone, config.outerDeadzone,
                            state.rightStickX, state.rightStickY, state.rightStickMagnitude);

        // Triggers in GLFW are in [-1.0, 1.0]; normalize to [0.0, 1.0]
        state.leftTrigger = std::clamp((gp.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.0f) * 0.5f, 0.0f, 1.0f);
        state.rightTrigger = std::clamp((gp.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0f) * 0.5f, 0.0f, 1.0f);

        // Buttons
        state.a = (gp.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS);
        state.b = (gp.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS);
        state.x = (gp.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS);
        state.y = (gp.buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS);
        state.leftBumper = (gp.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS);
        state.rightBumper = (gp.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS);
        state.back = (gp.buttons[GLFW_GAMEPAD_BUTTON_BACK] == GLFW_PRESS);
        state.start = (gp.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS);
        state.guide = (gp.buttons[GLFW_GAMEPAD_BUTTON_GUIDE] == GLFW_PRESS);
        state.leftThumb = (gp.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] == GLFW_PRESS);
        state.rightThumb = (gp.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] == GLFW_PRESS);
        state.dpadUp = (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS);
        state.dpadRight = (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS);
        state.dpadDown = (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS);
        state.dpadLeft = (gp.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] == GLFW_PRESS);
    } else {
        // Fallback for unmapped joysticks (read raw axes & buttons)
        int axisCount = 0;
        const float* axes = glfwGetJoystickAxes(targetJid, &axisCount);
        if (axes && axisCount >= 4) {
            float rawLx = axes[0];
            float rawLy = -axes[1];
            applyRadialDeadzone(rawLx, rawLy, config.innerDeadzone, config.outerDeadzone,
                                state.leftStickX, state.leftStickY, state.leftStickMagnitude);

            float rawRx = (axisCount > 2) ? axes[2] : 0.0f;
            float rawRy = (axisCount > 3) ? -axes[3] : 0.0f;
            applyRadialDeadzone(rawRx, rawRy, config.innerDeadzone, config.outerDeadzone,
                                state.rightStickX, state.rightStickY, state.rightStickMagnitude);

            if (axisCount >= 6) {
                state.leftTrigger = std::clamp((axes[4] + 1.0f) * 0.5f, 0.0f, 1.0f);
                state.rightTrigger = std::clamp((axes[5] + 1.0f) * 0.5f, 0.0f, 1.0f);
            }
        }
        int btnCount = 0;
        const unsigned char* btns = glfwGetJoystickButtons(targetJid, &btnCount);
        if (btns) {
            if (btnCount > 0) state.a = (btns[0] == GLFW_PRESS);
            if (btnCount > 1) state.b = (btns[1] == GLFW_PRESS);
            if (btnCount > 2) state.x = (btns[2] == GLFW_PRESS);
            if (btnCount > 3) state.y = (btns[3] == GLFW_PRESS);
            if (btnCount > 4) state.leftBumper = (btns[4] == GLFW_PRESS);
            if (btnCount > 5) state.rightBumper = (btns[5] == GLFW_PRESS);
            if (btnCount > 6) state.back = (btns[6] == GLFW_PRESS);
            if (btnCount > 7) state.start = (btns[7] == GLFW_PRESS);
            if (btnCount > 8) state.leftThumb = (btns[8] == GLFW_PRESS);
            if (btnCount > 9) state.rightThumb = (btns[9] == GLFW_PRESS);
        }
    }

    // Compute edge triggers
    state.aPressed = state.a && !state.prevA;
    state.bPressed = state.b && !state.prevB;
    state.xPressed = state.x && !state.prevX;
    state.yPressed = state.y && !state.prevY;
    state.leftBumperPressed = state.leftBumper && !state.prevLeftBumper;
    state.rightBumperPressed = state.rightBumper && !state.prevRightBumper;
    state.backPressed = state.back && !state.prevBack;
    state.startPressed = state.start && !state.prevStart;
    state.guidePressed = state.guide && !state.prevGuide;
    state.leftThumbPressed = state.leftThumb && !state.prevLeftThumb;
    state.rightThumbPressed = state.rightThumb && !state.prevRightThumb;
    state.dpadUpPressed = state.dpadUp && !state.prevDpadUp;
    state.dpadRightPressed = state.dpadRight && !state.prevDpadRight;
    state.dpadDownPressed = state.dpadDown && !state.prevDpadDown;
    state.dpadLeftPressed = state.dpadLeft && !state.prevDpadLeft;
    state.bReleased = !state.b && state.prevB;

    const bool currLt = (state.leftTrigger > config.triggerThreshold);
    const bool currRt = (state.rightTrigger > config.triggerThreshold);
    state.leftTriggerPressed = currLt && !state.previousLeftTriggerDown;
    state.rightTriggerPressed = currRt && !state.previousRightTriggerDown;

    // Track B hold duration
    if (state.b) {
        state.bHeldDuration += deltaSeconds;
        if (state.bHeldDuration >= 0.35f && !state.bLongHeldTriggered) {
            state.bLongHeldTriggered = true; // Long-press threshold crossed this frame
        }
    } else {
        state.bHeldDuration = 0.0f;
        state.bLongHeldTriggered = false;
    }

    // Track RightThumb (R3) hold duration for Tactical Crouch vs Prone
    if (state.rightThumb) {
        state.rightThumbHeldDuration += deltaSeconds;
        if (state.rightThumbHeldDuration >= 0.35f && !state.rightThumbLongHeldTriggered) {
            state.rightThumbLongHeldTriggered = true; // Long-press threshold crossed this frame
        }
    } else {
        state.rightThumbHeldDuration = 0.0f;
        state.rightThumbLongHeldTriggered = false;
    }
}

} // namespace cadence::gamepad
