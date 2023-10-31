#pragma once

#include "../../Core/Defines.h"

#include "stdafx.h"
#include "GpuDeviceChild.h"
#include "GpuDevice.h"
#include "RootSignature.h"

namespace Warp
{
	

	struct RHIGraphicsPipelineDesc
	{
		RHIRootSignature RootSignature;
		std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements; // TODO: Remove InputElements when we switch to mesh shaders
		D3D12_SHADER_BYTECODE VS;
		D3D12_SHADER_BYTECODE PS;
		D3D12_BLEND_DESC BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		D3D12_RASTERIZER_DESC Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		D3D12_DEPTH_STENCIL_DESC DepthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		UINT NumRTVs = 0;
		DXGI_FORMAT RTVFormats[8];
		DXGI_FORMAT DSVFormat;
		D3D12_PIPELINE_STATE_FLAGS Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	};

	struct RHIMeshPipelineDesc
	{
		RHIRootSignature RootSignature;
		D3D12_SHADER_BYTECODE AS;
		D3D12_SHADER_BYTECODE MS;
		D3D12_SHADER_BYTECODE PS;
		D3D12_BLEND_DESC BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		D3D12_RASTERIZER_DESC Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		D3D12_DEPTH_STENCIL_DESC DepthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		UINT NumRTVs = 0;
		DXGI_FORMAT RTVFormats[8]{};
		DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;
		D3D12_PIPELINE_STATE_FLAGS Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	};

	struct RHIComputePipelineDesc
	{
		RHIRootSignature RootSignature;
		D3D12_SHADER_BYTECODE CS;
		D3D12_PIPELINE_STATE_FLAGS Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	};

	class GpuPipelineState : public GpuDeviceChild
	{
	public:
		GpuPipelineState() = default;
		explicit GpuPipelineState(GpuDevice* device)
			: GpuDeviceChild(device)
		{
		}

		WARP_ATTR_NODISCARD inline ID3D12PipelineState* GetD3D12PipelineState() const { return m_D3D12PipelineState.Get(); }

	protected:
		ComPtr<ID3D12PipelineState> m_D3D12PipelineState;
	};

	class RHIGraphicsPipelineState final : public GpuPipelineState
	{
	public:
		RHIGraphicsPipelineState() = default;
		RHIGraphicsPipelineState(GpuDevice* device, const RHIGraphicsPipelineDesc& desc);
		
		WARP_ATTR_NODISCARD inline constexpr const RHIGraphicsPipelineDesc& GetDesc() const { return m_desc; }

	private:
		RHIGraphicsPipelineDesc m_desc;
	};

	class RHIMeshPipelineState final : public GpuPipelineState
	{
	public:
		RHIMeshPipelineState() = default;
		RHIMeshPipelineState(GpuDevice* device, const RHIMeshPipelineDesc& desc);

		WARP_ATTR_NODISCARD inline constexpr const RHIMeshPipelineDesc& GetDesc() const { return m_desc; }

	private:
		RHIMeshPipelineDesc m_desc;
	};

	class RHIComputePipelineState final : public GpuPipelineState
	{
	public:
		RHIComputePipelineState() = default;
		RHIComputePipelineState(GpuDevice* device, const RHIComputePipelineDesc& desc);

		WARP_ATTR_NODISCARD inline constexpr const RHIComputePipelineDesc& GetDesc() const { return m_desc; }

	private:
		RHIComputePipelineDesc m_desc;
	};

}