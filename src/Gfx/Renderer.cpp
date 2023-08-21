#include "Renderer.h"

#include "../Util/String.h"
#include "../Core/Logger.h"
#include "../Core/Assert.h"

namespace Warp
{

	bool Renderer::Create()
	{
		if (s_instance)
			Renderer::Delete();

		s_instance = new Renderer();
		return true;
	}

	void Renderer::Delete()
	{
		delete s_instance;
		s_instance = nullptr;
	}

	bool Renderer::Init()
	{
		if (!InitD3D12Api())
		{
			WARP_LOG_ERROR("Failed to init D3D12 Components");
			return false;
		}

		WARP_LOG_INFO("Renderer was initialized successfully");
		return true;
	}

	bool Renderer::InitD3D12Api()
	{
		UINT dxgiFactoryFlags = 0;
		HRESULT hr;

		// Init debug interface
#ifdef WARP_DEBUG
		ID3D12Debug* di;
		hr = D3D12GetDebugInterface(IID_PPV_ARGS(&di));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to get D3D12 Debug Interface");

		hr = di->QueryInterface(IID_PPV_ARGS(&m_debugInterface));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to query required D3D12 Debug Interface");

		m_debugInterface->EnableDebugLayer();
		m_debugInterface->SetEnableGPUBasedValidation(TRUE);
		di->Release();

		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

		hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create DXGI Factory");

		// Selecting an adapter
		if (!SelectBestSuitableDXGIAdapter())
		{
			WARP_LOG_FATAL("Failed to select D3D12 Adapter as no suitable was found");
			return false;
		}
		
		DXGI_ADAPTER_DESC1 adapterDesc;
		m_adapter->GetDesc1(&adapterDesc);

		std::string adapterVendor;
		WStringToString(adapterVendor, adapterDesc.Description);

		// Describe selected adapter to the client
		WARP_LOG_INFO("[Renderer] Successfully selected D3D12 Adapter");
		WARP_LOG_INFO("[Renderer] Adapter Description: {}", adapterVendor);
		WARP_LOG_INFO("[Renderer] Available Dedicated Video Memory: {} bytes", adapterDesc.DedicatedVideoMemory);

		hr = D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));
		WARP_ASSERT(SUCCEEDED(hr), "Failed to create D3D12 Device");
		return true;
	}

	bool Renderer::IsDXGIAdapterSuitable(IDXGIAdapter1* adapter, const DXGI_ADAPTER_DESC1& desc)
	{
		// Don't select render driver, provided by D3D12. We only use physical hardware
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			return false;

		// Passing nullptr as a parameter would just test the adapter for compatibility
		if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
			return false;

		return true;
	}

	bool Renderer::SelectBestSuitableDXGIAdapter()
	{
		WARP_ASSERT(m_factory, "Factory cannot be nullptr at this point");

		SIZE_T largestDedicatedMemory = 0;
		IDXGIAdapter1* adapter;
		for (UINT i = 0;
			(HRESULT)m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; // Implicit cast between different types is fine...
			++i)
		{

			DXGI_ADAPTER_DESC1 adapterDesc;
			adapter->GetDesc1(&adapterDesc);
			
			// If the adapter is suitable, we can compare it with current adapter
			if (IsDXGIAdapterSuitable(adapter, adapterDesc) && largestDedicatedMemory < adapterDesc.DedicatedVideoMemory)
			{
				largestDedicatedMemory = adapterDesc.DedicatedVideoMemory;
				m_adapter.Attach(adapter);
				continue;
			}

			adapter->Release();
			adapter = nullptr;
		}

		return m_adapter != nullptr;
	}

}