#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <array>

namespace Warp
{

    // EKeycode only represents a key on a general-case keyboard. Some keyboards might not have physical representation of these keys.
    // ! -> Key note here is that not all keycodes represent same characters. 
    // ! For example, eKeycode_A could represent either 'a' or 'A' or 'á' (if the keyboard supports combining diacritics). This depends on other factors, such as
    // ! If the ALT key is held down, pressing A key produces Alt + A which the system does not treat as a character at all, but rather as a system command.
    enum EKeycode : uint32_t
    {
        eKeycode_Invalid = 0,
        // F1 - F12 (we do not support F13 - F24)
        eKeycode_F1, eKeycode_F2, eKeycode_F3, eKeycode_F4, eKeycode_F5, eKeycode_F6,
        eKeycode_F7, eKeycode_F8, eKeycode_F9, eKeycode_F10, eKeycode_F11, eKeycode_F12,
        // Keybord nums 1 - 9 and 0
        eKeycode_1, eKeycode_2, eKeycode_3, eKeycode_4, eKeycode_5, eKeycode_6, eKeycode_7, eKeycode_8, eKeycode_9, eKeycode_0,
        eKeycode_A, eKeycode_B, eKeycode_C, eKeycode_D, eKeycode_E, eKeycode_F, eKeycode_G, eKeycode_H,
        eKeycode_I, eKeycode_J, eKeycode_K, eKeycode_L, eKeycode_M, eKeycode_N, eKeycode_O, eKeycode_P,
        eKeycode_Q, eKeycode_R, eKeycode_S, eKeycode_T, eKeycode_U, eKeycode_V, eKeycode_W, eKeycode_X,
        eKeycode_Y, eKeycode_Z,
        // Reserved to always be last
        eKeycode_NumKeycodes,
    };

    enum EKeycodeState : uint8_t
    {
        eKeycodeState_Released = 0,
        eKeycodeState_Pressed,
    };

    // SITREP (As of 17.02.2024)
    // For all of the WinAPI inputs dogma we chill here - https://learn.microsoft.com/en-us/windows/win32/learnwin32/keyboard-input
    // The basic idea of the system is to fulfill three distinct types of input, that are:
    // - Character input. Text that the user types into a document or edit box.
    // - Keyboard shortcuts (Ctrl + O to open a file, etc.)
    // - System commands (Alt + Tab, Win + Tab, Win + D, etc.)
    class KeyboardDevice
    {
    public:
        // Represents a "key changed" or "key interacted" event
        struct EvKeyInteraction
        {
            EKeycode Keycode;
            EKeycodeState PrevState;
            EKeycodeState NextState;
        };

        using KeyInteractionEventDelegate = std::function<void(const EvKeyInteraction&)>;

        KeyboardDevice() = default;

        KeyboardDevice(const KeyboardDevice&) = delete;
        KeyboardDevice& operator=(const KeyboardDevice&) = delete;

        KeyboardDevice(KeyboardDevice&&) = delete;
        KeyboardDevice& operator=(KeyboardDevice&&) = delete;

        void AddKeyInteractionDelegate(const KeyInteractionEventDelegate& delegate);
        void SetKeycodeState(EKeycode keycode, EKeycodeState state);
        EKeycodeState GetKeycodeState(EKeycode keycode) const { return m_keycodeStates[keycode]; }

    private:
        std::vector<KeyInteractionEventDelegate> m_keyInteractionDelegates;
        std::array<EKeycodeState, eKeycode_NumKeycodes> m_keycodeStates;
    };

}