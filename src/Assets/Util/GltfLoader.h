#pragma once

#include <string>
#include <vector>

#include <string>

#include "../Asset.h"
#include "../AssetManager.h"

namespace Warp::GltfLoader
{

	std::vector<AssetProxy> LoadFromFile(std::string_view filepath, AssetManager* assetManager);

}