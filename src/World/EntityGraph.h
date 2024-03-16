#pragma once

#include "Entity.h"

namespace Warp
{

    struct EntityNode
    {
        Entity EntityOfNode;
        EntityNode* NextNode = nullptr;
    };

    class EntityGraph
    {

    };

}