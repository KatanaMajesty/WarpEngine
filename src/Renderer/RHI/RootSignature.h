#pragma once

#include <vector>

#include "stdafx.h"
#include "DeviceChild.h"

namespace Warp
{

    class RHIDescriptorTable
    {
    public:
        RHIDescriptorTable() = default;
        RHIDescriptorTable(UINT numDescriptorRanges, D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

        RHIDescriptorTable& AddDescriptorRange(
            D3D12_DESCRIPTOR_RANGE_TYPE type,
            UINT numDescriptors,
            UINT shaderRegister,
            UINT registerSpace,
            UINT offsetInDescriptors = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

        inline constexpr UINT GetNumDescriptorRanges() const { return m_numDescriptorRanges; };
        const D3D12_DESCRIPTOR_RANGE1* GetAllRanges() const;

    private:
        UINT m_numDescriptorRanges = 0;
        D3D12_DESCRIPTOR_RANGE_FLAGS m_flags;
        std::vector<D3D12_DESCRIPTOR_RANGE1> m_ranges;
    };

    class RHIRootSignatureDesc
    {
    public:
        RHIRootSignatureDesc(UINT numRootParameters, D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE);

        RHIRootSignatureDesc& SetConstantBufferView(UINT rootIndex, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility);
        RHIRootSignatureDesc& SetShaderResourceView(UINT rootIndex, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility);
        RHIRootSignatureDesc& SetUnorderedAccessView(UINT rootIndex, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility);
        RHIRootSignatureDesc& SetDescriptorTable(UINT rootIndex, const RHIDescriptorTable& descriptorTable, D3D12_SHADER_VISIBILITY visibility);
        RHIRootSignatureDesc& Set32BitConstants(UINT rootIndex, UINT num32BitValues, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility);
        RHIRootSignatureDesc& AddStaticSampler(
            UINT shaderRegister,
            UINT registerSpace,
            D3D12_SHADER_VISIBILITY visibility,
            D3D12_FILTER filter,
            D3D12_TEXTURE_ADDRESS_MODE addressU,
            D3D12_TEXTURE_ADDRESS_MODE addressV,
            D3D12_TEXTURE_ADDRESS_MODE addressW,
            UINT maxAnisotropy = 16,
            D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
            D3D12_STATIC_BORDER_COLOR borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

        inline constexpr D3D12_ROOT_SIGNATURE_FLAGS GetFlags() const { return m_flags; }
        inline constexpr UINT GetNumRootParameters() const { return m_numRootParameters; }
        inline constexpr UINT GetNumStaticSamplers() const { return static_cast<UINT>(m_staticSamplers.size()); }
        const D3D12_ROOT_PARAMETER1* GetRootParameters() const;
        const D3D12_STATIC_SAMPLER_DESC* GetStaticSamplers() const;

    private:
        RHIRootSignatureDesc& SetParameter(UINT rootIndex, const D3D12_ROOT_PARAMETER1& param);

        D3D12_ROOT_SIGNATURE_FLAGS m_flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        UINT m_numRootParameters = 0;
        UINT m_numInitRootParameters = 0;
        std::vector<D3D12_ROOT_PARAMETER1> m_params;
        std::vector<D3D12_STATIC_SAMPLER_DESC> m_staticSamplers;
    };

    class RHIRootSignature : public RHIDeviceChild
    {
    public:
        RHIRootSignature() = default;
        RHIRootSignature(RHIDevice* device, const RHIRootSignatureDesc& desc);

        WARP_ATTR_NODISCARD inline ID3D12RootSignature* GetD3D12RootSignature() const { return m_D3D12RootSignature.Get(); }

        void SetName(std::wstring_view name);

    private:
        ComPtr<ID3D12RootSignature> m_D3D12RootSignature;
        ComPtr<ID3D10Blob> m_D3D12BlobWithRootSignature;
        ComPtr<ID3D10Blob> m_D3D12BlobWithError;
    };

}