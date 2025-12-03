# Listing of all Jinja templates for Vulkan helper generator

WARP_HEADER_GENERIC_ANNOTATION = r"""{{ '#pragma once' }}

// WARP ENGINE NEBULAE RENDERING INTERFACE AUTO-GENERATED FILE -- DO NOT EDIT
//
// This header was generated as a part of NRI Vulkan helper auto-generation
// All scripts related to helper generation are stored under scripts/ directory of NRI
//
// Generated from: {{ xml_path }}
// Timestamp:      {{ timestamp }}
//
""".rstrip()

WARP_HEADER_VK_STRUCTURE_TYPE_TRAIT = WARP_HEADER_GENERIC_ANNOTATION + r"""
// Call 'Warp::nri::vk::GetVkSType<T>()' to obtain the matching 'VK_STRUCTURE_TYPE_*' enum value at compile time.

#include <vulkan/vulkan.h>

namespace Warp::nri::vk 
{

{{ 'template<typename T> constexpr VkStructureType GetVkSType() noexcept;' ~ '\n' }}

{#-------------------------------------------------------------#}
{#  One specialization per mapping, with optional guard macros #}
{#-------------------------------------------------------------#}

{% for m in mappings -%}
{{ m.guard_open -}}
template<> constexpr VkStructureType {{ '{:<80}'.format('GetVkSType<{}>() noexcept'.format(m.struct)) }} { return {{ m.s_type }}; }
{{ m.guard_close }}
{%- endfor %}

} // Warp::nri::vk namespace
"""

WARP_HEADER_VK_OBJECT_TYPE_TRAIT = WARP_HEADER_GENERIC_ANNOTATION + r"""
// Call 'Warp::nri::vk::GetVkObjectType<T>()' to obtain the matching 'VK_OBJECT_TYPE_*' enum value at compile time.

#include <vulkan/vulkan.h>

#include <type_traits>

namespace Warp::nri::vk 
{

{#  Then specialize all GetVkObjectType<T> specializations #}
{{ 'template<HandleType T> constexpr VkObjectType GetVkObjectType() noexcept;' }}
{% for i in mappings %}
{{ i.platform_guard_open -}}
{% if i.extension %}{{ i.extension_comment ~ '\n'}}{% endif -%}
template<> constexpr VkObjectType {{ '{:<56}'.format('GetVkObjectType<{}>() noexcept'.format(i.get_type())) }} { return {{ i.get_enum() }}; }
{{ i.platform_guard_close -}}
{%- endfor %}

{#  Firstly specify all IsNonDispatchableHandle_v<T> specializations #}
{#  You cannot just simply invert value of IsDispatchableHandle_v here because some arbitrary types T would then falsely be treated as Vk handles #}
{{ 'template<typename T> constexpr bool IsNonDispatchableHandle_v = false;' }}
{% for i in mappings %}
{{ i.platform_guard_open -}}
template<> constexpr bool {{ '{:<64}'.format('IsNonDispatchableHandle_v<{}>'.format(i.get_type())) }} = {{ 'false' if i.is_dispatchable() else 'true' }};
{{ i.platform_guard_close -}}
{%- endfor %}

{#  After this specify all IsDispatchableHandle_v<T> specializations #}
{{ 'template<typename T> constexpr bool IsDispatchableHandle_v = false;' }}
{% for i in mappings %}
{{ i.platform_guard_open -}}
template<> constexpr bool {{ '{:<64}'.format('IsDispatchableHandle_v<{}>'.format(i.get_type())) }} = {{ 'true' if i.is_dispatchable() else 'false' }};
{{ i.platform_guard_close -}}
{%- endfor %}

template<typename T> constexpr bool IsHandleType_v = IsDispatchableHandle_v<T> || IsNonDispatchableHandle_v<T>;
template<typename T> concept HandleType = IsHandleType_v<T>;

} // Warp::nri::vk namespace
"""

import datetime as dt
import pathlib
from jinja2 import Environment, BaseLoader


# 
# Some utility functions to manipulate and render Jinja templates easier
#

def render_header(template_str: str, *, output: pathlib.Path, xml_path: pathlib.Path, **kwargs) -> None:
    env = Environment(loader=BaseLoader(), trim_blocks=True, lstrip_blocks=True)
    template = env.from_string(template_str)

    # Base context for 'WARP_HEADER_GENERIC_ANNOTATION' must always be injected
    context = {
        "xml_path": str(xml_path),
        "timestamp": dt.datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC"),
        **kwargs  # also handle custom per-template vars
    }

    output.write_text(template.render(context), encoding="utf-8")
    print(f"[Vulkan helper auto-generation] wrote {output} (context: {', '.join(context)})")