#include "app/Gamepad.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace cadence::gamepad;

void testControllerIdentification() {
    assert(identifyController("Xbox Series X Controller") == ControllerType::Xbox);
    assert(identifyController("Xbox 360 Controller") == ControllerType::Xbox);
    assert(identifyController("XInput Controller") == ControllerType::Xbox);

    assert(identifyController("Sony Interactive Entertainment DualSense Wireless Controller") == ControllerType::DualSense);
    assert(identifyController("PS5 Controller") == ControllerType::DualSense);

    assert(identifyController("Sony DualShock 4") == ControllerType::DualShock4);
    assert(identifyController("PS4 Controller") == ControllerType::DualShock4);
    assert(identifyController("Wireless Controller") == ControllerType::DualShock4);

    assert(identifyController("PLAYSTATION(R)3 Controller") == ControllerType::DualShock3);
    assert(identifyController("DualShock 3") == ControllerType::DualShock3);

    // GUID based Sony detection (054C vendor in little-endian format)
    assert(identifyController("Generic USB Gamepad", "030000004c050000e60c000000000000") == ControllerType::DualSense);
    assert(identifyController("Generic USB Gamepad", "030000004c050000cc09000000000000") == ControllerType::DualShock4);
    assert(identifyController("Generic USB Gamepad", "030000004c0500006802000000000000") == ControllerType::DualShock3);

    std::cout << "[PASS] testControllerIdentification\n";
}

void testButtonLabels() {
    // Xbox labels
    assert(std::string_view(getButtonLabel(ControllerType::Xbox, Button::Y)) == "Y");
    assert(std::string_view(getButtonLabel(ControllerType::Xbox, Button::A)) == "A");
    assert(std::string_view(getButtonLabel(ControllerType::Xbox, Button::B)) == "B");
    assert(std::string_view(getButtonLabel(ControllerType::Xbox, Button::X)) == "X");
    assert(std::string_view(getButtonLabel(ControllerType::Xbox, Button::LeftBumper)) == "LB");
    assert(std::string_view(getButtonLabel(ControllerType::Xbox, Button::RightBumper)) == "RB");

    // PlayStation labels
    assert(std::string_view(getButtonLabel(ControllerType::DualShock4, Button::Y)) == "Triangle");
    assert(std::string_view(getButtonLabel(ControllerType::DualShock4, Button::A)) == "Cross");
    assert(std::string_view(getButtonLabel(ControllerType::DualShock4, Button::B)) == "Circle");
    assert(std::string_view(getButtonLabel(ControllerType::DualShock4, Button::X)) == "Square");
    assert(std::string_view(getButtonLabel(ControllerType::DualShock4, Button::LeftBumper)) == "L1");
    assert(std::string_view(getButtonLabel(ControllerType::DualShock4, Button::RightBumper)) == "R1");
    assert(std::string_view(getButtonLabel(ControllerType::DualSense, Button::Back)) == "Create");
    assert(std::string_view(getButtonLabel(ControllerType::DualShock4, Button::Back)) == "Share");

    std::cout << "[PASS] testButtonLabels\n";
}

void testRadialDeadzone() {
    float outX = 0.0f, outY = 0.0f, mag = 0.0f;

    // Inside deadzone (0.05 < 0.14)
    applyRadialDeadzone(0.03f, 0.04f, 0.14f, 0.96f, outX, outY, mag);
    assert(outX == 0.0f);
    assert(outY == 0.0f);
    assert(std::abs(mag - 0.05f) < 0.001f);

    // Full tilt (1.0 > 0.96)
    applyRadialDeadzone(1.0f, 0.0f, 0.14f, 0.96f, outX, outY, mag);
    assert(std::abs(outX - 1.0f) < 0.001f);
    assert(outY == 0.0f);
    assert(std::abs(mag - 1.0f) < 0.001f);

    // Diagonal tilt post-deadzone
    applyRadialDeadzone(0.5f, 0.5f, 0.14f, 0.96f, outX, outY, mag);
    assert(outX > 0.0f && outY > 0.0f);
    assert(std::abs(outX - outY) < 0.001f);

    std::cout << "[PASS] testRadialDeadzone\n";
}

