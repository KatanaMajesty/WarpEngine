#pragma once

#include "stdafx.h"

namespace Warp
{

	void OnDebugLayerMessage(D3D12_MESSAGE_CATEGORY category,
		D3D12_MESSAGE_SEVERITY severity,
		D3D12_MESSAGE_ID ID,
		LPCSTR description,
		void* vpDevice);

}