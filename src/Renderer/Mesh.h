#pragma once

#include "../Assets/Asset.h"
#include "../Assets/AssetManager.h"

namespace Warp
{

    struct MeshInstance
    {
        AssetProxy Mesh;
        std::vector<AssetProxy> MaterialOverrides;

        Math::Matrix InstanceToWorld;
        Math::Matrix NormalMatrix;
    };

}