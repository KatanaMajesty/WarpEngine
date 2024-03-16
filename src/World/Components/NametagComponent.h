#pragma once

#include <string>

namespace Warp
{

    // Representation of a name of an entity
    struct NametagComponent
    {
        NametagComponent() = default;
        NametagComponent(std::string_view name)
            : Nametag(name)
        {
        }

        std::string Nametag;
    };

}