#include "DeviceManager.h"

namespace Warp
{

    InputDeviceManager& InputDeviceManager::Get()
    {
        static InputDeviceManager instance;
        return instance;
    }

}