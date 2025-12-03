import sys
import xml.etree.ElementTree as ET
from typing import List, Dict, Set, Tuple, Optional
from pathlib import Path
from dataclasses import dataclass


def locate_vk_xml(path: Path) -> Path:
    path = path.expanduser().resolve()
    if path.is_file():
        return path
    for candidate in [
        path / "share" / "vulkan" / "registry" / "vk.xml",
    ]:
        if candidate.is_file():
            return candidate
    sys.exit(f"error: could not find vk.xml under {path}")


def check_api_support(supported_apis: List[str], api_list: List[str]) -> bool:
    """
    check whether api_list contains at least one API from supported_apis
    """
    # check for API support
    return any(api.lower() in api_list for api in supported_apis)


def enumerate_platform_guards(root: ET.Element) -> Dict[str, str]:
    """
    returns a str-to-str Dict of platform C++ mappings for protection guards of a header
    for example, platform="screen" would have a protect macro definition "VK_USE_PLATFORM_SCREEN_QNX", which needs to be accounted for
    """
    # <platforms comment="Vulkan platform names, reserved for use with platform- and window system-specific extensions">
    platforms = root.findall("./platforms/platform")
    platform_map: Dict[str, str] = {}
    for p in platforms:
        assert "name" in p.attrib and "protect" in p.attrib
        platform_map[p.attrib["name"]] = p.attrib["protect"]

    return platform_map


def enumerate_api_extensions(root: ET.Element, supported_apis: List[str]) -> List[ET.Element]:
    """
    returns a Dict of supported API extensions (Based of provided 'supported_apis' string array)
    resulting Dict can be used to check for extension-specific guards in registry

    notice: this only checks for supported APIs, not for supported platforms. Idea is to stay cross-platform
    """
    # <extensions comment="Vulkan extension interface definitions">
    # this returns ALL extensions, including those that are not supported in Vulkan. This additional checks are done later on
    extensions = root.findall("./extensions/extension")

    supported_extensions: List[ET.Element] = []
    for ext in extensions:
        ext_name = ext.attrib["name"]
        ext_apis = ext.attrib["supported"] # supported - comma-separated list of required API names for which this extension is defined.
        api_list = ext_apis.split(",")

        if check_api_support(supported_apis, api_list):
            supported_extensions.append(ext)

    return supported_extensions


def enumerate_api_core_structs(root: ET.Element, supported_apis: List[str]) -> List[str]:
    """
    returns a list of all **required** core API structure types (sTypes, VK_STRUCTURE_TYPE_), those that were ratified to be in standard of ONE of APIs in 'supported_apis'
    """
    # <feature api="vulkan,vulkansc" name="VK_VERSION_1_0" number="1.0" comment="Vulkan core API interface definitions">
    # <feature api="vulkan,vulkansc" name="VK_VERSION_1_1" number="1.1" comment="Vulkan 1.1 core API interface definitions.">
    # <feature api="vulkan,vulkansc" name="VK_VERSION_1_2" number="1.2" comment="Vulkan 1.2 core API interface definitions.">
    # <feature api="vulkan,vulkansc" name="VK_VERSION_1_3" number="1.3" comment="Vulkan 1.3 core API interface definitions.">

    # This one differs a bit, for instance if we look at 
    #
    # <require comment="Promoted from VK_EXT_subgroup_size_control (STDPROMOTE/PROPLIMCHANGE) (extension 226)">
    #     <type name="VkPhysicalDeviceSubgroupSizeControlFeatures"/>
    #     <type name="VkPhysicalDeviceSubgroupSizeControlProperties"/>
    #     <type name="VkPipelineShaderStageRequiredSubgroupSizeCreateInfo"/>
    #     <enum offset="0" extends="VkStructureType"  extnumber="226"         name="VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES"/>
    #     <enum offset="1" extends="VkStructureType"  extnumber="226"         name="VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO"/>
    #     <enum offset="2" extends="VkStructureType"  extnumber="226"         name="VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES"/>
    #     <enum bitpos="0" extends="VkPipelineShaderStageCreateFlagBits"      name="VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT"/>
    #     <enum bitpos="1" extends="VkPipelineShaderStageCreateFlagBits"      name="VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT"/>
    # </require>
    #
    # we can see that every sType would 'extend' from VkStructureType and have a corresponding sType name
    all_features = root.findall("./feature")

    # from all features we need to sift down those, unrelated to supported_apis list
    supported_features: List[ET.Element] = []
    for f in all_features:
        assert "api" in f.attrib, "API must be speicifed as a part of Vulkan registry's feature"
        feature_apis = f.attrib["api"]
        api_list = feature_apis.split(",")

        # check for API support
        if check_api_support(supported_apis, api_list):
            supported_features.append(f)    

    # after sifting out 'unsupported' core features, iterate over each and seek for VkStructureType
    struct_types: List[str] = []
    for f in supported_features:
        requirements = f.findall("./require")
        for r in requirements:
            # every VK_STRUCTURE_TYPE_ value would be an enum, so we seek for those
            enums = r.findall("./enum[@extends='VkStructureType']")
            for e in enums:
                assert "name" in e.attrib, "Attributes of VkStructureType enum should have a value name"
                struct_types.append(e.attrib["name"])

    # also add <enums name="VkStructureType" type="enum" comment="Structure type enumerant"> as they are not a part of feature, but of enums
    core_enum_vals = root.findall("./enums[@name='VkStructureType']")
    for enums in core_enum_vals:
        evals = enums.findall("./enum")
        for e in evals:
            assert "name" in e.attrib, "Attributes of VkStructureType enum should have a value name"
            struct_types.append(e.attrib["name"])

    return struct_types


