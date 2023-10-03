#include "GpuDevice.h"

#include "../../Core/Defines.h"
#include "../../Core/Assert.h"
#include "../../Core/Logger.h"
#include "../../Util/String.h"

#include "GpuValidationLayer.h"

namespace Warp
{

	GpuDevice::~GpuDevice()
	{
		if (m_debugInfoQueue)
		{
			WARP_LOG_INFO("Unregistering validation layer message callback");
			WARP_MAYBE_UNUSED HRESULT hr = m_debugInfoQueue->UnregisterMessageCallback(m_messageCallbackCookie);
			WARP_ASSERT(SUCCEEDED(hr));
		}
	}

	bool GpuDevice::Init(const GpuDeviceDesc& desc)
	{
		HWND hwnd = desc.hwnd;
		WARP_ASSERT(hwnd, "Provided HWND is invalid");

		UINT dxgiFactoryFlags = 0;
		WARP_MAYBE_UNUSED HRESULT hr;

		if (desc.EnableDebugLayer)
		{
			ID3D12Debug* di;
			hr = D3D12GetDebugInterface(IID_PPV_ARGS(&di));
			WARP_ASSERT(SUCCEEDED(hr), "Failed to get D3D12 Debug Interface");

			hr = di->QueryInterface(IID_PPV_ARGS(&m_debugInterface));
			WARP_ASSERT(SUCCEEDED(hr), "Failed to query required D3D12 Debug Interface");

			// Enable debug layer and check if we need to enable GBV
			WARP_LOG_INFO("Enabling debug level for a GpuDevice");
			m_debugInterface->EnableDebugLayer();

			if (desc.EnableGpuBasedValidation)
			{
				WARP_LOG_INFO("Enabling GPU-Based validation (GBV)");
				m_debugInterface->SetEnableGPUBasedValidation(TRUE);
			}
			di->Release();

			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}

		hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory));
		WARP_LOG_INFO("Creating dxgi factory");
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

		// Setup validation layer messages callbacks
		if (desc.EnableDebugLayer)
		{
			hr = m_device->QueryInterface(IID_PPV_ARGS(m_debugInfoQueue.ReleaseAndGetAddressOf()));
			if (FAILED(hr))
			{
				WARP_LOG_WARN("Failed to initialize debug info queue. Validation layer message callback won't be initialized");
			}
			else
			{
				hr = m_debugInfoQueue->RegisterMessageCallback(OnDebugLayerMessage, D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &m_messageCallbackCookie);
				WARP_ASSERT(SUCCEEDED(hr), "Failed to register validation layer message callback");
			}
		}
		
		hr = m_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_PRINT_SCREEN); // No need for print-screen rn
		if (FAILED(hr))
		{
			// TODO: Do not return but handle somehow in future
			WARP_LOG_WARN("Failed to monitor window's message queue. Fullscreen-to-Windowed will not be available via alt+enter");
		}

		return true;
	}

	bool IsDXGIAdapterSuitable(IDXGIAdapter1* adapter, const DXGI_ADAPTER_DESC1& desc)
	{
		// Don't select render driver, provided by D3D12. We only use physical hardware
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			return false;

		// Passing nullptr as a parameter would just test the adapter for compatibility
		if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
			return false;

		return true;
	}

	bool GpuDevice::SelectBestSuitableDXGIAdapter()
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