#pragma once

namespace Warp
{

    // Large scale incoming... 15.03.24
    // TODO: We chill here for now -> https://poniesandlight.co.uk/reflect/island_rendergraph_1/

    struct FrameGraphNode
    {

    };

    class FrameGraph
    {
    public:
        FrameGraph() = default;

    private:
        FrameGraphNode m_RootPass;
    };

}