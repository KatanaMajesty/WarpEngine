#pragma once

#include <cstdint>

namespace Warp
{

    enum class EGraphicsApi
    {
        Vulkan
    };

    struct EngineSpecification
    {
        EGraphicsApi graphicsApi = EGraphicsApi::Vulkan;
    };

    class Engine
    {
    private:
        // Constructs invalid instance of Engine
        Engine() = default;

        // Constructs an instance of engine with provided specification
        Engine(const EngineSpecification& specification);
    
    public:
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;



    private:
        EngineSpecification m_specification;
    };

} // Warp namespace