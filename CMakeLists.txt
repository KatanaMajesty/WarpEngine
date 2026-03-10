# CMake 3.28 is required as some features from Vulkan SDK require CMake 3.28+
cmake_minimum_required(VERSION 3.28)
project(warp_engine LANGUAGES CXX)

add_executable(warp_engine
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/memory/arc.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/assert.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/attr_defs.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/breakpoint.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/cross_log.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/cross_log.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/cross_log_colorizer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/enum_flag_set.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/enum_utils.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/filesystem_io.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/filesystem_io.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/print.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/shader_factory/nri_shader_compiler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/shader_factory/nri_shader_compiler.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/shader_factory/nri_shader_library.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/shader_factory/nri_shader_library.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/shader_factory/nri_slang_util.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/nri_vk_device.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/nri_vk_device.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/nri_vk_instance.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/nri_vk_instance.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/nri_vk_memory_safety.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/nri_vk_physical_device.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/nri_vk_physical_device.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/nri_vk_surface.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/nri_vk_surface.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/nri_vk_swapchain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/nri_vk_swapchain.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/nri_vk_renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/nri_vk_renderer.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform_window.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform_window.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform_win32_window.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform_win32_window.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/warp_win32_entry_point.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/warp_win32_entry_point.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/win_main.cpp"
)
target_include_directories(warp_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_property(TARGET warp_engine PROPERTY CXX_STANDARD 26)
# With MSVC __cplusplus stays 199711L unless you enable it
if (MSVC)
  target_compile_options(warp_engine PUBLIC /Zc:__cplusplus)
endif()
target_compile_definitions(warp_engine PRIVATE $<$<CONFIG:Debug>:WARP_ENGINE_DEBUG>)

# warp_engine's development cycle heavily relies on latest Vulkan SDK
# Below is a setup of all such libraries and packages

# include Vulkan and add necessary components
#
# This CMake page describes all supported Vulkan SDK components currently availble to the build system
# https://cmake.org/cmake/help/latest/module/FindVulkan.html
#
# Try to specify all required components here in FindVulkan, 
# but some libraries are still not available here, such as Slang, which Warp relies on, so these exceptions are handled manually
find_package(Vulkan REQUIRED COMPONENTS SPIRV-Tools)

# Find and include Slang as a main warp_engine's shading language
# Slang is currently not a part of FindVulkan so see a link below on how to resolve
# https://docs.vulkan.org/tutorial/latest/02_Development_environment.html#_cmake 
find_package(Slang CONFIG HINTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/slang/cmake")
get_target_property(Slang_INCLUDE_DIRS slang::slang INTERFACE_INCLUDE_DIRECTORIES)

target_include_directories(warp_engine PRIVATE 
    ${Vulkan_INCLUDE_DIRS}
    ${Slang_INCLUDE_DIRS}) # we also need to manually add Slang include dirs as it is not a part of FindVulkan
target_link_libraries(warp_engine 
    PRIVATE
        Vulkan::Vulkan
        Vulkan::SPIRV-Tools
        slang::slang
)
target_compile_definitions(warp_engine PUBLIC VK_USE_PLATFORM_WIN32_KHR)

# copy Slang DLL
add_custom_command(TARGET warp_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:slang::slang> 
        $<TARGET_FILE_DIR:warp_engine>
)

# Always copy on every build (even if warp_engine didn’t rebuild)
add_custom_target(warp_engine_nebulae_shaders ALL 
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/shader_library" $<TARGET_FILE_DIR:warp_engine>
    COMMENT "Copying nebulae shaders to output dir...")
add_dependencies(warp_engine warp_engine_nebulae_shaders)

# 3. Execute Python scripts
find_package(Python REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python_EXECUTABLE} -m pip install jinja2)

# Generate PCI Vendor ID list
set(GENERATE_SCRIPT_NRI_PCI_VENDOR_LIST         "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_pci_id_definitions.py")
set(GENERATE_OUTPUT_HEADER_NRI_PCI_VENDOR_LIST  "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/generated/nri_pci_vendor_ids.h")
add_custom_command(
    OUTPUT  "${GENERATE_OUTPUT_HEADER_NRI_PCI_VENDOR_LIST}"
    COMMAND "${Python_EXECUTABLE}" "${GENERATE_SCRIPT_NRI_PCI_VENDOR_LIST}" -o "${GENERATE_OUTPUT_HEADER_NRI_PCI_VENDOR_LIST}"
    DEPENDS "${GENERATE_SCRIPT_NRI_PCI_VENDOR_LIST}"
    COMMENT "Generating PCI VendorID definitions header"
    VERBATIM
)
# Generate Vulkan sType helper
set(GENERATE_SCRIPT_VULKAN_HELPERS                  "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_vulkan_helpers.py")
set(GENERATE_OUTPUT_HEADER_VK_STYPE_HELPER          "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/generated/nri_vk_stype_helper.h")
set(GENERATE_OUTPUT_HEADER_VK_OBJTYPE_HELPER        "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/vulkan/generated/nri_vk_object_type_helper.h")
add_custom_command(
    OUTPUT  
        "${GENERATE_OUTPUT_HEADER_VK_STYPE_HELPER}" 
        "${GENERATE_OUTPUT_HEADER_VK_OBJTYPE_HELPER}"
    COMMAND "${Python_EXECUTABLE}" "${GENERATE_SCRIPT_VULKAN_HELPERS}" 
        --vulkan_sdk "$ENV{VULKAN_SDK}" 
        --vk_stype_helper_output "${GENERATE_OUTPUT_HEADER_VK_STYPE_HELPER}"
        --vk_objtype_helper_output "${GENERATE_OUTPUT_HEADER_VK_OBJTYPE_HELPER}"
    DEPENDS "${GENERATE_SCRIPT_VULKAN_HELPERS}"
    COMMENT "Generating Vulkan structure helper header"
    VERBATIM
)

add_custom_target(generate_vk_stype_helper ALL DEPENDS "${GENERATE_OUTPUT_HEADER_VK_STYPE_HELPER}")
add_custom_target(generate_pci_vendor_list ALL DEPENDS "${GENERATE_OUTPUT_HEADER_NRI_PCI_VENDOR_LIST}")
add_dependencies(warp_engine 
    generate_vk_stype_helper
    generate_pci_vendor_list
)

# Always enable testing
enable_testing()
add_subdirectory(tests)