def enumerate_api_core_object_type_enums(root: ET.Element) -> Set[str]:
    """
    returns a set of all VkObjectType enum values from Vulkan Core that are defined within the registry
    combining this with all supported extension VkObjectType values (obtained by calling enumerate_api_extension_object_type_enums) should provide
            with a full list of handle types defined within Vulkan specification for current device
    """
    # <enums name="VkObjectType" type="enum" comment="Enums to track objects of various types - also see objtypeenum attributes on type tags">
    #
    # for now in Vulkan all VkObejctTypes from core API are defined in <enums></enums> but we might also want to eventually account for <require>
    obj_types: List[str] = []
    core_obj_type_enum_vals = root.findall("./enums[@name='VkObjectType']")
    for enums in core_obj_type_enum_vals:
        evals = enums.findall("./enum")
        for e in evals:
            assert "name" in e.attrib, "Attributes of VkObjectType enum should have a value name"
            obj_types.append(e.attrib["name"])
    return set(obj_types)


def enumerate_api_extension_structs(root: ET.Element, api_extensions: List[ET.Element], supported_apis: List[str]) -> Dict[str, str]:
    """
    returns a str-to-str dict of all extension-based VkStructureType values with its corresponding extension name VK_EXT/VK_KHR from api_extensions list
    """
    struct_types: Dict[str, str] = {}
    for ext in api_extensions:
        ext_name = ext.attrib["name"]
        requirements = ext.findall("./require")
        for r in requirements:
            # check whether requirements have API-specific needs, and if not satisfied - bail
            if "api" in r.attrib:
                api_list_r = r.attrib["api"]
                api_list_r = api_list_r.split(",")
                if not check_api_support(supported_apis, api_list_r):
                    continue
            # every VK_STRUCTURE_TYPE_ value would be an enum, so we seek for those
            enums = r.findall("./enum[@extends='VkStructureType']")
            for e in enums:
                assert "name" in e.attrib, "Attributes of VkStructureType enum should have a value name"
                struct_types[e.attrib["name"]] = ext_name
    return struct_types


