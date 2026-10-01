#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADINPUT_HPP

#include "SDL_events.h"
#include "SDL_gamecontroller.h"
#include "prx/libScePad/include/InputMapping.hpp"
#include <array>
#include <chrono>

class DisplayWindow;

class PadInput {
public:
    void HandleEvent(const SDL_Event& event, DisplayWindow& window);
    void Update();

private:
    void publish();
    void setMouseMode(bool enabled);
    void openController(int deviceIndex);
    void closeController();
    void readController();
    std::array<bool, Pad::InputMapping.size()> pressed{};
    std::array<std::chrono::steady_clock::time_point, Pad::InputMapping.size()> wheelReleaseTimes{};
    std::array<std::uint8_t, 2> mouseStick{128, 128};
    std::chrono::steady_clock::time_point nextMousePoll{};
    bool mouseEnabled = false;
    std::uint32_t scripted = 0;
    std::array<std::uint8_t, 4> scriptedSticks{128, 128, 128, 128};
    SDL_GameController* controller = nullptr;
    SDL_JoystickID controllerId = -1;
    std::uint32_t controllerButtons = 0;
    std::array<std::uint8_t, 4> controllerSticks{128, 128, 128, 128};
    std::uint8_t controllerL2 = 0;
    std::uint8_t controllerR2 = 0;
};

#endif
