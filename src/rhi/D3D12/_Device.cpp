#include "_Device.h"

#include "../../Core/Assert.h"
#include "../../Core/Defines.h"

// https://pcisig.com/membership/member-companies?combine=&order=field_vendor_id&sort=asc
#define WARP_PCI_VENDOR_ID_NVIDIA 0x10DE
#define WARP_PCI_VENDOR_ID_AMD 0x1022
#define WARP_PCI_VENDOR_ID_INTEL 0x8086

namespace Warp::rhi
{

    D3D12PhysicalDevice::D3D12PhysicalDevice(const D3D12PhysicalDeviceDesc& desc)
        : m_HWND(desc.Hwnd)
    {
        UINT dxgiFactoryFlags = 0;
        if (desc.EnableDebugLayer)
        {
            InitDebugInterface(desc.EnableGpuBasedValidation);
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        WARP_LOG_INFO("Creating DXGI factory");
        WARP_RHI_VALIDATE(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory)));

        // Selecting an adapter
        if (!SelectBestSuitableDXGIAdapter(DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE))
        {
            WARP_LOG_FATAL("Failed to select D3D12 Adapter as no suitable was found");
            return;
        }

        WARP_ASSERT(m_Adapter);
        m_Adapter->GetDesc1(&m_AdapterDesc);

        // Set the vendorID
        switch (m_AdapterDesc.VendorId)
        {
        case (WARP_PCI_VENDOR_ID_NVIDIA):   m_DeviceVendor = EDeviceVendor::Nvidia; break;
        case (WARP_PCI_VENDOR_ID_AMD):      m_DeviceVendor = EDeviceVendor::Amd; break;
        case (WARP_PCI_VENDOR_ID_INTEL):    m_DeviceVendor = EDeviceVendor::Intel; break;
        default: {
            WARP_LOG_WARN("Failed to determine if the GPU vendor is AMD or Nvidia. GpuVendor is set to \"unknown\"");
            m_DeviceVendor = EDeviceVendor::Unknown;
        };
        break;
        }

        WARP_MAYBE_UNUSED HRESULT hr = m_Factory->MakeWindowAssociation(m_HWND, DXGI_MWA_NO_PRINT_SCREEN); // No need for print-screen rn
        if (FAILED(hr))
        {
            // TODO: Do not return but handle somehow in future
            WARP_LOG_WARN("Failed to monitor window's message queue. Fullscreen-to-Windowed will not be available via alt+enter");
        }
    }

    WARP_ATTR_NODISCARD BOOL D3D12PhysicalDevice::IsValid() const
    {
        return FALSE;
    }

    void D3D12PhysicalDevice::InitDebugInterface(BOOL enableGpuBasedValidation)
    {
        WARP_RHI_VALIDATE(D3D12GetDebugInterface(IID_PPV_ARGS(m_DebugInterface.ReleaseAndGetAddressOf())));
        WARP_ASSERT(m_DebugInterface);
        WARP_LOG_INFO("Enabling debug level for an RHIDevice");
        m_DebugInterface->EnableDebugLayer();

        if (enableGpuBasedValidation)
        {
            WARP_LOG_INFO("Enabling GPU-Based validation");
            m_DebugInterface->SetEnableGPUBasedValidation(TRUE);
        }
    }

    // TODO: Maybe move this inside the RHIPhysicalDevice class?
    static bool IsDXGIAdapterSuitable(IDXGIAdapter3* adapter, const DXGI_ADAPTER_DESC1& desc)
    {
        // Don't select render driver, provided by D3D12. We only use physical hardware
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            return false;

        // Passing nullptr as a parameter would just test the adapter for compatibility
        if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
            return false;

        return true;
    }

    BOOL D3D12PhysicalDevice::SelectBestSuitableDXGIAdapter(DXGI_GPU_PREFERENCE preference)
    {
        WARP_ASSERT(m_Factory, "Factory cannot be nullptr at this point");

        UINT64 maxVideoMemory = 0;
        IDXGIAdapter3* adapter;
        for (UINT i = 0;
             (HRESULT)m_Factory->EnumAdapterByGpuPreference(i, preference, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; // Implicit cast between different types is fine...
             ++i)
        {
            DXGI_ADAPTER_DESC1 adapterDesc;
            adapter->GetDesc1(&adapterDesc);

            // If the adapter is suitable, we can compare it with current adapter
            if (adapterDesc.DedicatedVideoMemory > maxVideoMemory && IsDXGIAdapterSuitable(adapter, adapterDesc))
            {
                maxVideoMemory = adapterDesc.DedicatedVideoMemory;
                m_Adapter.Attach(adapter);
                continue;
            }

            adapter->Release();
            adapter = nullptr;
        }

        return (m_Adapter != nullptr) ? TRUE : FALSE;
    }

}