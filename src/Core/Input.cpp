#include "Input.h"

namespace Warp
{

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