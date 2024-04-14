#include "PIXRuntime.h"

#define	USE_PIX
#include <WinPixEventRuntime/pix3.h>

namespace Warp::Pix
{

    void BeginEvent(RHICommandContext* commandContext, std::string_view name)
    {
        static BYTE ColorIndex = 0;
        PIXBeginEvent(commandContext->GetD3D12CommandList(), PIX_COLOR_INDEX(ColorIndex++), name.data());
    }

    void EndEvent(RHICommandContext* commandContext)
    {
        PIXEndEvent(commandContext->GetD3D12CommandList());
    }

    void NotifyWakeFromSignal(HANDLE event)
    {
        PIXNotifyWakeFromFenceSignal(event);
    }

}
