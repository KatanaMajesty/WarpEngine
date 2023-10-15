#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif					// Exclude rarely-used stuff from Windows headers.

#include <Windows.h>

// #pragma comment(lib, "DirectXTK.lib")
// #pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "D3DCompiler.lib")

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>

#include <string>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

// It is best to avoid calling WARP_RHI_VALIDATE on a per-frame basis. Prefer using the macro for one-time resource initialization only
#define WARP_RHI_VALIDATE(expr) \
	do \
	{  \
		HRESULT hr = expr; \
		if (FAILED(hr)) \
		{ \
		throw std::runtime_error("Hresult is in failed state"); \
		} \
	} while(false) // use do-while to enforce semicolon