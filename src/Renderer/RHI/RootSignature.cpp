#include "RootSignature.h"

#include "../../Core/Defines.h"
#include "../../Core/Assert.h"
#include "../../Util/Logger.h"
#include "Device.h"

namespace Warp
{

    RHIDescriptorTable::RHIDescriptorTable(UINT numDescriptorRanges, D3D12_DESCRIPTOR_RANGE_FLAGS flags)
        : m_numDescriptorRanges(numDescriptorRanges)
        , m_flags(flags)
    {
        m_ranges.reserve(numDescriptorRanges);
    }

    RHIDescriptorTable& RHIDescriptorTable::AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE type, UINT numDescriptors, UINT shaderRegister, UINT registerSpace, UINT offsetInDescriptors)
    {
        CD3DX12_DESCRIPTOR_RANGE1 range;
        range.Init(type, numDescriptors, shaderRegister, registerSpace, m_flags, offsetInDescriptors);
        m_ranges.push_back(range);
        return *this;
    }

    const D3D12_DESCRIPTOR_RANGE1* RHIDescriptorTable::GetAllRanges() const
    {
        WARP_ASSERT(m_numDescriptorRanges == m_ranges.size());
        return m_ranges.data();
    }

    RHIRootSignatureDesc::RHIRootSignatureDesc(UINT numRootParameters, D3D12_ROOT_SIGNATURE_FLAGS flags)
        : m_flags(flags)
        , m_numRootParameters(numRootParameters)
    {
        m_params.resize(numRootParameters);
    }

    RHIRootSignatureDesc& RHIRootSignatureDesc::SetConstantBufferView(UINT rootIndex, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility)
    {
        CD3DX12_ROOT_PARAMETER1 param{};
        param.InitAsConstantBufferView(shaderRegister, registerSpace, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, visibility);
        return SetParameter(rootIndex, param);
    }

    RHIRootSignatureDesc& RHIRootSignatureDesc::SetShaderResourceView(UINT rootIndex, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility)
    {
        CD3DX12_ROOT_PARAMETER1 param{};
        param.InitAsShaderResourceView(shaderRegister, registerSpace, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, visibility);
        return SetParameter(rootIndex, param);
    }

    RHIRootSignatureDesc& RHIRootSignatureDesc::SetUnorderedAccessView(UINT rootIndex, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility)
    {
        CD3DX12_ROOT_PARAMETER1 param{};
        param.InitAsUnorderedAccessView(shaderRegister, registerSpace, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, visibility);
        return SetParameter(rootIndex, param);
    }

    RHIRootSignatureDesc& RHIRootSignatureDesc::SetDescriptorTable(UINT rootIndex, const RHIDescriptorTable& descriptorTable, D3D12_SHADER_VISIBILITY visibility)
    {
        UINT numDescriptorRanges = descriptorTable.GetNumDescriptorRanges();
        if (numDescriptorRanges == 0)
        {
            WARP_LOG_WARN("RHIRootSignatureDesc -> Tried setting empty descriptor table. Skipping");
            return *this;
        }

        CD3DX12_ROOT_PARAMETER1 param;
        param.InitAsDescriptorTable(numDescriptorRanges, descriptorTable.GetAllRanges(), visibility);
        return SetParameter(rootIndex, param);
    }

    RHIRootSignatureDesc& RHIRootSignatureDesc::Set32BitConstants(UINT rootIndex, UINT num32BitValues, UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility)
    {
        CD3DX12_ROOT_PARAMETER1 param{};
        param.InitAsConstants(num32BitValues, shaderRegister, registerSpace, visibility);
        return SetParameter(rootIndex, param);
    }

    RHIRootSignatureDesc& RHIRootSignatureDesc::AddStaticSampler(
        UINT shaderRegister,
        UINT registerSpace,
        D3D12_SHADER_VISIBILITY visibility,
        D3D12_FILTER filter,
        D3D12_TEXTURE_ADDRESS_MODE addressU,
        D3D12_TEXTURE_ADDRESS_MODE addressV,
        D3D12_TEXTURE_ADDRESS_MODE addressW,
        UINT maxAnisotropy,
        D3D12_COMPARISON_FUNC comparisonFunc,
        D3D12_STATIC_BORDER_COLOR borderColor)
    {
        CD3DX12_STATIC_SAMPLER_DESC desc;
        desc.Init(shaderRegister,
            filter,
            addressU,
            addressV,
            addressW,
            0.0f,
            maxAnisotropy,
            comparisonFunc,
            borderColor,
            0.0f,
            D3D12_FLOAT32_MAX,
            visibility,
            registerSpace);
        m_staticSamplers.push_back(desc);
        return *this;
    }

    const D3D12_ROOT_PARAMETER1* RHIRootSignatureDesc::GetRootParameters() const
    {
        WARP_ASSERT(m_numRootParameters == m_numInitRootParameters);
        return m_params.data();
    }

    const D3D12_STATIC_SAMPLER_DESC* RHIRootSignatureDesc::GetStaticSamplers() const
    {
        return m_staticSamplers.data();
    }

    RHIRootSignatureDesc& RHIRootSignatureDesc::SetParameter(UINT rootIndex, const D3D12_ROOT_PARAMETER1& param)
    {
        WARP_ASSERT(rootIndex < m_numRootParameters);
        m_params[rootIndex] = param;
        ++m_numInitRootParameters; // Increase amount of init root params
        return *this;
    }

    RHIRootSignature::RHIRootSignature(RHIDevice* device, const RHIRootSignatureDesc& desc)
        : RHIDeviceChild(device)
    {
        D3D12_ROOT_SIGNATURE_DESC1 rootDesc
        {
            .NumParameters = desc.GetNumRootParameters(),
            .pParameters = desc.GetRootParameters(),
            .NumStaticSamplers = desc.GetNumStaticSamplers(),
            .pStaticSamplers = desc.GetStaticSamplers(),
            .Flags = desc.GetFlags(),
        };

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC verDesc{};
        verDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        verDesc.Desc_1_1 = rootDesc;

        WARP_RHI_VALIDATE(D3D12SerializeVersionedRootSignature(&verDesc,
            m_D3D12BlobWithRootSignature.GetAddressOf(),
            m_D3D12BlobWithError.GetAddressOf()));

        WARP_RHI_VALIDATE(device->GetD3D12Device()->CreateRootSignature(0,
            m_D3D12BlobWithRootSignature->GetBufferPointer(),
            m_D3D12BlobWithRootSignature->GetBufferSize(),
            IID_PPV_ARGS(m_D3D12RootSignature.GetAddressOf())
        ));
    }

    void RHIRootSignature::SetName(std::wstring_view name)
    {
        WARP_SET_RHI_NAME(m_D3D12RootSignature.Get(), name);
    }

}