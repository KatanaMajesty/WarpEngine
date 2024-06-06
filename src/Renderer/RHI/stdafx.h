#pragma once

// 12/11/23 moved these to WinAPI.h, should not bother anymore
//#ifndef WIN32_LEAN_AND_MEAN
//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
//#endif
//
//#ifndef NOMINMAX
//#define NOMINMAX
//#endif					// Exclude rarely-used stuff from Windows headers.

//#include <Windows.h>
#include "../../WinAPI.h"

// #pragma comment(lib, "DirectXTK.lib")
#pragma comment(lib, "dxguid.lib") // for guids such as WKPDID_D3DDebugObjectName
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "D3DCompiler.lib")

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>

#include <string>
#include <string_view>
#include <format> // TODO: Currently here, will be removed tho
#include <wrl.h>

// Must be included for WARP_DEBUG to be defined
#include "../../Core/Defines.h"

using Microsoft::WRL::ComPtr;

// It is best to avoid calling WARP_RHI_VALIDATE on a per-frame basis. Prefer using the macro for one-time resource initialization only
#include <stdexcept>
#define WARP_RHI_VALIDATE(expr) \
	do \
	{  \
		HRESULT hr = expr; \
		if (FAILED(hr)) \
		{ \
		throw std::runtime_error("Hresult is in failed state"); \
		} \
	} while(false) // use do-while to enforce semicolon (?)

#define WARP_COM_SAFE_RELEASE(ptr) if (ptr) { ptr->Release(); ptr = nullptr; }

#ifdef WARP_DEBUG
#define WARP_SET_RHI_NAME(Ptr, NAME) if (Ptr) \
	{ \
		Ptr->SetName(std::wstring(L"WARP_").append(NAME).data()); \
	}
#else
#define WARP_SET_RHI_NAME(objectPtr, NAME)
#endif