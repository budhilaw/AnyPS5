#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "SDL.h"
#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libSceVideoOut/include/MainThread.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/PadInputTypes.hpp"
#include <cstdlib>
#include <vector>
#include <tuple>
#include <array>

namespace {

struct ScriptedInput { std::uint32_t buttons = 0; std::array<std::uint8_t, 4> sticks{128, 128, 128, 128}; };

ScriptedInput ScriptedControls(std::chrono::steady_clock::time_point now) {
    struct Press { double at; double duration; std::uint32_t button; int axis; std::uint8_t axisValue; };
    static const std::vector<Press> presses = [] {
        std::vector<Press> result;
        const char* text = std::getenv("ANYPS5_DEBUG_PRESS");
        if (text == nullptr) return result;
        const std::vector<std::pair<std::string, Pad::PadButton>> names = {
            {"cross", Pad::PadButton::Cross}, {"circle", Pad::PadButton::Circle}, {"triangle", Pad::PadButton::Triangle}, {"square", Pad::PadButton::Square},
            {"options", Pad::PadButton::Options}, {"up", Pad::PadButton::Up}, {"down", Pad::PadButton::Down},
            {"left", Pad::PadButton::Left}, {"right", Pad::PadButton::Right}, {"l1", Pad::PadButton::L1}, {"r1", Pad::PadButton::R1}};
        const std::vector<std::tuple<std::string, int, std::uint8_t>> axes = {
            {"lsleft", 0, 0}, {"lsright", 0, 255}, {"lsup", 1, 0}, {"lsdown", 1, 255}, {"rsleft", 2, 0}, {"rsright", 2, 255}, {"rsup", 3, 0}, {"rsdown", 3, 255}};
        std::string entry;
        for (const char* cursor = text;; ++cursor) {
            if (*cursor != ',' && *cursor != '\0') { entry += *cursor; continue; }
            const auto colon = entry.find(':');
            if (colon != std::string::npos) {
                auto name = entry.substr(colon + 1);
                double duration = 0.3;
                if (const auto second = name.find(':'); second != std::string::npos) { duration = std::atof(name.c_str() + second + 1); name = name.substr(0, second); }
                for (const auto& [candidate, button] : names) if (candidate == name) result.push_back({std::atof(entry.c_str()), duration, static_cast<std::uint32_t>(button), -1, 128});
                for (const auto& [candidate, axis, value] : axes) if (candidate == name) result.push_back({std::atof(entry.c_str()), duration, 0, axis, value});
            }
            entry.clear();
            if (*cursor == '\0') break;
        }
        return result;
    }();
    ScriptedInput input;
    if (presses.empty()) return input;
    static const auto start = now;
    const double elapsed = std::chrono::duration<double>(now - start).count();
    for (const auto& press : presses) {
        if (elapsed < press.at || elapsed >= press.at + press.duration) continue;
        if (press.axis < 0) input.buttons |= press.button;
        else input.sticks[static_cast<std::size_t>(press.axis)] = press.axisValue;
    }
    return input;
}

}

void PadInput::setMouseMode(bool enabled) {
    MainThread::Run([&] {
        if (SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE) != 0) throw std::runtime_error(std::string("Pad: relative mouse mode failed: ") + SDL_GetError());
    });
    int deltaX = 0;
    int deltaY = 0;
    SDL_GetRelativeMouseState(&deltaX, &deltaY);
    mouseEnabled = enabled;
    mouseStick = {128, 128};
    nextMousePoll = std::chrono::steady_clock::now() + std::chrono::milliseconds(Pad::MousePollIntervalMs);
}

void PadInput::HandleEvent(const SDL_Event& event, DisplayWindow& window) {
    if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST || event.window.event == SDL_WINDOWEVENT_CLOSE)) {
        pressed.fill(false);
        wheelReleaseTimes.fill({});
        if (mouseEnabled) setMouseMode(false);
        publish();
        return;
    }
    if (event.type == SDL_MOUSEWHEEL) {
        int direction = (event.wheel.y > 0) - (event.wheel.y < 0);
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) direction = -direction;
        if (direction == 0) return;
        const auto releaseTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(Pad::WheelPressDurationMs);
        for (std::size_t index = 0; index < Pad::InputMapping.size(); ++index) {
            const auto& binding = Pad::InputMapping[index];
            if (binding.wheelDirection == 0) continue;
            pressed[index] = binding.wheelDirection == direction;
            wheelReleaseTimes[index] = pressed[index] ? releaseTime : std::chrono::steady_clock::time_point{};
        }
        publish();
        return;
    }
    const bool keyboard = event.type == SDL_KEYDOWN || event.type == SDL_KEYUP;
    const bool mouse = event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP;
    if (!keyboard && !mouse) return;
    if (keyboard && event.key.repeat != 0) return;
    const bool down = event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN;
    for (std::size_t index = 0; index < Pad::InputMapping.size(); ++index) {
        const auto& binding = Pad::InputMapping[index];
        const bool keyMatches = binding.key != SDL_SCANCODE_UNKNOWN && binding.key == event.key.keysym.scancode;
        const bool mouseMatches = binding.mouseButton != Pad::MouseButton::None && binding.mouseButton == static_cast<Pad::MouseButton>(event.button.button);
        const bool matches = keyboard ? keyMatches : mouseMatches;

        if (!matches) continue;
        if (binding.control == Pad::InputControl::ToggleFullscreen) {
            if (keyboard && down && !pressed[index] && window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) window.ToggleFullscreen();
        }
        if (binding.control == Pad::InputControl::ToggleMouse && down && !pressed[index]) setMouseMode(!mouseEnabled);
        pressed[index] = down;
    }
    publish();
}

