#pragma once
#include <hitagi/math/vector.hpp>

#include <array>

namespace hitagi {

template <typename T>
struct DoubleState {
    T    current, previous;
    bool dirty;

    inline void Update(T new_value) noexcept {
        previous = std::move(current);
        current  = std::move(new_value);
        dirty    = true;
    }

    inline void ClearDirty() noexcept {
        if (dirty)
            dirty = false;
        else
            previous = current;
    }
};

using KeyState = DoubleState<bool>;
struct MouseState {
    DoubleState<math::vec2f> position;
    DoubleState<math::vec2f> scroll;
};

enum class MouseEvent {
    MOVE_X,
    MOVE_Y,
    SCROLL_Y,
    SCROLL_X,
};

enum class VirtualKeyCode {
    NONE           = 0x0,
    MOUSE_L_BUTTON = 0x01,
    MOUSE_R_BUTTON = 0x02,
    // KEY_CANCEL =0x03  //	Control-break processing
    MOUSE_M_BUTTON = 0x04,
    KEY_BACKSPACE  = 0x08,
    KEY_TAB        = 0x09,
    // ...
    KEY_CLEAR = 0x0C,
    KEY_ENTER = 0x0D,
    // ...
    KEY_SHIFT     = 0x10,
    KEY_CTRL      = 0x11,
    KEY_ALT       = 0x12,
    KEY_PAUSE     = 0x13,
    KEY_CAPS_LOCK = 0x14,
    // ...
    KEY_ESCAPE = 0x1B,
    // ...
    KEY_SPACE = 0x20,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_END,
    KEY_HOME,
    KEY_LEFT,
    KEY_UP,
    KEY_RIGHT,
    KEY_DOWN,
    KEY_SELECT,
    KEY_PRINT,
    KEY_EXE,
    KEY_SNAPSHOT,
    KEY_INSERT,
    KEY_DELETE,
    KEY_HELP,

    KEY_0 = 0x30,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    KEY_A = 0x41,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,

    KEY_NUMPAD_0 = 0x60,
    KEY_NUMPAD_1,
    KEY_NUMPAD_2,
    KEY_NUMPAD_3,
    KEY_NUMPAD_4,
    KEY_NUMPAD_5,
    KEY_NUMPAD_6,
    KEY_NUMPAD_7,
    KEY_NUMPAD_8,
    KEY_NUMPAD_9,
    KEY_NUMPAD_MULTIPLY,
    KEY_NUMPAD_ADD,
    KEY_NUMPAD_SEPARA,
    KEY_NUMPAD_SUBTRACT,
    KEY_NUMPAD_DECIMAL,
    KEY_NUMPAD_DIVIDE,

    KEY_F1 = 0x70,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_F13,
    KEY_F14,
    KEY_F15,
    KEY_F16,
    KEY_F17,
    KEY_F18,
    KEY_F19,
    KEY_F20,
    KEY_F21,
    KEY_F22,
    KEY_F23,
    KEY_F24,

    KEY_NUM_LOCK    = 0x90,
    KEY_SCROLL_LOCK = 0x91,

    KEY_L_SHIFT = 0xA0,
    KEY_R_SHIFT,
    KEY_L_CTRL,
    KEY_R_CTRL,
    KEY_L_ALT,
    KEY_R_ALT,

    // ...
    KEY_SEMICOLON = 0xBA,    // ;:
    KEY_EQUAL,               // =+
    KEY_COMMA,               // ,<
    KEY_MINUS,               // -_
    KEY_PERIOD,              // .>
    KEY_SLASH,               // /?
    KEY_GRAVEACCENT,         // `~
    KEY_LEFTBRACKET = 0XDB,  // [{
    KEY_BACKSLASH,           // \|
    KEY_RIGHTBRACKET,        // ]}
    KEY_APOSTROPHE,          // '"
    NUM
};

}  // namespace hitagi
