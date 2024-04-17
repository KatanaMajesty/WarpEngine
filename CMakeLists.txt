cmake_minimum_required(VERSION 3.20)
project(WarpEngine LANGUAGES CXX)

add_executable(WarpEngine WIN32)

# Use C++20
set_property(TARGET WarpEngine PROPERTY CXX_STANDARD 20)

# Explicitly list sources instead of globbing
# From CMake docs ->
# (We do not recommend using GLOB to collect a list of source files from your source tree. 
# If no CMakeLists.txt file changes when a source is added or removed then the generated build system cannot know 
# when to ask CMake to regenerate.)
set(WARP_SRC_DIR "${CMAKE_SOURCE_DIR}/src")

target_sources(WarpEngine
PRIVATE
    "${WARP_SRC_DIR}/WinAPI.h"
    "${WARP_SRC_DIR}/WinInput.h"
    "${WARP_SRC_DIR}/WinMain.cpp"
)

# Assets subdirectory
set(WARP_SRC_ASSETS
    "${WARP_SRC_DIR}/Assets/Importers/Formats/GltfMeshImporter.cpp"
    "${WARP_SRC_DIR}/Assets/Importers/Formats/ImageLoader.cpp"
    "${WARP_SRC_DIR}/Assets/Importers/Formats/ImageLoader.h"
    "${WARP_SRC_DIR}/Assets/Importers/AssetImporter.cpp"
    "${WARP_SRC_DIR}/Assets/Importers/AssetImporter.h"
    "${WARP_SRC_DIR}/Assets/Importers/MeshImporter.cpp"
    "${WARP_SRC_DIR}/Assets/Importers/MeshImporter.h"
    "${WARP_SRC_DIR}/Assets/Importers/TextureImporter.cpp"
    "${WARP_SRC_DIR}/Assets/Importers/TextureImporter.h"
    "${WARP_SRC_DIR}/Assets/Asset.h"
    "${WARP_SRC_DIR}/Assets/AssetManager.cpp"
    "${WARP_SRC_DIR}/Assets/AssetManager.h"
    "${WARP_SRC_DIR}/Assets/MaterialAsset.h"
    "${WARP_SRC_DIR}/Assets/MeshAsset.h"
    "${WARP_SRC_DIR}/Assets/TextureAsset.h"
)
target_sources(WarpEngine PRIVATE ${WARP_SRC_ASSETS})

# Core subdirectory
set(WARP_SRC_CORE
    "${WARP_SRC_DIR}/Core/Application.cpp"
    "${WARP_SRC_DIR}/Core/Application.h"
    "${WARP_SRC_DIR}/Core/Assert.h"
    "${WARP_SRC_DIR}/Core/Defines.h"
    "${WARP_SRC_DIR}/Core/Logger.cpp"
    "${WARP_SRC_DIR}/Core/Logger.h"
)
target_sources(WarpEngine PRIVATE ${WARP_SRC_CORE})

# Input subdirectory
set(WARP_SRC_INPUT
    "${WARP_SRC_DIR}/Input/DeviceManager.cpp"
    "${WARP_SRC_DIR}/Input/DeviceManager.h"
    "${WARP_SRC_DIR}/Input/KeyboardDevice.cpp"
    "${WARP_SRC_DIR}/Input/KeyboardDevice.h"
    "${WARP_SRC_DIR}/Input/MouseDevice.cpp"
    "${WARP_SRC_DIR}/Input/MouseDevice.h"
)
target_sources(WarpEngine PRIVATE ${WARP_SRC_INPUT})

# Math subdirectory
set(WARP_SRC_MATH
    "${WARP_SRC_DIR}/Math/Math.h"
)
target_sources(WarpEngine PRIVATE ${WARP_SRC_MATH})

