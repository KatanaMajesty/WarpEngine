import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from typing import Optional, Dict, List, Set, Tuple
from pathlib import Path

import vulkan_registry_common as VkRegistry

@dataclass
class HandleInformation:
    """
    contains the information about handle, such as its respective VkObjectType enum value, corresponding extension,
    its C++ type, etc.
    """
    common_handle_info: VkRegistry.RegistryCommonHandleInformation
    extension:          Optional[str] = None # specifies the extension handle is added in, or None if in Vulkan Core
    platform_guard:     Optional[str] = None # specifies a platform guard for this handle to be defined with

    # these string rely on Vulkan specification and naming rules defined in https://registry.khronos.org/vulkan/specs/latest/styleguide.html
    # 2.6.2. Command Verbs ("Destroy": destroys an object - paired with "Create")
    # 2.6.3. Function Pointer Type Names (Function pointer names are declared exactly as the equivalent statically declared command would be declared, but prefixed with PFN_)
    #create_function:    Optional[VkRegistry.RegistryHandleLifetimeCommand] = None # optionally, a command that would create a handle
    #destroy_function:   Optional[VkRegistry.RegistryHandleLifetimeCommand] = None # optionally, a command that would destroy the handle

    @property
    def platform_guard_open(self) -> str:
        """`#ifdef GUARD` line or empty string."""
        return f"#ifdef {self.platform_guard}\n" if self.platform_guard else ""

    @property
    def platform_guard_close(self) -> str:
        """`#endif // GUARD` line or empty string."""
        return f"#endif // {self.platform_guard}\n" if self.platform_guard else ""

    @property
    def extension_comment(self) -> str:
        """returns simple extension comment"""
        return f"// Provided by {self.extension}" if self.extension else ""

    #@property
    #def create_pfn(self) -> str:
    #    """
    #    from https://registry.khronos.org/vulkan/specs/latest/styleguide.html
    #    2.6.3. Function Pointer Type Names (Function pointer names are declared exactly as the equivalent statically declared command would be declared, but prefixed with PFN_)
    #    """
    #    return "PFN_" + self.create_function.name if self.create_function.name else ""
    
    #@property
    #def destroy_pfn(self) -> str:
    #    """
    #    from https://registry.khronos.org/vulkan/specs/latest/styleguide.html
    #    2.6.3. Function Pointer Type Names (Function pointer names are declared exactly as the equivalent statically declared command would be declared, but prefixed with PFN_)
    #    """
    #    return "PFN_" + self.destroy_function.name if self.destroy_function.name else ""

    def get_enum(self) -> str:
        return self.common_handle_info.handle_enum
    
    def get_type(self) -> str:
        return self.common_handle_info.handle_type
    
    def get_parent_type(self) -> str:
        return self.common_handle_info.handle_parent_type
    
    #def get_proc_addr_scope(self, name: str) -> str:
    #    """
    #    based on common_info's parent type returns either:
    #        'global'    - if function scope does not rely neither on VkInstance nor on VkDevice
    #        'instance'  - if function scope relies on VkInstance
    #        'device'    - if function scope relies on VkDevice
    #    """
    #    if self.common_handle_info.handle_parent_type is None:
    #        return 'global'
    #    elif self.common_handle_info.handle_parent_type == 'VkInstance' or \
    #        self.common_handle_info.handle_parent_type == 'VkPhysicalDevice' or \
    #        self.common_handle_info.handle_parent_type == 'VkDisplayKHR' or \
    #        name == 'vkDestroyInstance':
    #        return 'instance'
    #    else:
    #        return 'device'
                
    def is_dispatchable(self) -> bool:
        return self.common_handle_info.is_dispatchable
    
    def __str__(self) -> str:
        g = f", platform_guard={self.platform_guard}" if self.platform_guard else ""
        e = f", ext={self.extension}" if self.extension else ""
        return f"{self.__class__.__name__}(handle_type={self.handle_type}, dispatchable={self.is_dispatchable}, enum={self.handle_enum}{e}{g})"
        


def query_generation_mappings(
    tree: ET.Element,
    supported_apis: List[str]) -> List[HandleInformation]:

    registry_platform_guard_map: Dict[str, str] = VkRegistry.enumerate_platform_guards(tree)
    registry_api_extensions = VkRegistry.enumerate_api_extensions(tree, supported_apis)
    registry_extension_object_type_enums: Dict[str, ET.Element] = VkRegistry.enumerate_api_extension_object_type_enums(tree, registry_api_extensions, supported_apis)
    registry_api_core_object_type_enums: Set[str] = VkRegistry.enumerate_api_core_object_type_enums(tree)
    registry_api_common_handle_info: List[VkRegistry.RegistryCommonHandleInformation] = VkRegistry.enumerate_api_common_handle_information(tree)

    registry_handle_lifetime_commands: Dict[str, List[VkRegistry.RegistryHandleLifetimeCommand]] = VkRegistry.enumerate_api_handle_lifetime_commands(tree)

    handle_information_array: List[HandleInformation] = []
    for common_handle_info in registry_api_common_handle_info:
        # firstly, create handle information object using data we currently have
        handle_info = HandleInformation(common_handle_info=common_handle_info)
        # check whether or not this handle enum is added by an extension
        # and if so, check if the extension is supported
        if handle_info.get_enum() in registry_extension_object_type_enums:
            ext: ET.Element = registry_extension_object_type_enums[handle_info.get_enum()]
            ext_name = ext.attrib['name']
            handle_info.extension = ext_name
            if 'platform' in ext.attrib:
                platform_guard = registry_platform_guard_map[ext.attrib['platform']]
                handle_info.platform_guard = platform_guard
        elif handle_info.get_enum() not in registry_api_core_object_type_enums:
            # if handle enum is not in supported extension list AND is not a part of Core API - skip it
            continue

        # now we know that this handle type is certainly supported, thus we can try and look for associated create/destroy commands
        # if handle_info.get_type() in registry_handle_lifetime_commands:
        #     lifetime_cmds = registry_handle_lifetime_commands[handle_info.get_type()]
        #     for c in lifetime_cmds:
        #         if c.is_create_command:
        #             handle_info.create_function = c
        #         elif c.is_destroy_command:
        #             handle_info.destroy_function = c

        # lastly, add handle information to result array
        handle_information_array.append(handle_info)

    return handle_information_array
