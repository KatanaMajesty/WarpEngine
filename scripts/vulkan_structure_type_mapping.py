import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from typing import Optional, Dict, List
from pathlib import Path

import vulkan_registry_common as VkRegistry


@dataclass(order=True, slots=True)
class StructureTypeMapping:
    """
    Holds the information that ties a Vulkan VkStructureType enum value
    (`sType`) to its corresponding struct name, plus an optional protection
    macro (e.g. VK_ENABLE_BETA_EXTENSIONS).
    """
    _sort_key: tuple[str, str] = field(init=False, repr=False, compare=True)
    s_type: str
    struct: str
    guard: Optional[str] = None

    def __post_init__(self) -> None:
        # Alphabetical order of mappings by struct name
        object.__setattr__(self, "_sort_key", (self.struct.lower(), self.s_type))

    @property
    def has_guard(self) -> bool:
        """True if a guard macro is specified."""
        return bool(self.guard)

    @property
    def guard_open(self) -> str:
        """`#ifdef GUARD` line or empty string."""
        return f"#ifdef {self.guard}\n" if self.guard else ""

    @property
    def guard_close(self) -> str:
        """`#endif // GUARD` line or empty string."""
        return f"#endif // {self.guard}\n" if self.guard else ""

    def __str__(self) -> str:
        g = f", guard={self.guard}" if self.guard else ""
        return f"{self.__class__.__name__}(sType={self.s_type}, struct={self.struct}{g})"
    

def query_structure_type_mappings(
    root: ET.Element,
    stype_mapping: Dict[str, str], 
    api_core_stype_vals: List[str],             # a simple list of core sType values
    api_extension_stype_vals: Dict[str, str],   # a dict of sType values with their corresponding extension name
    platform_guard_map: Dict[str, str]
) -> List[StructureTypeMapping]:
    
    mappings: List[StructureTypeMapping] = []
    for s_type, object_type in stype_mapping.items():
        m = StructureTypeMapping(
                s_type=s_type,
                struct=object_type
        )
        if s_type in api_extension_stype_vals:
            ext_name = api_extension_stype_vals[s_type]
            extensions = root.findall(f"./extensions/extension[@name='{ext_name}']")
            assert len(extensions) == 1, "Extensions must only be defined once inside of Vulkan registry. Make sure your registry is valid"

            extension: ET.Element = extensions[0]
            if "platform" in extension.attrib:
                platform = extension.attrib["platform"]
                assert platform in platform_guard_map, "Platform should be defined in registry"
                m.guard = platform_guard_map[platform]
            mappings.append(m)
        elif s_type in api_core_stype_vals:
            mappings.append(m)
        else:
            continue
    return mappings


def query_generation_mappings(
    tree: ET.Element,
    supported_apis: List[str]) -> List[StructureTypeMapping]:
    """Return an ordered mapping: struct name -> `VK_STRUCTURE_TYPE_*`."""

    root = tree.getroot()

    # enumerate platform guards (macro defines) for each registered platform that is supported in given Vulkan registry
    registry_platform_guard_map = VkRegistry.enumerate_platform_guards(root)

    # enumerate all supported API extensions for the given Vulkan registry
    registry_api_extensions: List[ET.Element] = VkRegistry.enumerate_api_extensions(root, supported_apis)

    # we now need to enumerate every core API and extension stype values for specified 'supported_apis' and 'api_extensions', so that we could use values later
    registry_api_core_stype_vals: List[str]           = VkRegistry.enumerate_api_core_structs(root, supported_apis)
    registry_api_extension_stype_vals: Dict[str, str] = VkRegistry.enumerate_api_extension_structs(root, registry_api_extensions, supported_apis)
    registry_stype_mapping: Dict[str, str]            = VkRegistry.query_stype_map(root)

    mappings = query_structure_type_mappings(
        root=root,
        stype_mapping=registry_stype_mapping,
        api_core_stype_vals=registry_api_core_stype_vals,
        api_extension_stype_vals=registry_api_extension_stype_vals,
        platform_guard_map=registry_platform_guard_map)

    return mappings
