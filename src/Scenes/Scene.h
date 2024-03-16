#pragma once

#include <vector>

#include "../World/World.h"

#include "../Assets/AssetManager.h"
#include "../Assets/Importers/MeshImporter.h"
#include "../Assets/Importers/TextureImporter.h"

namespace Warp
{

    class Scene
    {
    public:
        Scene(AssetManager* assetManager)
            : m_AssetManager(assetManager)
        {
        }

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        Scene(Scene&&) = delete;
        Scene& operator=(Scene&&) = delete;

        World& GetWorld() { return m_World; }

    private:
        AssetManager*   m_AssetManager;
        MeshImporter    m_MeshImporter;
        TextureImporter m_TextureImporter;

        World m_World;
    };

}