void PadInput::Update() {
    const auto now = std::chrono::steady_clock::now();
    if (const auto controls = ScriptedControls(now); controls.buttons != scripted || controls.sticks != scriptedSticks) {
        scripted = controls.buttons;
        scriptedSticks = controls.sticks;
        publish();
    }
    bool released = false;
    for (std::size_t index = 0; index < Pad::InputMapping.size(); ++index) {
        if (Pad::InputMapping[index].wheelDirection == 0 || !pressed[index] || now < wheelReleaseTimes[index]) continue;
        pressed[index] = false;
        wheelReleaseTimes[index] = {};
        released = true;
    }
    if (released) publish();
    if (!mouseEnabled) return;
    if (SDL_GetKeyboardFocus() == nullptr) {
        pressed.fill(false);
        wheelReleaseTimes.fill({});
        setMouseMode(false);
        publish();
        return;
    }
    if (now < nextMousePoll) return;
    nextMousePoll = now + std::chrono::milliseconds(Pad::MousePollIntervalMs);
    int deltaX = 0;
    int deltaY = 0;
    SDL_GetRelativeMouseState(&deltaX, &deltaY);
    mouseStick = {128, 128};
    if (deltaX != 0 || deltaY != 0) {
        const double distance = std::hypot(deltaX, deltaY);
        const double scale = std::clamp(distance * Pad::MouseSensitivity + 16.0, 64.0, 128.0) / distance;
        const auto mapAxis = [scale](int delta) { return static_cast<std::uint8_t>(std::clamp(128L + std::lround(delta * scale), 0L, 255L)); };
        mouseStick = {mapAxis(deltaX), mapAxis(deltaY)};
    }
    publish();
}

void PadInput::publish() {
    PadInputState state;
    std::array<bool, 4> negative{};
    std::array<bool, 4> positive{};
    for (std::size_t index = 0; index < Pad::InputMapping.size(); ++index) {
        if (!pressed[index]) continue;
        const auto& binding = Pad::InputMapping[index];
        switch (binding.control) {
            case Pad::InputControl::Button: state.buttons |= static_cast<std::uint32_t>(binding.button); break;
            case Pad::InputControl::LeftStickLeft: negative[0] = true; break;
            case Pad::InputControl::LeftStickRight: positive[0] = true; break;
            case Pad::InputControl::LeftStickUp: negative[1] = true; break;
            case Pad::InputControl::LeftStickDown: positive[1] = true; break;
            case Pad::InputControl::RightStickLeft: negative[2] = true; break;
            case Pad::InputControl::RightStickRight: positive[2] = true; break;
            case Pad::InputControl::RightStickUp: negative[3] = true; break;
            case Pad::InputControl::RightStickDown: positive[3] = true; break;
            case Pad::InputControl::TouchLeft: state.touchLeft = true; break;
            case Pad::InputControl::TouchRight: state.touchRight = true; break;
            case Pad::InputControl::ToggleMouse: break;
            case Pad::InputControl::ToggleFullscreen: break;
        }
    }
    for (std::size_t axis = 0; axis < state.sticks.size(); ++axis) {
        state.sticks[axis] = negative[axis] == positive[axis] ? 128 : negative[axis] ? 0 : 255;
    }
    if (mouseEnabled) {
        state.sticks[2] = mouseStick[0];
        state.sticks[3] = mouseStick[1];
    }
    state.buttons |= scripted;
    for (std::size_t axis = 0; axis < state.sticks.size(); ++axis) if (scriptedSticks[axis] != 128) state.sticks[axis] = scriptedSticks[axis];
    PadPublishInput_nid_postfix(state);
}
