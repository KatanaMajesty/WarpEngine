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

		// TODO: Added 16.02.2024 for gbuffer views -> temp, remove them and implement normally
		eKeycode_Z,
		eKeycode_X,
		eKeycode_C,

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
		void SetCursorPos(int64_t x, int64_t y);
		inline int64_t GetCursorX() const { return m_cursorX; }
		inline int64_t GetCursorY() const { return m_cursorY; }

		void SetKeyIsPressed(EKeycode code, bool pressed);
		void SetButtonIsPressed(EMouseButton button, bool pressed);

		bool IsKeyPressed(EKeycode code) const;
		bool IsButtonPressed(EMouseButton button) const;

	private:
		std::array<bool, eKeycode_NumKeycodes> m_keyStates;
		std::array<bool, eMouseButton_NumButtons> m_mouseButtonStates;
		int64_t m_cursorX = 0;
		int64_t m_cursorY = 0;
	};

}