# RHI src are separated for convenience
# -> Will be probably moved outside and rewritten entirely in near future
set(WARP_SRC_RHI_DIR "${WARP_SRC_DIR}/Renderer/RHI")
set(WARP_SRC_RHI
    "${WARP_SRC_RHI_DIR}/CommandAllocatorPool.cpp"
    "${WARP_SRC_RHI_DIR}/CommandAllocatorPool.h"
    "${WARP_SRC_RHI_DIR}/CommandContext.cpp"
    "${WARP_SRC_RHI_DIR}/CommandContext.h"
    "${WARP_SRC_RHI_DIR}/CommandList.cpp"
    "${WARP_SRC_RHI_DIR}/CommandList.h"
    "${WARP_SRC_RHI_DIR}/CommandQueue.cpp"
    "${WARP_SRC_RHI_DIR}/CommandQueue.h"
    "${WARP_SRC_RHI_DIR}/Descriptor.h"
    "${WARP_SRC_RHI_DIR}/DescriptorHeap.cpp"
    "${WARP_SRC_RHI_DIR}/DescriptorHeap.h"
    "${WARP_SRC_RHI_DIR}/Device.cpp"
    "${WARP_SRC_RHI_DIR}/Device.h"
    "${WARP_SRC_RHI_DIR}/DeviceChild.h"
    "${WARP_SRC_RHI_DIR}/PhysicalDevice.cpp"
    "${WARP_SRC_RHI_DIR}/PhysicalDevice.h"
    "${WARP_SRC_RHI_DIR}/PipelineState.cpp"
    "${WARP_SRC_RHI_DIR}/PipelineState.h"
    # TODO: PIX runtime should be moved out of RHI probably and be in a separate subdir
    "${WARP_SRC_RHI_DIR}/PIXRuntime.cpp"    
    "${WARP_SRC_RHI_DIR}/PIXRuntime.h"
    "${WARP_SRC_RHI_DIR}/Resource.cpp"    
    "${WARP_SRC_RHI_DIR}/Resource.h"    
    "${WARP_SRC_RHI_DIR}/ResourceTrackingContext.h"    
    "${WARP_SRC_RHI_DIR}/RootSignature.cpp"    
    "${WARP_SRC_RHI_DIR}/RootSignature.h"    
    "${WARP_SRC_RHI_DIR}/stdafx.h"    
    "${WARP_SRC_RHI_DIR}/Swapchain.cpp"    
    "${WARP_SRC_RHI_DIR}/Swapchain.h"    
    "${WARP_SRC_RHI_DIR}/ValidationLayer.cpp"    
    "${WARP_SRC_RHI_DIR}/ValidationLayer.h"    
)
target_sources(WarpEngine PRIVATE ${WARP_SRC_RHI})

# Renderer subdirectory
# -> Will be removed probably as RHI subdirectory will be moved outside and rewritten entirely
set(WARP_SRC_RENDERER
    "${WARP_SRC_DIR}/Renderer/Mesh.h"
    "${WARP_SRC_DIR}/Renderer/Renderer.cpp"
    "${WARP_SRC_DIR}/Renderer/Renderer.h"
    "${WARP_SRC_DIR}/Renderer/Shader.cpp"
    "${WARP_SRC_DIR}/Renderer/Shader.h"
    "${WARP_SRC_DIR}/Renderer/ShaderCompiler.cpp"
    "${WARP_SRC_DIR}/Renderer/ShaderCompiler.h"
    "${WARP_SRC_DIR}/Renderer/Vertex.h"
)
target_sources(WarpEngine PRIVATE ${WARP_SRC_RENDERER})

# Util subdirectory
# TODO: This subdir is very old, almost legacy. Should be refactored
set(WARP_SRC_UTIL
    "${WARP_SRC_DIR}/Util/Guid.cpp"
    "${WARP_SRC_DIR}/Util/Guid.h"
    "${WARP_SRC_DIR}/Util/String.cpp"
    "${WARP_SRC_DIR}/Util/String.h"
    "${WARP_SRC_DIR}/Util/Timer.h"
)
target_sources(WarpEngine PRIVATE ${WARP_SRC_UTIL})

# World subdirectory
set(WARP_SRC_WORLD
    "${WARP_SRC_DIR}/World/Components.h"
    "${WARP_SRC_DIR}/World/Entity.h"
    "${WARP_SRC_DIR}/World/EntityCapacitor.cpp"
    "${WARP_SRC_DIR}/World/EntityCapacitor.h"
    "${WARP_SRC_DIR}/World/EntityGraph.h"
    "${WARP_SRC_DIR}/World/World.cpp"
    "${WARP_SRC_DIR}/World/World.h"
)
target_sources(WarpEngine PRIVATE ${WARP_SRC_WORLD})

