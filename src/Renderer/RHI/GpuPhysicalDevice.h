#pragma once

#include <memory>

#include "stdafx.h"
#include "../../Core/Defines.h"

namespace Warp
{

	struct GpuPhysicalDeviceDesc
	{
		HWND hwnd = nullptr;

		// Enables debug layer for a GPU device. This allows to enable.
		// The debug layer adds important debug and diagnostic features for application developers during application development.
		bool EnableDebugLayer = false;

		// GPU-based validation helps to identify the following errors :
		// 
		//	- Use of uninitialized or incompatible descriptors in a shader.
		//	- Use of descriptors referencing deleted Resources in a shader.
		//	- Validation of promoted resource states and resource state decay.
		//	- Indexing beyond the end of the descriptor heap in a shader.
		//	- Shader accesses of resources in incompatible state.
		//	- Use of uninitialized or incompatible Samplers in a shader.
		// 
		// For more info see https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer-gpu-based-validation
		// NOTICE! GPU-based validation can only be enabled if the debug layer is enabled as well.
		bool EnableGpuBasedValidation = false;
	};

	enum class GpuVendor
	{
		Unknown = 0,
		Nvidia,
		AMD,
	};

	class GpuDevice;

	// Warp uses an approach somewhat similar to Vulkan's. There are two types of device representations: physical and logical.
	// GpuPhysicalDevice is a physical representation of a GPU device, which is responsible for:
	//	- Adapter information
	//	- Debug Layers and GPU Validation
	//	- Storing and querying driver information
	class GpuPhysicalDevice
	{
	public:
		GpuPhysicalDevice() = default;
		GpuPhysicalDevice(const GpuPhysicalDeviceDesc& desc);

		// We do not want to allow copying physical device, as it is a singleton on a hardware-level anyway
		GpuPhysicalDevice(const GpuPhysicalDevice&) = delete;
		GpuPhysicalDevice& operator=(const GpuPhysicalDevice&) = delete;

		// Queries if the physical device is valid and was initialized correctly
		WARP_ATTR_NODISCARD bool IsValid() const;
		WARP_ATTR_NODISCARD inline constexpr GpuVendor GetDeviceVendor() const { return m_vendorID; }

		WARP_ATTR_NODISCARD inline HWND GetWindowHandle() const { return m_HWND; }
		WARP_ATTR_NODISCARD inline IDXGIFactory7* GetFactory() const { return m_factory.Get(); }
		WARP_ATTR_NODISCARD inline IDXGIAdapter1* GetAdapter() const { return m_adapter.Get(); }
		WARP_ATTR_NODISCARD inline bool IsDebugLayerEnabled() const { return m_debugInterface != nullptr; }

		GpuDevice* GetAssociatedLogicalDevice();

	private:
		void InitDebugInterface(bool enableGpuBasedValidation);
		bool SelectBestSuitableDXGIAdapter();

		HWND m_HWND = nullptr;
		ComPtr<IDXGIFactory7> m_factory;
		ComPtr<IDXGIAdapter1> m_adapter;
		DXGI_ADAPTER_DESC1 m_adapterDesc;
		GpuVendor m_vendorID = GpuVendor::Unknown;

		ComPtr<ID3D12Debug3> m_debugInterface;

		// Direct3D 12 devices are singletons per adapter. If a Direct3D 12 device already exists in the current process for a given adapter, 
		// then a subsequent call to D3D12CreateDevice returns the existing device
		std::unique_ptr<GpuDevice> m_linkedLogicalDevice;
	};

	class GpuPhysicalDeviceChild
	{
	public:
		GpuPhysicalDeviceChild() = default;
		explicit GpuPhysicalDeviceChild(GpuPhysicalDevice* physicalDevice)
			: m_physicalDevice(physicalDevice)
		{
		}

		// Returns a physical device, associated with the child
		WARP_ATTR_NODISCARD inline constexpr GpuPhysicalDevice* GetPhysicalDevice() const { return m_physicalDevice; }

	protected:
		GpuPhysicalDevice* m_physicalDevice;
	};

}