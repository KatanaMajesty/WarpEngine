#include "nri_vk_renderer.h"

#include "common/assert.h"
#include "common/attr_defs.h"
#include "shader_factory/nri_shader_library.h"

#include <span>

namespace Warp::nri
{

    Renderer::~Renderer()
    {
        // destroy shader modules
        vkDestroyShaderModule(m_device->GetNativeHandle(), m_fsModule, nullptr);
        vkDestroyShaderModule(m_device->GetNativeHandle(), m_vsModule, nullptr);
    }

    void Renderer::Init(const RendererInfo& info)
    {
        InitInstance();
        InitSurface(info.window);
        InitDevice();
        InitSwapchain();

        // shader module initialization
        InitShaderModules();
    }

    void Renderer::InitInstance()
    {
        m_instance = Arc<vk::NriInstance>::Make(vk::NriInstanceInfo{
            .appName = "Warp test application",
            .appVersion = VK_MAKE_API_VERSION(1, 0, 0, 0),
            .engineName = "Warp Engine",
            .engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0),
            .bSurfaceRequired = true,
        });
    }

    void Renderer::InitSurface(Arc<IPlatformWindow> window)
    {
        vk::ESurfaceType surfaceType = vk::ESurfaceType::None;
        switch (window->GetType())
        {
        case EWindowImpl::Win32: surfaceType = vk::ESurfaceType::Win32; break;
        case EWindowImpl::None: WARP_A_FALLTHROUGH;
        default:
            WARP_ASSERT(false, "Unknown window implementation type specified");
        }

        m_surface = Arc<vk::NriSurface>::Make(vk::NriSurfaceInfo{
            .instance = m_instance,
            .type = surfaceType,
            .nativeHandle = window->GetNativeHandle(),
        });
    }

    void Renderer::InitDevice()
    {
        m_device = Arc<vk::NriDevice>::Make(vk::NriDeviceInfo{
            .instance = m_instance,
            .surface = m_surface,
            .bSwapchainRequired = true,
        });
    }

    void Renderer::InitSwapchain()
    {
        m_swapchain = Arc<vk::NriSwapchain>::Make(vk::NriSwapchainInfo{
            .surface = m_surface,
            .device = m_device,
            .numSwapchainImages = 3,
        }); 
    }

    void Renderer::InitShaderModules()
    {
        static auto CreateShaderModule = [device = m_device](std::span<const uint32_t> spirvWords)
        {
            auto createInfo = NRI_VK_STRUCT(VkShaderModuleCreateInfo);
            createInfo.codeSize = spirvWords.size_bytes();
            createInfo.pCode = spirvWords.data();

            VkShaderModule shaderModule;
            NRI_VK_CHECK_RESULT(vkCreateShaderModule(device->GetNativeHandle(), &createInfo, nullptr, &shaderModule),
                                "Failed to create shader module");
            return shaderModule;
        };

        ShaderLibrary* shaderLibrary = ShaderLibrary::Get();
        ShaderCompilerOutput vsHelloTriangleSpirv =
            shaderLibrary->CompileFromLibrary(EShaderLang::Slang, "hello_triangle.slang", "vertexMain", {});
        ShaderCompilerOutput fsHelloTriangleSpirv =
            shaderLibrary->CompileFromLibrary(EShaderLang::Slang, "hello_triangle.slang", "fragmentMain", {});

        m_vsModule = CreateShaderModule(vsHelloTriangleSpirv.spirvWords);
        m_fsModule = CreateShaderModule(fsHelloTriangleSpirv.spirvWords);
    }

} // Warp::nri namespace