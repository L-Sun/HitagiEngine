#pragma once
#include "InputEvent.hpp"

#include <imgui.h>

namespace Hitagi::Gui {

inline VirtualKeyCode convert_imgui_key(ImGuiKey key) {
    switch (key) {
        case ImGuiKey_Tab:
            return VirtualKeyCode::KEY_TAB;
        case ImGuiKey_LeftArrow:
            return VirtualKeyCode::KEY_LEFT;
        case ImGuiKey_RightArrow:
            return VirtualKeyCode::KEY_RIGHT;
        case ImGuiKey_UpArrow:
            return VirtualKeyCode::KEY_UP;
        case ImGuiKey_DownArrow:
            return VirtualKeyCode::KEY_DOWN;
        case ImGuiKey_PageUp:
            return VirtualKeyCode::KEY_PAGE_UP;
        case ImGuiKey_PageDown:
            return VirtualKeyCode::KEY_PAGE_DOWN;
        case ImGuiKey_Home:
            return VirtualKeyCode::KEY_HOME;
        case ImGuiKey_End:
            return VirtualKeyCode::KEY_END;
        case ImGuiKey_Insert:
            return VirtualKeyCode::KEY_INSERT;
        case ImGuiKey_Delete:
            return VirtualKeyCode::KEY_DELETE;
        case ImGuiKey_Backspace:
            return VirtualKeyCode::KEY_BACKSPACE;
        case ImGuiKey_Space:
            return VirtualKeyCode::KEY_SPACE;
        case ImGuiKey_Enter:
            return VirtualKeyCode::KEY_ENTER;
        case ImGuiKey_Escape:
            return VirtualKeyCode::KEY_ESCAPE;
        case ImGuiKey_LeftCtrl:
            return VirtualKeyCode::KEY_L_CTRL;
        case ImGuiKey_LeftShift:
            return VirtualKeyCode::KEY_L_SHIFT;
        case ImGuiKey_LeftAlt:
            return VirtualKeyCode::KEY_L_ALT;
        case ImGuiKey_LeftSuper:
            return VirtualKeyCode::NONE;  //* No support
        case ImGuiKey_RightCtrl:
            return VirtualKeyCode::KEY_R_CTRL;
        case ImGuiKey_RightShift:
            return VirtualKeyCode::KEY_R_SHIFT;
        case ImGuiKey_RightAlt:
            return VirtualKeyCode::KEY_R_ALT;
        case ImGuiKey_RightSuper:
            return VirtualKeyCode::NONE;  //* No support
        case ImGuiKey_Menu:
            return VirtualKeyCode::KEY_L_ALT;
        case ImGuiKey_0:
            return VirtualKeyCode::KEY_0;
        case ImGuiKey_1:
            return VirtualKeyCode::KEY_1;
        case ImGuiKey_2:
            return VirtualKeyCode::KEY_2;
        case ImGuiKey_3:
            return VirtualKeyCode::KEY_3;
        case ImGuiKey_4:
            return VirtualKeyCode::KEY_4;
        case ImGuiKey_5:
            return VirtualKeyCode::KEY_5;
        case ImGuiKey_6:
            return VirtualKeyCode::KEY_6;
        case ImGuiKey_7:
            return VirtualKeyCode::KEY_7;
        case ImGuiKey_8:
            return VirtualKeyCode::KEY_8;
        case ImGuiKey_9:
            return VirtualKeyCode::KEY_9;
        case ImGuiKey_A:
            return VirtualKeyCode::KEY_A;
        case ImGuiKey_B:
            return VirtualKeyCode::KEY_B;
        case ImGuiKey_C:
            return VirtualKeyCode::KEY_C;
        case ImGuiKey_D:
            return VirtualKeyCode::KEY_D;
        case ImGuiKey_E:
            return VirtualKeyCode::KEY_E;
        case ImGuiKey_F:
            return VirtualKeyCode::KEY_F;
        case ImGuiKey_G:
            return VirtualKeyCode::KEY_G;
        case ImGuiKey_H:
            return VirtualKeyCode::KEY_H;
        case ImGuiKey_I:
            return VirtualKeyCode::KEY_I;
        case ImGuiKey_J:
            return VirtualKeyCode::KEY_J;
        case ImGuiKey_K:
            return VirtualKeyCode::KEY_K;
        case ImGuiKey_L:
            return VirtualKeyCode::KEY_L;
        case ImGuiKey_M:
            return VirtualKeyCode::KEY_M;
        case ImGuiKey_N:
            return VirtualKeyCode::KEY_N;
        case ImGuiKey_O:
            return VirtualKeyCode::KEY_O;
        case ImGuiKey_P:
            return VirtualKeyCode::KEY_P;
        case ImGuiKey_Q:
            return VirtualKeyCode::KEY_Q;
        case ImGuiKey_R:
            return VirtualKeyCode::KEY_R;
        case ImGuiKey_S:
            return VirtualKeyCode::KEY_S;
        case ImGuiKey_T:
            return VirtualKeyCode::KEY_T;
        case ImGuiKey_U:
            return VirtualKeyCode::KEY_U;
        case ImGuiKey_V:
            return VirtualKeyCode::KEY_V;
        case ImGuiKey_W:
            return VirtualKeyCode::KEY_W;
        case ImGuiKey_X:
            return VirtualKeyCode::KEY_X;
        case ImGuiKey_Y:
            return VirtualKeyCode::KEY_Y;
        case ImGuiKey_Z:
            return VirtualKeyCode::KEY_Z;
        case ImGuiKey_F1:
            return VirtualKeyCode::KEY_F1;
        case ImGuiKey_F2:
            return VirtualKeyCode::KEY_F2;
        case ImGuiKey_F3:
            return VirtualKeyCode::KEY_F3;
        case ImGuiKey_F4:
            return VirtualKeyCode::KEY_F4;
        case ImGuiKey_F5:
            return VirtualKeyCode::KEY_F5;
        case ImGuiKey_F6:
            return VirtualKeyCode::KEY_F6;
        case ImGuiKey_F7:
            return VirtualKeyCode::KEY_F7;
        case ImGuiKey_F8:
            return VirtualKeyCode::KEY_F8;
        case ImGuiKey_F9:
            return VirtualKeyCode::KEY_F9;
        case ImGuiKey_F10:
            return VirtualKeyCode::KEY_F10;
        case ImGuiKey_F11:
            return VirtualKeyCode::KEY_F11;
        case ImGuiKey_F12:
            return VirtualKeyCode::KEY_F12;
        case ImGuiKey_Apostrophe:
            return VirtualKeyCode::KEY_APOSTROPHE;
        case ImGuiKey_Comma:
            return VirtualKeyCode::KEY_COMMA;
        case ImGuiKey_Minus:
            return VirtualKeyCode::KEY_MINUS;
        case ImGuiKey_Period:
            return VirtualKeyCode::KEY_PERIOD;
        case ImGuiKey_Slash:
            return VirtualKeyCode::KEY_SLASH;
        case ImGuiKey_Semicolon:
            return VirtualKeyCode::KEY_SEMICOLON;
        case ImGuiKey_Equal:
            return VirtualKeyCode::KEY_EQUAL;
        case ImGuiKey_LeftBracket:
            return VirtualKeyCode::KEY_LEFTBRACKET;
        case ImGuiKey_Backslash:
            return VirtualKeyCode::KEY_BACKSLASH;
        case ImGuiKey_RightBracket:
            return VirtualKeyCode::KEY_RIGHTBRACKET;
        case ImGuiKey_GraveAccent:
            return VirtualKeyCode::KEY_GRAVEACCENT;
        case ImGuiKey_CapsLock:
            return VirtualKeyCode::KEY_CAPS_LOCK;
        case ImGuiKey_ScrollLock:
            return VirtualKeyCode::KEY_SCROLL_LOCK;
        case ImGuiKey_NumLock:
            return VirtualKeyCode::KEY_NUM_LOCK;
        case ImGuiKey_PrintScreen:
            return VirtualKeyCode::KEY_SNAPSHOT;
        case ImGuiKey_Pause:
            return VirtualKeyCode::KEY_PAUSE;
        case ImGuiKey_Keypad0:
            return VirtualKeyCode::KEY_NUMPAD_0;
        case ImGuiKey_Keypad1:
            return VirtualKeyCode::KEY_NUMPAD_1;
        case ImGuiKey_Keypad2:
            return VirtualKeyCode::KEY_NUMPAD_2;
        case ImGuiKey_Keypad3:
            return VirtualKeyCode::KEY_NUMPAD_3;
        case ImGuiKey_Keypad4:
            return VirtualKeyCode::KEY_NUMPAD_4;
        case ImGuiKey_Keypad5:
            return VirtualKeyCode::KEY_NUMPAD_5;
        case ImGuiKey_Keypad6:
            return VirtualKeyCode::KEY_NUMPAD_6;
        case ImGuiKey_Keypad7:
            return VirtualKeyCode::KEY_NUMPAD_7;
        case ImGuiKey_Keypad8:
            return VirtualKeyCode::KEY_NUMPAD_8;
        case ImGuiKey_Keypad9:
            return VirtualKeyCode::KEY_NUMPAD_9;
        case ImGuiKey_KeypadDecimal:
            return VirtualKeyCode::KEY_NUMPAD_DECIMAL;
        case ImGuiKey_KeypadDivide:
            return VirtualKeyCode::KEY_NUMPAD_DIVIDE;
        case ImGuiKey_KeypadMultiply:
            return VirtualKeyCode::KEY_NUMPAD_MULTIPLY;
        case ImGuiKey_KeypadSubtract:
            return VirtualKeyCode::KEY_NUMPAD_SUBTRACT;
        case ImGuiKey_KeypadAdd:
            return VirtualKeyCode::KEY_NUMPAD_ADD;
        case ImGuiKey_KeypadEnter:
            return VirtualKeyCode::KEY_ENTER;
        case ImGuiKey_KeypadEqual:
            return VirtualKeyCode::KEY_EQUAL;

        // Gamepad (some of those are analog values, 0.0f to 1.0f)                              // NAVIGATION action
        case ImGuiKey_GamepadStart:        // Menu (Xbox)          + (Switch)   Start/Options (PS) // --
        case ImGuiKey_GamepadBack:         // View (Xbox)          - (Switch)   Share (PS)         // --
        case ImGuiKey_GamepadFaceUp:       // Y (Xbox)             X (Switch)   Triangle (PS)      // -> ImGuiNavInput_Input
        case ImGuiKey_GamepadFaceDown:     // A (Xbox)             B (Switch)   Cross (PS)         // -> ImGuiNavInput_Activate
        case ImGuiKey_GamepadFaceLeft:     // X (Xbox)             Y (Switch)   Square (PS)        // -> ImGuiNavInput_Menu
        case ImGuiKey_GamepadFaceRight:    // B (Xbox)             A (Switch)   Circle (PS)        // -> ImGuiNavInput_Cancel
        case ImGuiKey_GamepadDpadUp:       // D-pad Up                                             // -> ImGuiNavInput_DpadUp
        case ImGuiKey_GamepadDpadDown:     // D-pad Down                                           // -> ImGuiNavInput_DpadDown
        case ImGuiKey_GamepadDpadLeft:     // D-pad Left                                           // -> ImGuiNavInput_DpadLeft
        case ImGuiKey_GamepadDpadRight:    // D-pad Right                                          // -> ImGuiNavInput_DpadRight
        case ImGuiKey_GamepadL1:           // L Bumper (Xbox)      L (Switch)   L1 (PS)            // -> ImGuiNavInput_FocusPrev + ImGuiNavInput_TweakSlow
        case ImGuiKey_GamepadR1:           // R Bumper (Xbox)      R (Switch)   R1 (PS)            // -> ImGuiNavInput_FocusNext + ImGuiNavInput_TweakFast
        case ImGuiKey_GamepadL2:           // L Trigger (Xbox)     ZL (Switch)  L2 (PS) [Analog]
        case ImGuiKey_GamepadR2:           // R Trigger (Xbox)     ZR (Switch)  R2 (PS) [Analog]
        case ImGuiKey_GamepadL3:           // L Thumbstick (Xbox)  L3 (Switch)  L3 (PS)
        case ImGuiKey_GamepadR3:           // R Thumbstick (Xbox)  R3 (Switch)  R3 (PS)
        case ImGuiKey_GamepadLStickUp:     // [Analog]                                             // -> ImGuiNavInput_LStickUp
        case ImGuiKey_GamepadLStickDown:   // [Analog]                                             // -> ImGuiNavInput_LStickDown
        case ImGuiKey_GamepadLStickLeft:   // [Analog]                                             // -> ImGuiNavInput_LStickLeft
        case ImGuiKey_GamepadLStickRight:  // [Analog]                                             // -> ImGuiNavInput_LStickRight
        case ImGuiKey_GamepadRStickUp:     // [Analog]
        case ImGuiKey_GamepadRStickDown:   // [Analog]
        case ImGuiKey_GamepadRStickLeft:   // [Analog]
        case ImGuiKey_GamepadRStickRight:  // [Analog]

        // Keyboard Modifiers
        // - This is mirroring the data also written to io.KeyCtrl, io.KeyShift, io.KeyAlt, io.KeySuper, in a format allowing
        //   them to be accessed via standard key API, allowing calls such as IsKeyPressed(), IsKeyReleased(), querying duration etc.
        // - Code polling every keys (e.g. an interface to detect a key press for input mapping) might want to ignore those
        //   and prefer using the real keys (e.g. ImGuiKey_LeftCtrl, ImGuiKey_RightCtrl instead of ImGuiKey_ModCtrl).
        // - In theory the value of keyboard modifiers should be roughly equivalent to a logical or of the equivalent left/right keys.
        //   In practice: it's complicated、 mods are often provided from different sources. Keyboard layout, IME, sticky keys and
        //   backends tend to interfere and break that equivalence. The safer decision is to relay that ambiguity down to the end-user...
        case ImGuiKey_ModCtrl:
        case ImGuiKey_ModShift:
        case ImGuiKey_ModAlt:
        case ImGuiKey_ModSuper:
        default:
            return VirtualKeyCode::NONE;
    }
}

}  // namespace Hitagi::Gui
