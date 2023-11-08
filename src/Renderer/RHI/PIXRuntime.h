#pragma once

#include "stdafx.h"
#include "CommandContext.h"
#include "../../Core/Defines.h"

namespace Warp::Pix
{

	// For more info https://devblogs.microsoft.com/pix/winpixeventruntime/

	void BeginEvent(RHICommandContext* commandContext, std::string_view name);
	void EndEvent(RHICommandContext* commandContext);

	struct ScopedEvent
	{
		inline ScopedEvent(RHICommandContext* commandContext, std::string_view name)
			: Context(commandContext)
		{
			BeginEvent(Context, name);
		}

		inline ~ScopedEvent()
		{
			EndEvent(Context);
		}

		RHICommandContext* Context;
	};

	// TODO: Replace this with HANDLE. I was to lazy to figure-out includes
	// Smhw pix3.h wants stdafx.h to be included before itself
	// void NotifyWakeFromSignal(void* event);
}

#define WARP_CONCAT(a, b) a##b
#define WARP_GET_SCOPED_EVENT_VARNAME(a, b) WARP_CONCAT(a, b)
#define WARP_SCOPED_EVENT(Context, Name) ::Warp::Pix::ScopedEvent WARP_GET_SCOPED_EVENT_VARNAME(WarpEvent, __LINE__)(Context, Name)
