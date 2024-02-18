#include "MouseDevice.h"

#include "../Core/Assert.h"
#include "../Core/Logger.h"

namespace Warp
{
	
	void MouseDevice::AddMouseButtonInteractionDelegate(const MouseButtonInteractionEventDelegate& delegate)
	{
		// If nullptr - do not add it basically
		if (!delegate)
		{
			return;
		}

		m_buttonInteractionDelegates.push_back(delegate);
	}

	void MouseDevice::SetMouseButtonStates(EMouseButton button, EMouseButtonStates states)
	{
		WARP_ASSERT(button < eMouseButton_NumButtons, "Out of bounds");
		EMouseButtonStates& prevStates = m_buttonStates[button];
		if (prevStates != states)
		{
			// TODO: (as of 18.02.2024) Delegates may be called from another thread. Racing is not handled here
			const CursorHotspot& hotspot = GetCursorHotspot();
			for (const MouseButtonInteractionEventDelegate& delegate : m_buttonInteractionDelegates)
			{
				WARP_ASSERT(delegate, "Should not be nullptr here");
				std::invoke(delegate, EvMouseButtonInteraction{
						.Button = button,
						.PrevStates = prevStates,
						.NextStates = states,
						.InteractionHotspot = hotspot,
					});
			}
		}
		prevStates = states;
	}

	EMouseButtonStates MouseDevice::GetMouseButtonStates(EMouseButton button) const
	{
		WARP_ASSERT(button < eMouseButton_NumButtons, "Out of bounds");
		return m_buttonStates[button];
	}

}