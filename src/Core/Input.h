#pragma once

#include <array>

namespace Warp
{

	enum EKeycode
	{
		eKeycode_W,
		eKeycode_A,
		eKeycode_S,
		eKeycode_D,
		eKeycode_NumKeycodes,
	};

	enum EMouseButton
	{
		eMouseButton_Left,
		eMouseButton_Middle,
		eMouseButton_Right,
		eMouseButton_NumButtons,
	};

	class InputManager
	{
	public:
		void SetKeyIsPressed(EKeycode code, bool pressed);
		void SetButtonIsPressed(EMouseButton button, bool pressed);

		bool IsKeyPressed(EKeycode code) const;
		bool IsButtonPressed(EMouseButton button) const;

	private:
		std::array<bool, eKeycode_NumKeycodes> m_keyStates;
		std::array<bool, eMouseButton_NumButtons> m_mouseButtonStates;
	};

}