cmake_minimum_required(VERSION 3.20)
project(WarpEngine LANGUAGES CXX)

add_executable(WarpEngine
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/memory/arc.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/memory/arc_object.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/assert.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/attr_defs.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/breakpoint.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/cross_log.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/cross_log.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/cross_log_colorizer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/enum_flag_set.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/enum_utils.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/print.h"

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

    "${CMAKE_CURRENT_SOURCE_DIR}/src/nebulae/nri_vk_renderer.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/src/warp_win32_entry_point.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/warp_win32_entry_point.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/win_main.cpp"
)
target_include_directories(WarpEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_property(TARGET WarpEngine PROPERTY CXX_STANDARD 26)
# With MSVC __cplusplus stays 199711L unless you enable it
if (MSVC)
  target_compile_options(WarpEngine PUBLIC /Zc:__cplusplus)
endif()
target_compile_definitions(WarpEngine PRIVATE $<$<CONFIG:Debug>:WARP_ENGINE_DEBUG>)

#include Vulkan and add necessary preprocessor directives
find_package(Vulkan REQUIRED)

target_include_directories(WarpEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(WarpEngine 
    PRIVATE
        Vulkan::Vulkan
)
target_compile_definitions(WarpEngine PUBLIC VK_USE_PLATFORM_WIN32_KHR)

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
add_dependencies(WarpEngine 
    generate_vk_stype_helper
    generate_pci_vendor_list
)