def enumerate_api_extension_object_type_enums(root: ET.Element, api_extensions: List[ET.Element], supported_apis: List[str]) -> Dict[str, ET.Element]:
    """
    returns a str-to-XmlElement dict of all extension-based VkObjectType values with its corresponding extension XML element from api_extension list
    notice: resulting dict would only store VkObjectType values from extensions that are supported for specified API list 
            even though api_extension would already be checking for that there are cases where specific elements/types have separate requirements
    """
    obj_types: Dict[str, ET.Element] = {}
    for ext in api_extensions:
        ext_name = ext.attrib['name']
        requirements = ext.findall("./require")
        for r in requirements:
            # check whether requirements have API-specific needs, and if not satisfied - bail
            if "api" in r.attrib:
                api_list_r = r.attrib["api"]
                api_list_r = api_list_r.split(",")
                if not check_api_support(supported_apis, api_list_r):
                    continue
            # every VK_OBJECT_TYPE_ value would be an enum, so we seek for those
            enums = r.findall("./enum[@extends='VkObjectType']")
            for e in enums:
                assert "name" in e.attrib, "Attributes of VkObjectType enum should have a value name"
                obj_types[e.attrib["name"]] = ext
    return obj_types


def query_stype_map(root: ET.Element) -> Dict[str, str]:
    """
    returns a str-to-str mapping of stype enum value to its corresponding Vulkan structure type
    """
    # <types comment="Vulkan type definitions">
    all_structs = root.findall("./types/type[@category='struct']")
    mapping: Dict[str, str] = {}
    for s in all_structs:
        # name - optional. Name of this type (if not defined in the tag body).
        if "name" not in s.attrib:
            continue # skip if no name - nothing to map against

        s_name = s.attrib["name"]
        s_type_members = [member for member in s.findall("./member") if member.findtext("type") == "VkStructureType"]
        if not s_type_members:
            continue

        # reduce s_type members to 1 and assume so
        assert len(s_type_members) == 1, "Cannot be more than 1 sType member in structure type. Double-check whether your specified Vulkan registry is correct"
        s_type_member = s_type_members[0]

        # values - only valid on the sType member of a struct. This is a comma-separated list of enumerant values that are valid for the structure type; 
        # usually there is only a single value.
        if "values" not in s_type_member.attrib:
            continue # this is weird, but needed for some reason

        s_type = s_type_member.attrib["values"].split(",")[0] # take the very first
        if s_type != s_type_member.attrib["values"]:
            print(f"Extracted sType {s_type} is not equal to actual node's values ({s_type_member.attrib["values"]})'")

        mapping[s_type] = s_name

    return mapping


def query_objtype_map(root: ET.Element) -> Dict[str, Tuple[str, bool]]:
    """
    returns a str-to-str mapping of Vulkan handle T to its corresponding VkObjectType enum value 
    boolean value in a tuple would correspond to whether or not this handle is dispatchable
    """
    # <types comment="Vulkan type definitions">
    # <type category="handle" parent="VkInstance" objtypeenum="VK_OBJECT_TYPE_PHYSICAL_DEVICE"><type>VK_DEFINE_HANDLE</type>(<name>VkPhysicalDevice</name>)</type> 
    # </types>
    all_handles = root.findall("./types/type[@category='handle']")
    mapping: Dict[str, str] = {}
    for h in all_handles:
        if not 'objtypeenum' in h.attrib:
            continue
        handle_type = h.find('name').text
        handle_enum = h.attrib['objtypeenum']
        is_dispatchable = True if h.find('type').text == 'VK_DEFINE_HANDLE' else False # else assumed to be VK_DEFINE_NON_DISPATCHABLE_HANDLE
        assert is_dispatchable or h.find('type').text == 'VK_DEFINE_NON_DISPATCHABLE_HANDLE'

        mapping[handle_type] = (handle_enum, is_dispatchable)
    return mapping


@dataclass
class RegistryCommonHandleInformation:
    """
    represents data extracted from <type category="handle"> from vk.xml
    """
    handle_type: str
    handle_enum: str                            # Maps directly to VkObjectType enum value
    is_dispatchable: bool                       # Whether the handle was defined with VK_DEFINE_HANDLE or VK_DEFINE_NON_DISPATCHABLE_HANDLE
    handle_parent_type: Optional[str] = None    # specifies handle's parent type defined as 'parent' in <types> XML scope


