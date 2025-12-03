#pragma once

#include "common/memory/arc_object.h"

namespace Warp::nri
{

    // TODO: Replace this with a render graph after a triangle is rendered
    class Renderer : public AtomicallyRefCounted<Renderer>
    {
    public:
        Renderer() = default;

        void NextFrame()
        {
        
        }

    private:
        uint32_t m_frameIndex;
    };

} // Warp::nri namespace