# Vendor is here
set(WARP_VENDOR_DIR         "${CMAKE_SOURCE_DIR}/vendor")
set(WARP_VENDOR_D3D12MA_DIR "${WARP_VENDOR_DIR}/D3D12MA")
set(WARP_VENDOR_DX12TK_DIR  "${WARP_VENDOR_DIR}/DirectXTK12")
set(WARP_VENDOR_DXMESH_DIR  "${WARP_VENDOR_DIR}/DirectXMesh")
set(WARP_VENDOR_DXTEX_DIR   "${WARP_VENDOR_DIR}/DirectXTex")
set(WARP_VENDOR_PIX_DIR     "${WARP_VENDOR_DIR}/WinPixEventRuntime")

set(WARP_DIRECTX_MESH_LIBRARY 
    optimized "${WARP_VENDOR_DXMESH_DIR}/Bin/x64/Release/DirectXMesh.lib" 
    debug "${WARP_VENDOR_DXMESH_DIR}/Bin/x64/Debug/DirectXMesh.lib"
)

set(WARP_DIRECTX_TEX_LIBRARY
    optimized "${WARP_VENDOR_DXTEX_DIR}/Bin/x64/Release/DirectXTex.lib" 
    debug "${WARP_VENDOR_DXTEX_DIR}/Bin/x64/Debug/DirectXTex.lib"
)

# TODO: Maybe make D3D12MA as a separate add_library?
target_sources(WarpEngine 
PRIVATE
    "${WARP_VENDOR_DIR}/DirectXTex/DDSTextureLoader/DDSTextureLoader12.cpp"
    "${WARP_VENDOR_DIR}/DirectXTex/DDSTextureLoader/DDSTextureLoader12.h"
    "${WARP_VENDOR_DIR}/DirectXTex/WICTextureLoader/WICTextureLoader12.cpp"
    "${WARP_VENDOR_DIR}/DirectXTex/WICTextureLoader/WICTextureLoader12.h"
    "${WARP_VENDOR_DX12TK_DIR}/Include/SimpleMath.h"
    "${WARP_VENDOR_D3D12MA_DIR}/D3D12MemAlloc.cpp"    
    "${WARP_VENDOR_D3D12MA_DIR}/D3D12MemAlloc.h"
)

# Add vendor as an include directory
target_include_directories(WarpEngine 
PRIVATE 
    "${WARP_VENDOR_DIR}"
    "${WARP_VENDOR_DX12TK_DIR}/Include"
    "${WARP_VENDOR_PIX_DIR}/Include"
)

add_subdirectory("${WARP_VENDOR_DIR}/entt")
add_subdirectory("${WARP_VENDOR_DIR}/spdlog")

# Resolve PIX profiling API
add_library(WinPixEventRuntime SHARED IMPORTED)
set_target_properties(WinPixEventRuntime PROPERTIES IMPORTED_LOCATION "${WARP_VENDOR_PIX_DIR}/bin/x64/WinPixEventRuntime.dll")
set_target_properties(WinPixEventRuntime PROPERTIES IMPORTED_IMPLIB   "${WARP_VENDOR_PIX_DIR}/bin/x64/WinPixEventRuntime.lib")

target_link_libraries(WarpEngine
PRIVATE
    ${WARP_DIRECTX_MESH_LIBRARY}
    ${WARP_DIRECTX_TEX_LIBRARY}
    EnTT::EnTT
    spdlog::spdlog
    WinPixEventRuntime
)

# Copy DLLs to the out path
add_custom_command(TARGET WarpEngine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:WarpEngine> $<TARGET_FILE_DIR:WarpEngine>
  COMMAND ${CMAKE_COMMAND} -E copy ${WARP_VENDOR_DIR}/dxcompiler.dll ${WARP_VENDOR_DIR}/dxil.dll $<TARGET_FILE_DIR:WarpEngine>
  COMMAND_EXPAND_LISTS
)