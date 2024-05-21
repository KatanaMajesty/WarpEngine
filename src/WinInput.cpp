#include "WinWrap.h"

#include <unordered_map>
#include <windowsx.h>

#include "Input/DeviceManager.h"
#include "Input/KeyboardDevice.h"
#include "Input/MouseDevice.h"

using  KeyMappingContainer = std::unordered_map<WORD, Warp::EKeycode>;
static KeyMappingContainer g_keyMapping;

namespace Warp::WinWrap
{

    void WinProc_ProcessInput(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        InputDeviceManager& inputDeviceManager = InputDeviceManager::Get();

        KeyboardDevice& keyboard = inputDeviceManager.GetKeyboard();
        KeyMappingContainer& keyMapping = g_keyMapping;

        MouseDevice& mouse = inputDeviceManager.GetMouse();
        switch (uMsg)
        {
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            WORD vkCode = LOWORD(wParam);
            WORD keyFlags = HIWORD(lParam);
            BOOL isUp = (keyFlags & KF_UP);

            auto it = keyMapping.find(vkCode);
            if (it == keyMapping.end())
            {
                break;
            }

            EKeycode keycode = it->second;
            EKeycodeState nextState = isUp ? eKeycodeState_Released : eKeycodeState_Pressed;
            keyboard.SetKeycodeState(keycode, nextState);
        };
        break;
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP: {
            WORD flags = LOWORD(wParam);
            EMouseButton button = eMouseButton_Left;
            EMouseButtonStates states = (flags & MK_LBUTTON) ? eMouseButtonState_ClickedOnce : eMouseButtonState_Released;
            if (states == eMouseButtonState_ClickedOnce && uMsg == WM_LBUTTONDBLCLK)
                states = eMouseButtonState_ClickedTwice;

            mouse.SetMouseButtonStates(button, states);
        };
        break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK: {
            WORD flags = LOWORD(wParam);
            EMouseButton button = eMouseButton_Right;
            EMouseButtonStates states = (flags & MK_RBUTTON) ? eMouseButtonState_ClickedOnce : eMouseButtonState_Released;
            if (states == eMouseButtonState_ClickedOnce && uMsg == WM_RBUTTONDBLCLK)
                states = eMouseButtonState_ClickedTwice;

            mouse.SetMouseButtonStates(button, states);
        };
        break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK: {
            WORD flags = LOWORD(wParam);
            EMouseButton button = eMouseButton_Middle;
            EMouseButtonStates states = (flags & MK_MBUTTON) ? eMouseButtonState_ClickedOnce : eMouseButtonState_Released;
            if (states == eMouseButtonState_ClickedOnce && uMsg == WM_MBUTTONDBLCLK)
            {
                states = eMouseButtonState_ClickedTwice;
            }

            mouse.SetMouseButtonStates(button, states);
        };
        break;
        case WM_MOUSEMOVE: {
            // Do not use the LOWORD or HIWORD macros to extract the x- and y- coordinates of the cursor position
            // because these macros return incorrect results on systems with multiple monitors.
            // Systems with multiple monitors can have negative x- and y- coordinates, and LOWORD and HIWORD treat the coordinates as unsigned quantities.
            INT32 x = GET_X_LPARAM(lParam);
            INT32 y = GET_Y_LPARAM(lParam);
            mouse.SetCursorHotspot(MouseDevice::CursorHotspot{
                .PosX = x,
                .PosY = y,
            });
        };
        break;
        }
    }

    void InitInputMappings()
    {
        using namespace Warp;
        KeyMappingContainer& keyMapping = g_keyMapping;

        // F1 - F12 (we do not support F13 - F24)
        keyMapping[VK_F1] = eKeycode_F1;
        keyMapping[VK_F2] = eKeycode_F2;
        keyMapping[VK_F3] = eKeycode_F3;
        keyMapping[VK_F4] = eKeycode_F4;
        keyMapping[VK_F5] = eKeycode_F5;
        keyMapping[VK_F6] = eKeycode_F6;
        keyMapping[VK_F7] = eKeycode_F7;
        keyMapping[VK_F8] = eKeycode_F8;
        keyMapping[VK_F9] = eKeycode_F9;
        keyMapping[VK_F10] = eKeycode_F10;
        keyMapping[VK_F11] = eKeycode_F11;
        keyMapping[VK_F12] = eKeycode_F12;

        // Keyboard nums 1 - 9 and 0
        keyMapping['0'] = eKeycode_0;
        keyMapping['1'] = eKeycode_1;
        keyMapping['2'] = eKeycode_2;
        keyMapping['3'] = eKeycode_3;
        keyMapping['4'] = eKeycode_4;
        keyMapping['5'] = eKeycode_5;
        keyMapping['6'] = eKeycode_6;
        keyMapping['7'] = eKeycode_7;
        keyMapping['8'] = eKeycode_8;
        keyMapping['9'] = eKeycode_9;

        // Keyboard chars 'A' - 'Z'
        keyMapping['A'] = eKeycode_A;
        keyMapping['B'] = eKeycode_B;
        keyMapping['C'] = eKeycode_C;
        keyMapping['D'] = eKeycode_D;
        keyMapping['E'] = eKeycode_E;
        keyMapping['F'] = eKeycode_F;
        keyMapping['G'] = eKeycode_G;
        keyMapping['H'] = eKeycode_H;
        keyMapping['I'] = eKeycode_I;
        keyMapping['J'] = eKeycode_J;
        keyMapping['K'] = eKeycode_K;
        keyMapping['L'] = eKeycode_L;
        keyMapping['M'] = eKeycode_M;
        keyMapping['N'] = eKeycode_N;
        keyMapping['O'] = eKeycode_O;
        keyMapping['P'] = eKeycode_P;
        keyMapping['Q'] = eKeycode_Q;
        keyMapping['R'] = eKeycode_R;
        keyMapping['S'] = eKeycode_S;
        keyMapping['T'] = eKeycode_T;
        keyMapping['U'] = eKeycode_U;
        keyMapping['V'] = eKeycode_V;
        keyMapping['W'] = eKeycode_W;
        keyMapping['X'] = eKeycode_X;
        keyMapping['Y'] = eKeycode_Y;
        keyMapping['Z'] = eKeycode_Z;
    }
    
};