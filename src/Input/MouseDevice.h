#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <array>

namespace Warp
{

    // For future mouse implementation - https://learn.microsoft.com/en-us/windows/win32/inputdev/mouse-input

    enum EMouseButton : uint16_t
    {
        eMouseButton_Invalid = 0,
        eMouseButton_Left,
        eMouseButton_Right,
        eMouseButton_Middle,
        eMouseButton_NumButtons,
    };

    using EMouseButtonStates = uint8_t;
    enum  EMouseButtonState : uint8_t
    {
        eMouseButtonState_Released = 0,
        eMouseButtonState_ClickedOnce = 1,
        eMouseButtonState_ClickedTwice = 2,
        eMouseButtonState_Clicked = eMouseButtonState_ClickedOnce | eMouseButtonState_ClickedTwice,
    };

    class MouseDevice
    {
    public:
        // The mouse cursor contains a single-pixel point called the hot spot, 
        // a point that the system tracks and recognizes as the position of the cursor
        struct CursorHotspot
        {
            int32_t PosX;
            int32_t PosY;
        };

        struct EvMouseButtonInteraction
        {
            EMouseButton Button;
            EMouseButtonStates PrevStates;
            EMouseButtonStates NextStates;
            CursorHotspot InteractionHotspot;
        };

        using MouseButtonInteractionEventDelegate = std::function<void(const EvMouseButtonInteraction&)>;

        MouseDevice() = default;

        MouseDevice(const MouseDevice&) = delete;
        MouseDevice& operator=(const MouseDevice&) = delete;

        MouseDevice(MouseDevice&&) = delete;
        MouseDevice& operator=(MouseDevice&&) = delete;

        void SetCursorHotspot(const CursorHotspot& hotspot) { m_cursorHotspot = hotspot; }
        const CursorHotspot& GetCursorHotspot() const { return m_cursorHotspot; }

        void AddMouseButtonInteractionDelegate(const MouseButtonInteractionEventDelegate& delegate);
        void SetMouseButtonStates(EMouseButton button, EMouseButtonStates states);
        EMouseButtonStates GetMouseButtonStates(EMouseButton button) const;

    private:
        CursorHotspot m_cursorHotspot;
        std::vector<MouseButtonInteractionEventDelegate> m_buttonInteractionDelegates;
        std::array<EMouseButtonStates, eMouseButton_NumButtons> m_buttonStates;
    };

}