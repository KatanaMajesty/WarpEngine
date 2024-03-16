#include "KeyboardDevice.h"

#include "../Core/Assert.h"

namespace Warp
{

    void KeyboardDevice::AddKeyInteractionDelegate(const KeyInteractionEventDelegate& delegate)
    {
        // If nullptr - do not add it basically
        if (!delegate)
        {
            return;
        }

        m_keyInteractionDelegates.push_back(delegate);
    }

    void KeyboardDevice::SetKeycodeState(EKeycode keycode, EKeycodeState state)
    {
        WARP_ASSERT(keycode < eKeycode_NumKeycodes, "Out of range!");

        EKeycodeState& prevState = m_keycodeStates[keycode];
        if (prevState == state)
        {
            return;
        }

        // TODO: (as of 17.02.2024) Delegates may be called from another thread. Racing is not handled here
        for (const KeyInteractionEventDelegate& delegate : m_keyInteractionDelegates)
        {
            WARP_ASSERT(delegate, "Nullptr delegates should not be in m_keyInteractionDelegates!");
            std::invoke(delegate, EvKeyInteraction{
                    .Keycode = keycode,
                    .PrevState = prevState,
                    .NextState = state,
                });
        }

        // We do not store repeatCount here. The only way to retrieve it is using event-based delegate propagation
        prevState = state;
    }

}