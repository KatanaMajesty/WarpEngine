#include "PhysicalDevice.h"

#include "../../Core/Assert.h"
#include "../../Core/Logger.h"
#include "Device.h"

// https://pcisig.com/membership/member-companies?combine=&order=field_vendor_id&sort=asc
#define WARP_PCI_VENDOR_ID_NVIDIA 0x10DE
#define WARP_PCI_VENDOR_ID_AMD    0x1022
#define WARP_PCI_VENDOR_ID_INTEL  0x8086

namespace Warp
{

    RHIPhysicalDevice::RHIPhysicalDevice(const RHIPhysicalDeviceDesc& desc)
        : m_HWND(desc.hwnd)
    {
        UINT dxgiFactoryFlags = 0;
        if (desc.EnableDebugLayer)
        {
            InitDebugInterface(desc.EnableGpuBasedValidation);
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        WARP_LOG_INFO("Creating DXGI factory");
        WARP_RHI_VALIDATE(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

        // Selecting an adapter
        if (!SelectBestSuitableDXGIAdapter(DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE))
        {
            WARP_LOG_FATAL("Failed to select D3D12 Adapter as no suitable was found");
            return;
        }

        WARP_ASSERT(m_adapter);
        m_adapter->GetDesc1(&m_adapterDesc);

        // Set the vendorID
        switch (m_adapterDesc.VendorId)
        {
        case (WARP_PCI_VENDOR_ID_NVIDIA): m_vendorID = RHIVendor::Nvidia; break;
        case (WARP_PCI_VENDOR_ID_AMD):    m_vendorID = RHIVendor::Amd; break;
        case (WARP_PCI_VENDOR_ID_INTEL):  m_vendorID = RHIVendor::Intel; break;
        default:
        {
            WARP_LOG_WARN("Failed to determine if the GPU vendor is AMD or Nvidia. GpuVendor is set to \"unknown\"");
            m_vendorID = RHIVendor::Unknown;
        }; break;
        }

        WARP_MAYBE_UNUSED HRESULT hr = m_factory->MakeWindowAssociation(m_HWND, DXGI_MWA_NO_PRINT_SCREEN); // No need for print-screen rn
        if (FAILED(hr))
        {
            // TODO: Do not return but handle somehow in future
            WARP_LOG_WARN("Failed to monitor window's message queue. Fullscreen-to-Windowed will not be available via alt+enter");
        }
    }

    BOOL RHIPhysicalDevice::IsValid() const
    {
        // TODO: Maybe add more checks for adapter
        return m_HWND && m_factory && m_adapter;
    }

    void RHIPhysicalDevice::AssociateLogicalDevice(RHIDevice* device)
    {
        if (m_associatedLogicalDevice)
        {
            WARP_LOG_FATAL("Cannot reset logical device once it was already associated!");
            throw std::runtime_error("Cannot reset logical device once it was already associated!");
            return;
        }

        m_associatedLogicalDevice = device;
    }

    RHIDevice* RHIPhysicalDevice::GetAssociatedLogicalDevice()
    {
        WARP_ASSERT(m_associatedLogicalDevice, "Cannot retrieve associated logical device before it was created");
        return m_associatedLogicalDevice;
    }

    void RHIPhysicalDevice::InitDebugInterface(BOOL enableGpuBasedValidation)
    {
        WARP_RHI_VALIDATE(D3D12GetDebugInterface(IID_PPV_ARGS(m_debugInterface.ReleaseAndGetAddressOf())));
        WARP_ASSERT(m_debugInterface);
        WARP_LOG_INFO("Enabling debug level for an RHIDevice");
        m_debugInterface->EnableDebugLayer();

        if (enableGpuBasedValidation)
        {
            WARP_LOG_INFO("Enabling GPU-Based validation");
            m_debugInterface->SetEnableGPUBasedValidation(TRUE);
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

    BOOL RHIPhysicalDevice::SelectBestSuitableDXGIAdapter(DXGI_GPU_PREFERENCE preference)
    {
        WARP_ASSERT(m_factory, "Factory cannot be nullptr at this point");

        UINT64 maxVideoMemory = 0;
        IDXGIAdapter3* adapter;
        for (UINT i = 0;
            (HRESULT)m_factory->EnumAdapterByGpuPreference(i, preference, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; // Implicit cast between different types is fine...
            ++i)
        {
            DXGI_ADAPTER_DESC1 adapterDesc;
            adapter->GetDesc1(&adapterDesc);

            // If the adapter is suitable, we can compare it with current adapter
            if (adapterDesc.DedicatedVideoMemory > maxVideoMemory && IsDXGIAdapterSuitable(adapter, adapterDesc))
            {
                maxVideoMemory = adapterDesc.DedicatedVideoMemory;
                m_adapter.Attach(adapter);
                continue;
            }

            adapter->Release();
            adapter = nullptr;
        }

        return (m_adapter != nullptr) ? TRUE : FALSE;
    }

}