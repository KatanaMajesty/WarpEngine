#pragma once

#include "../../rc/RcPtr.h"

// TODO: tmp stdafx from src/Renderer/RHI -> should be moved to src/rhi
#include "../../Renderer/RHI/stdafx.h"

namespace Warp::rhi
{

    struct D3D12DeviceDesc
    {
        HWND Hwnd = NULL;

        // Enables debug layer for a GPU device. This allows to enable.
        // The debug layer adds important debug and diagnostic features for application developers during application development.
        BOOL EnableDebugLayer = FALSE;

        // For more info see https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer-gpu-based-validation
        // NOTICE! GPU-based validation can only be enabled if the debug layer is enabled as well.
        BOOL EnableGpuBasedValidation = FALSE;
    };

    enum class EDeviceVendor
    {
        Unknown = 0,
        Nvidia,
        Amd,
        Intel,
    };

    class D3D12Device
    {
    public:
        D3D12Device() = default;
        D3D12Device(const D3D12Device& desc);

        // We do not want to allow copying physical device, as it is a singleton on a hardware-level anyway
        D3D12Device(const D3D12Device&) = delete;
        D3D12Device& operator=(const D3D12Device&) = delete;

        // Queries if the physical device is valid and was initialized correctly
        WARP_ATTR_NODISCARD BOOL IsValid() const;
        WARP_ATTR_NODISCARD EDeviceVendor GetDeviceVendor() const { return m_DeviceVendor; }

        WARP_ATTR_NODISCARD HWND GetWindowHandle() const { return m_HWND; }
        WARP_ATTR_NODISCARD IDXGIFactory7* GetFactory() const { return m_Factory.Get(); }
        WARP_ATTR_NODISCARD IDXGIAdapter1* GetAdapter() const { return m_Adapter.Get(); }
        WARP_ATTR_NODISCARD BOOL IsDebugLayerEnabled() const { return m_DebugInterface != nullptr; }

    private:
        void InitDebugInterface(BOOL enableGpuBasedValidation);
        BOOL SelectBestSuitableDXGIAdapter(DXGI_GPU_PREFERENCE preference);

        HWND m_HWND;
        rc::RcPtr<IDXGIFactory7> m_Factory;
        rc::RcPtr<IDXGIAdapter3> m_Adapter;
        DXGI_ADAPTER_DESC1 m_AdapterDesc;

        EDeviceVendor m_DeviceVendor = EDeviceVendor::Unknown;
        
        rc::RcPtr<ID3D12Debug3> m_DebugInterface;     
    };

    

    class D3D12Device
    {
    public:
        

        const D3D12DeviceCapabilities& GetDeviceCapabilities() const { return m_DeviceCapabilities; }

    private:
        D3D12DeviceCapabilities m_DeviceCapabilities;

        rc::RcPtr<ID3D12Device9> m_Device;
        rc::RcPtr<ID3D12DebugDevice> m_DebugDevice;
        rc::RcPtr<ID3D12InfoQueue1> m_DebugInfoQueue;
        DWORD m_MessageCallbackCookie = DWORD(-1);     
    };

}