def enumerate_api_common_handle_information(root: ET.Element) -> List[RegistryCommonHandleInformation]:
    """
    enumerates all handles stored inside of Vulkan registry
    this function does not account for platform-specific or extension-specific details and just returns common information about handle
    """
    # <types comment="Vulkan type definitions">
    # <type category="handle" parent="VkInstance" objtypeenum="VK_OBJECT_TYPE_PHYSICAL_DEVICE"><type>VK_DEFINE_HANDLE</type>(<name>VkPhysicalDevice</name>)</type> 
    # </types>
    all_handles = root.findall("./types/type[@category='handle']")
    handle_info_array: List[RegistryCommonHandleInformation] = []
    for handle in all_handles:
        if not 'objtypeenum' in handle.attrib:
            continue

        handle_info = RegistryCommonHandleInformation(
            handle_type = handle.findtext('name'),
            handle_enum = handle.attrib['objtypeenum'],
            is_dispatchable=True if handle.find('type').text == 'VK_DEFINE_HANDLE' else False # else assumed to be VK_DEFINE_NON_DISPATCHABLE_HANDLE
        )
        assert handle_info.is_dispatchable or handle.find('type').text == 'VK_DEFINE_NON_DISPATCHABLE_HANDLE'
        if 'parent' in handle.attrib:
            # parent handle type is present - add it to common information
            handle_info.handle_parent_type = handle.attrib['parent']

        handle_info_array.append(handle_info)

    # patch handle info
    for handle in handle_info_array:
        if handle.handle_parent_type == 'VkPhysicalDevice':
            # remove VkPhysicalDevice parent type, as it doesnt really handle much
            handle.handle_parent_type = None

    return handle_info_array


@dataclass
class RegistryHandleLifetimeCommand:
    name: str # 16.3.2. Contents of proto Tags - The name tag is required, and contains the command name being described.
    handle_type: str # as this command is a handle command, this is not an optional field
    parent_handle_type: Optional[str] = None # optionally, a parent handle type, that manages the lifetime of handle
    is_create_command: bool = False
    is_destroy_command: bool = False

    def __str__(self) -> str:
        return f"{self.__class__.__name__}(command={self.name}, handle_type={self.handle_type}, is_create={self.is_create_command})"


def enumerate_api_handle_lifetime_commands(root: ET.Element) -> Dict[str, List[RegistryHandleLifetimeCommand]]:
    """
    returns a str-to-RegistryHandleLifetimeCommand dict that maps Vulkan handle types to its corresponding list of lifetime commands (should only be create/destroy pair)
    This function relies on style guiding rules from Vulkan specification (https://registry.khronos.org/vulkan/specs/latest/styleguide.html)
    2.6.2. Command Verbs ('Destroy': destroys an object - paired with "Create")
    2.6.3. Function Pointer Type Names (Function pointer names are declared exactly as the equivalent statically declared command would be declared, but prefixed with PFN_)
    """
    # <commands comment="Vulkan command definitions">
    commands: Dict[str, List[RegistryHandleLifetimeCommand]] = {}
    all_commands = root.findall("./commands/command")
    for c in all_commands:
        # 16.2. Contents of command Tags (https://registry.khronos.org/vulkan/specs/latest/registry.html#_contents_of_command_tags)
        # proto is required and must be the first element. It is a tag defining the C function prototype of a command 
        proto = c.find('proto')
        if not proto:
            continue

        # 16.3.2. Contents of proto Tags
        # The name tag is required, and contains the command name being described.
        name: str = proto.find('name').text
        if name.startswith('vkDestroy'): # as per Vulkan style guide (2.6.2 Command Verbs)
            handle_type = 'Vk' + name.removeprefix('vkDestroy') # vkDestroyInstance -> VkInstance

            # todo: add checks for parent handle types, which are not necessary atm
            commands.setdefault(handle_type, []).append(RegistryHandleLifetimeCommand(
                name=name,
                handle_type=handle_type,
                is_destroy_command=True
            ))
        elif name.startswith('vkCreate'):
            handle_type = 'Vk' + name.removeprefix('vkCreate') # vkCreateInstance -> VkInstance

            # todo: add checks for parent handle types, which are not necessary atm
            commands.setdefault(handle_type, []).append(RegistryHandleLifetimeCommand(
                name=name,
                handle_type=handle_type,
                is_create_command=True
            ))
    return commands




