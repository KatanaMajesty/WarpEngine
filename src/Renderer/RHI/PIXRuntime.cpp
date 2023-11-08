#include "PIXRuntime.h"

#define	USE_PIX // TODO: Currently bruteforce. Just a workaround
#include <pix3.h>
#pragma comment(lib, "WinPixEventRuntime.lib")

namespace Warp::Pix
{

	void BeginEvent(RHICommandContext* commandContext, std::string_view name)
	{
		static BYTE ColorIndex = 0;
		WARP_EXPAND_DEBUG_ONLY(
			PIXBeginEvent(commandContext->GetD3D12CommandList(), PIX_COLOR_INDEX(ColorIndex++), name.data()));
	}

	void EndEvent(RHICommandContext* commandContext)
	{
		WARP_EXPAND_DEBUG_ONLY(
			PIXEndEvent(commandContext->GetD3D12CommandList()));
	}

}
