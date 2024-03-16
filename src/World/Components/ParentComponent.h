#pragma once

#include "../Entity.h"

namespace Warp
{

    struct ParentComponent
    {
        ParentComponent() = default;
        ParentComponent(Entity parent)
            : Parent(parent)
        { 
        }

        Entity Parent;
    };

}