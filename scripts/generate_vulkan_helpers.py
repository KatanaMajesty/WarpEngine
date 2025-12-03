#!/usr/bin/env python3

import argparse
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import List

import vulkan_registry_common as VkRegistryHelper
import vulkan_helper_templates as VkTemplates
import vulkan_structure_type_mapping as VkSTypeHelperGenerator
import vulkan_object_type_mapping as VkObjTypeHelperGenerator


def main(argv: List[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Generate a header that maps Vk* structs to their VkStructureType constants (Windows build)")
    parser.add_argument("--vulkan_sdk", dest="sdk", help="Path to a Vulkan SDK root or directly to vk.xml")
    parser.add_argument("--apis", action="append", dest="supported_apis", default=['vulkan'], help="Specify a comma-separated API list to look-up extensions in Vulkan registry. For example: 'vulkan,vulkansc'")
    parser.add_argument("--vk_stype_helper_output", default=None, help="Output path for Vulkan sType helper mapping type_traits header")
    parser.add_argument("--vk_objtype_helper_output", default=None, help="Output path for Vulkan object type helper mapping type_traits header")
    ns = parser.parse_args(argv)

    xml_path = VkRegistryHelper.locate_vk_xml(Path(ns.sdk))
    tree = ET.parse(xml_path)
    supported_apis = list(ns.supported_apis)

    if ns.vk_stype_helper_output is not None:
        VkTemplates.render_header(VkTemplates.WARP_HEADER_VK_STRUCTURE_TYPE_TRAIT, 
            output=Path(ns.vk_stype_helper_output).resolve(), 
            xml_path=xml_path,
            mappings=VkSTypeHelperGenerator.query_generation_mappings(tree, supported_apis=supported_apis)
        )

    handle_information = VkObjTypeHelperGenerator.query_generation_mappings(tree, supported_apis=supported_apis)
    if ns.vk_objtype_helper_output is not None:
        VkTemplates.render_header(VkTemplates.WARP_HEADER_VK_OBJECT_TYPE_TRAIT,
            output=Path(ns.vk_objtype_helper_output).resolve(),
            xml_path=xml_path,
            mappings=handle_information
        )

if __name__ == "__main__":
    main()
