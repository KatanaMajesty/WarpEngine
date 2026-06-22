# WARP ENGINE

## HOW TO BUILD
In order to build the project you need to install latest LunarG Vulkan SDK (with GLM, VMA and SDL components enabled)

### Third-party dependencies

Third-party dependencies are mostly maintained using local `{root}/vendor` directory (for small dependencies) or via Cmake Package Manager (CPM):

- [CPM](https://github.com/cpm-cmake/cpm.cmake)
- [Nlohmann's JSON](https://github.com/nlohmann/json) via CPM
- [ImGui](https://github.com/ocornut/imgui) via CPM
- [ImPlot](https://github.com/epezent/implot) via CPM
- [EnTT](https://github.com/skypjack/entt) via CPM
- [slang](https://github.com/shader-slang/slang) under `vendor`
- [cgltf](https://github.com/jkuhlmann/cgltf) under `vendor`
- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) (with GLM, VMA and SDL) via manual setup on host

