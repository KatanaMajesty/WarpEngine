#pragma once

#include <vector>
#include "stdafx.h"
#include "../Core/Defines.h"

namespace Warp
{

	using Microsoft::WRL::ComPtr;

	class Renderer
	{
	private:
		Renderer() = default;

	public:
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;
		~Renderer() = default;

		static bool Create();
		static void Delete();
		static inline constexpr Renderer& GetInstance() { return *s_instance; }

		bool Init();

	private:
		bool InitD3D12Api();
		bool IsDXGIAdapterSuitable(IDXGIAdapter1* adapter, const DXGI_ADAPTER_DESC1& desc);
		bool SelectBestSuitableDXGIAdapter();

		static inline Renderer* s_instance = nullptr;

		ComPtr<IDXGIFactory7> m_factory;
		ComPtr<IDXGIAdapter1> m_adapter;
		ComPtr<ID3D12Device9> m_device;

#ifdef WARP_DEBUG
		ComPtr<ID3D12Debug3> m_debugInterface;
		ComPtr<ID3D12DebugDevice> m_debugDevice;
#endif
	};

}