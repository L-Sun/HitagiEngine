#pragma once
#include <iostream>
#include <functional>
#include <array>

#include "IRuntimeModule.hpp"

namespace Hitagi {
enum class InputEvent : unsigned {
    KEY_R,
    KEY_UP,
    KEY_DOWN,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_W,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_Z,
    KEY_X,
    KEY_M,
    KEY_SPACE,

    MOUSE_RIGHT,
    MOUSE_LEFT,
    MOUSE_MIDDLE,
    MOUSE_MOVE_X,
    MOUSE_MOVE_Y,
    MOUSE_SCROLL_X,
    MOUSE_SCROLL_Y,
    NUM_EVENT,
};

class InputManager : public IRuntimeModule {
    friend class GLFWApplication;

public:
    using UserDefAction = int;
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void Map(UserDefAction userAction, InputEvent key);

    bool  GetBool(UserDefAction userAction) const;
    bool  GetBoolNew(UserDefAction userAction) const;
    float GetFloat(UserDefAction userAction) const;
    float GetFloatDelta(UserDefAction userAction) const;

private:
    struct BoolMap {
        bool current = false, previous = false;
    };
    struct FloatMap {
        float current = 0, previous = 0;
    };

    std::array<BoolMap, static_cast<unsigned>(InputEvent::NUM_EVENT)>  m_BoolMapping;
    std::array<FloatMap, static_cast<unsigned>(InputEvent::NUM_EVENT)> m_FloatMapping;
    std::vector<InputEvent>                                            m_UserMapping;
};

extern std::unique_ptr<InputManager> g_InputManager;
}  // namespace Hitagi