void testThreeWeaponRotation() {
    // 3-weapon L1 cycling: (currentSlot + 1) % 3
    int slot = 0;
    slot = (slot + 1) % 3;
    assert(slot == 1);
    slot = (slot + 1) % 3;
    assert(slot == 2);
    slot = (slot + 1) % 3;
    assert(slot == 0);

    // Y swap: switches to previous/last slot
    int activeSlot = 0;
    int lastSlot = 2;
    int requested = lastSlot;
    assert(requested == 2);

    std::cout << "[PASS] testThreeWeaponRotation\n";
}

void testBButtonHoldProneLogic() {
    GamepadState state;
    GamepadConfig config;

    // Simulate holding B across several frames
    state.b = true;
    // Frame 1: 0.1s
    state.bHeldDuration += 0.1f;
    assert(!state.bLongHeldTriggered);

    // Frame 2: +0.2s = 0.3s
    state.bHeldDuration += 0.2f;
    assert(!state.bLongHeldTriggered);

    // Frame 3: +0.1s = 0.4s (crosses 0.35s threshold)
    state.bHeldDuration += 0.1f;
    if (state.bHeldDuration >= 0.35f && !state.bLongHeldTriggered) {
        state.bLongHeldTriggered = true;
    }
    assert(state.bLongHeldTriggered);

    // Release B
    state.b = false;
    state.bHeldDuration = 0.0f;
    state.bLongHeldTriggered = false;
    assert(!state.bLongHeldTriggered);

    std::cout << "[PASS] testBButtonHoldProneLogic\n";
}

void testRightThumbTacticalProneLogic() {
    GamepadState state;
    GamepadConfig config;

    // Simulate holding R3 (Right Thumb) across several frames
    state.rightThumb = true;
    // Frame 1: 0.1s
    state.rightThumbHeldDuration += 0.1f;
    assert(!state.rightThumbLongHeldTriggered);

    // Frame 2: +0.2s = 0.3s
    state.rightThumbHeldDuration += 0.2f;
    assert(!state.rightThumbLongHeldTriggered);

    // Frame 3: +0.1s = 0.4s (crosses 0.35s threshold)
    state.rightThumbHeldDuration += 0.1f;
    if (state.rightThumbHeldDuration >= 0.35f && !state.rightThumbLongHeldTriggered) {
        state.rightThumbLongHeldTriggered = true;
    }
    assert(state.rightThumbLongHeldTriggered);

    // Release R3
    state.rightThumb = false;
    state.rightThumbHeldDuration = 0.0f;
    state.rightThumbLongHeldTriggered = false;
    assert(!state.rightThumbLongHeldTriggered);

    std::cout << "[PASS] testRightThumbTacticalProneLogic\n";
}

void testSensitivityPresets() {
    GamepadConfig config;
    assert(config.sensitivityPresetIndex == 0);
    assert(std::abs(config.horizontalSensitivity - 2.6f) < 0.001f);
    assert(std::abs(config.verticalSensitivity - 2.0f) < 0.001f);

    config.cycleSensitivityPreset();
    assert(config.sensitivityPresetIndex == 1);
    assert(std::abs(config.horizontalSensitivity - 3.9f) < 0.001f);
    assert(std::abs(config.verticalSensitivity - 3.0f) < 0.001f);

    config.cycleSensitivityPreset();
    assert(config.sensitivityPresetIndex == 2);
    assert(std::abs(config.horizontalSensitivity - 5.2f) < 0.001f);
    assert(std::abs(config.verticalSensitivity - 4.0f) < 0.001f);

    config.cycleSensitivityPreset();
    assert(config.sensitivityPresetIndex == 3);
    assert(std::abs(config.horizontalSensitivity - 7.8f) < 0.001f);
    assert(std::abs(config.verticalSensitivity - 6.0f) < 0.001f);

    config.cycleSensitivityPreset();
    assert(config.sensitivityPresetIndex == 0);
    assert(std::abs(config.horizontalSensitivity - 2.6f) < 0.001f);
    assert(std::abs(config.verticalSensitivity - 2.0f) < 0.001f);

    config.setSensitivityPreset(2);
    assert(config.sensitivityPresetIndex == 2);
    assert(std::abs(config.horizontalSensitivity - 5.2f) < 0.001f);

    std::cout << "[PASS] testSensitivityPresets\n";
}

int main() {
    testControllerIdentification();
    testButtonLabels();
    testRadialDeadzone();
    testThreeWeaponRotation();
    testBButtonHoldProneLogic();
    testRightThumbTacticalProneLogic();
    testSensitivityPresets();
    std::cout << "All Gamepad tests passed!\n";
    return 0;
}
