#include "Input.h"

namespace Warp
{
	void InputManager::SetCursorPos(int64_t x, int64_t y)
	{
		m_cursorX = x;
		m_cursorY = y;
	}

	void InputManager::SetKeyIsPressed(EKeycode code, bool pressed)
	{
		m_keyStates[code] = pressed;
	}

	void InputManager::SetButtonIsPressed(EMouseButton button, bool pressed)
	{
		m_mouseButtonStates[button] = pressed;
	}

	bool InputManager::IsKeyPressed(EKeycode code) const
	{
		return m_keyStates[code];
	}

	bool InputManager::IsButtonPressed(EMouseButton button) const
	{
		return m_mouseButtonStates[button];
	}

}