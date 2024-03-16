#pragma once

#include "KeyboardDevice.h"
#include "MouseDevice.h"

namespace Warp
{

    class InputDeviceManager
    {
    private:
        InputDeviceManager() = default;

    public:
        InputDeviceManager(const InputDeviceManager&) = delete;
        InputDeviceManager& operator=(const InputDeviceManager&) = delete;

        InputDeviceManager(InputDeviceManager&&) = delete;
        InputDeviceManager& operator=(InputDeviceManager&&) = delete;

        static InputDeviceManager& Get();

        KeyboardDevice& GetKeyboard() { return m_keyboard; }
        const KeyboardDevice& GetKeyboard() const { return m_keyboard; }

        MouseDevice& GetMouse() { return m_mouse; }
        const MouseDevice& GetMouse() const { return m_mouse; }

        bool IsKeyPressed(EKeycode keycode) const { return GetKeyboard().GetKeycodeState(keycode) == eKeycodeState_Pressed; }
        bool IsKeyReleased(EKeycode keycode) const { return GetKeyboard().GetKeycodeState(keycode) == eKeycodeState_Released; }

        bool IsMouseButtonClicked(EMouseButton button) const { return GetMouse().GetMouseButtonStates(button) & eMouseButtonState_Clicked; }
        bool IsMouseButtonClickedOnce(EMouseButton button) const { return GetMouse().GetMouseButtonStates(button) & eMouseButtonState_ClickedOnce; }
        bool IsMouseButtonClickedTwice(EMouseButton button) const { return GetMouse().GetMouseButtonStates(button) & eMouseButtonState_ClickedTwice; }

        const MouseDevice::CursorHotspot& GetCursorHotspot() const { return GetMouse().GetCursorHotspot(); }

    private:
        KeyboardDevice m_keyboard;
        MouseDevice m_mouse;
    };

}