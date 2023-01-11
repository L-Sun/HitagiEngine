#pragma once
#include <hitagi/hid/input_event.hpp>

#include <imgui.h>

namespace hitagi::gui {

inline constexpr auto convert_imgui_key(ImGuiKey key) -> hid::VirtualKeyCode {
    switch (key) {
        case ImGuiKey_Tab:
            return hid::VirtualKeyCode::KEY_TAB;
        case ImGuiKey_LeftArrow:
            return hid::VirtualKeyCode::KEY_LEFT;
        case ImGuiKey_RightArrow:
            return hid::VirtualKeyCode::KEY_RIGHT;
        case ImGuiKey_UpArrow:
            return hid::VirtualKeyCode::KEY_UP;
        case ImGuiKey_DownArrow:
            return hid::VirtualKeyCode::KEY_DOWN;
        case ImGuiKey_PageUp:
            return hid::VirtualKeyCode::KEY_PAGE_UP;
        case ImGuiKey_PageDown:
            return hid::VirtualKeyCode::KEY_PAGE_DOWN;
        case ImGuiKey_Home:
            return hid::VirtualKeyCode::KEY_HOME;
        case ImGuiKey_End:
            return hid::VirtualKeyCode::KEY_END;
        case ImGuiKey_Insert:
            return hid::VirtualKeyCode::KEY_INSERT;
        case ImGuiKey_Delete:
            return hid::VirtualKeyCode::KEY_DELETE;
        case ImGuiKey_Backspace:
            return hid::VirtualKeyCode::KEY_BACKSPACE;
        case ImGuiKey_Space:
            return hid::VirtualKeyCode::KEY_SPACE;
        case ImGuiKey_Enter:
            return hid::VirtualKeyCode::KEY_ENTER;
        case ImGuiKey_Escape:
            return hid::VirtualKeyCode::KEY_ESCAPE;
        case ImGuiKey_LeftCtrl:
            return hid::VirtualKeyCode::KEY_L_CTRL;
        case ImGuiKey_LeftShift:
            return hid::VirtualKeyCode::KEY_L_SHIFT;
        case ImGuiKey_LeftAlt:
            return hid::VirtualKeyCode::KEY_L_ALT;
        case ImGuiKey_LeftSuper:
            return hid::VirtualKeyCode::NONE;  //* No support
        case ImGuiKey_RightCtrl:
            return hid::VirtualKeyCode::KEY_R_CTRL;
        case ImGuiKey_RightShift:
            return hid::VirtualKeyCode::KEY_R_SHIFT;
        case ImGuiKey_RightAlt:
            return hid::VirtualKeyCode::KEY_R_ALT;
        case ImGuiKey_RightSuper:
            return hid::VirtualKeyCode::NONE;  //* No support
        case ImGuiKey_Menu:
            return hid::VirtualKeyCode::KEY_L_ALT;
        case ImGuiKey_0:
            return hid::VirtualKeyCode::KEY_0;
        case ImGuiKey_1:
            return hid::VirtualKeyCode::KEY_1;
        case ImGuiKey_2:
            return hid::VirtualKeyCode::KEY_2;
        case ImGuiKey_3:
            return hid::VirtualKeyCode::KEY_3;
        case ImGuiKey_4:
            return hid::VirtualKeyCode::KEY_4;
        case ImGuiKey_5:
            return hid::VirtualKeyCode::KEY_5;
        case ImGuiKey_6:
            return hid::VirtualKeyCode::KEY_6;
        case ImGuiKey_7:
            return hid::VirtualKeyCode::KEY_7;
        case ImGuiKey_8:
            return hid::VirtualKeyCode::KEY_8;
        case ImGuiKey_9:
            return hid::VirtualKeyCode::KEY_9;
        case ImGuiKey_A:
            return hid::VirtualKeyCode::KEY_A;
        case ImGuiKey_B:
            return hid::VirtualKeyCode::KEY_B;
        case ImGuiKey_C:
            return hid::VirtualKeyCode::KEY_C;
        case ImGuiKey_D:
            return hid::VirtualKeyCode::KEY_D;
        case ImGuiKey_E:
            return hid::VirtualKeyCode::KEY_E;
        case ImGuiKey_F:
            return hid::VirtualKeyCode::KEY_F;
        case ImGuiKey_G:
            return hid::VirtualKeyCode::KEY_G;
        case ImGuiKey_H:
            return hid::VirtualKeyCode::KEY_H;
        case ImGuiKey_I:
            return hid::VirtualKeyCode::KEY_I;
        case ImGuiKey_J:
            return hid::VirtualKeyCode::KEY_J;
        case ImGuiKey_K:
            return hid::VirtualKeyCode::KEY_K;
        case ImGuiKey_L:
            return hid::VirtualKeyCode::KEY_L;
        case ImGuiKey_M:
            return hid::VirtualKeyCode::KEY_M;
        case ImGuiKey_N:
            return hid::VirtualKeyCode::KEY_N;
        case ImGuiKey_O:
            return hid::VirtualKeyCode::KEY_O;
        case ImGuiKey_P:
            return hid::VirtualKeyCode::KEY_P;
        case ImGuiKey_Q:
            return hid::VirtualKeyCode::KEY_Q;
        case ImGuiKey_R:
            return hid::VirtualKeyCode::KEY_R;
        case ImGuiKey_S:
            return hid::VirtualKeyCode::KEY_S;
        case ImGuiKey_T:
            return hid::VirtualKeyCode::KEY_T;
        case ImGuiKey_U:
            return hid::VirtualKeyCode::KEY_U;
        case ImGuiKey_V:
            return hid::VirtualKeyCode::KEY_V;
        case ImGuiKey_W:
            return hid::VirtualKeyCode::KEY_W;
        case ImGuiKey_X:
            return hid::VirtualKeyCode::KEY_X;
        case ImGuiKey_Y:
            return hid::VirtualKeyCode::KEY_Y;
        case ImGuiKey_Z:
            return hid::VirtualKeyCode::KEY_Z;
        case ImGuiKey_F1:
            return hid::VirtualKeyCode::KEY_F1;
        case ImGuiKey_F2:
            return hid::VirtualKeyCode::KEY_F2;
        case ImGuiKey_F3:
            return hid::VirtualKeyCode::KEY_F3;
        case ImGuiKey_F4:
            return hid::VirtualKeyCode::KEY_F4;
        case ImGuiKey_F5:
            return hid::VirtualKeyCode::KEY_F5;
        case ImGuiKey_F6:
            return hid::VirtualKeyCode::KEY_F6;
        case ImGuiKey_F7:
            return hid::VirtualKeyCode::KEY_F7;
        case ImGuiKey_F8:
            return hid::VirtualKeyCode::KEY_F8;
        case ImGuiKey_F9:
            return hid::VirtualKeyCode::KEY_F9;
        case ImGuiKey_F10:
            return hid::VirtualKeyCode::KEY_F10;
        case ImGuiKey_F11:
            return hid::VirtualKeyCode::KEY_F11;
        case ImGuiKey_F12:
            return hid::VirtualKeyCode::KEY_F12;
        case ImGuiKey_Apostrophe:
            return hid::VirtualKeyCode::KEY_APOSTROPHE;
        case ImGuiKey_Comma:
            return hid::VirtualKeyCode::KEY_COMMA;
        case ImGuiKey_Minus:
            return hid::VirtualKeyCode::KEY_MINUS;
        case ImGuiKey_Period:
            return hid::VirtualKeyCode::KEY_PERIOD;
        case ImGuiKey_Slash:
            return hid::VirtualKeyCode::KEY_SLASH;
        case ImGuiKey_Semicolon:
            return hid::VirtualKeyCode::KEY_SEMICOLON;
        case ImGuiKey_Equal:
            return hid::VirtualKeyCode::KEY_EQUAL;
        case ImGuiKey_LeftBracket:
            return hid::VirtualKeyCode::KEY_LEFTBRACKET;
        case ImGuiKey_Backslash:
            return hid::VirtualKeyCode::KEY_BACKSLASH;
        case ImGuiKey_RightBracket:
            return hid::VirtualKeyCode::KEY_RIGHTBRACKET;
        case ImGuiKey_GraveAccent:
            return hid::VirtualKeyCode::KEY_GRAVEACCENT;
        case ImGuiKey_CapsLock:
            return hid::VirtualKeyCode::KEY_CAPS_LOCK;
        case ImGuiKey_ScrollLock:
            return hid::VirtualKeyCode::KEY_SCROLL_LOCK;
        case ImGuiKey_NumLock:
            return hid::VirtualKeyCode::KEY_NUM_LOCK;
        case ImGuiKey_PrintScreen:
            return hid::VirtualKeyCode::KEY_SNAPSHOT;
        case ImGuiKey_Pause:
            return hid::VirtualKeyCode::KEY_PAUSE;
        case ImGuiKey_Keypad0:
            return hid::VirtualKeyCode::KEY_NUMPAD_0;
        case ImGuiKey_Keypad1:
            return hid::VirtualKeyCode::KEY_NUMPAD_1;
        case ImGuiKey_Keypad2:
            return hid::VirtualKeyCode::KEY_NUMPAD_2;
        case ImGuiKey_Keypad3:
            return hid::VirtualKeyCode::KEY_NUMPAD_3;
        case ImGuiKey_Keypad4:
            return hid::VirtualKeyCode::KEY_NUMPAD_4;
        case ImGuiKey_Keypad5:
            return hid::VirtualKeyCode::KEY_NUMPAD_5;
        case ImGuiKey_Keypad6:
            return hid::VirtualKeyCode::KEY_NUMPAD_6;
        case ImGuiKey_Keypad7:
            return hid::VirtualKeyCode::KEY_NUMPAD_7;
        case ImGuiKey_Keypad8:
            return hid::VirtualKeyCode::KEY_NUMPAD_8;
        case ImGuiKey_Keypad9:
            return hid::VirtualKeyCode::KEY_NUMPAD_9;
        case ImGuiKey_KeypadDecimal:
            return hid::VirtualKeyCode::KEY_NUMPAD_DECIMAL;
        case ImGuiKey_KeypadDivide:
            return hid::VirtualKeyCode::KEY_NUMPAD_DIVIDE;
        case ImGuiKey_KeypadMultiply:
            return hid::VirtualKeyCode::KEY_NUMPAD_MULTIPLY;
        case ImGuiKey_KeypadSubtract:
            return hid::VirtualKeyCode::KEY_NUMPAD_SUBTRACT;
        case ImGuiKey_KeypadAdd:
            return hid::VirtualKeyCode::KEY_NUMPAD_ADD;
        case ImGuiKey_KeypadEnter:
            return hid::VirtualKeyCode::KEY_ENTER;
        case ImGuiKey_KeypadEqual:
            return hid::VirtualKeyCode::KEY_EQUAL;

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
            return hid::VirtualKeyCode::NONE;
    }
}

}  // namespace hitagi::gui
