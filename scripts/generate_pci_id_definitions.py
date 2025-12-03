#!/usr/bin/env python3

# generates PCI GPU vendor ID header based on public repository of all known ID's used in PCI devices: https://pci-ids.ucw.cz/
# needs pci.ids file, obtained from the repository as a reference. 
# 
# TODO: If not able to download pci.ids from public repository or a mirror we might want to fallback to a local one

import datetime
import pathlib
import re
import argparse
import requests
import sys
from typing import List, Dict
from jinja2 import Environment, BaseLoader, select_autoescape

PCI_IDS_URL = 'https://pci-ids.ucw.cz/v2.2/pci.ids'

_HEADER_TEMPLATE = r"""#pragma once

// ---------------------------------------------------------------------------
// WARP ENGINE NEBULAE RENDERING INTERFACE AUTO-GENERATED FILE -- DO NOT EDIT
// Generated from: {{ source_url }}
// Timestamp:      {{ timestamp }}
//  
// You can find more information on PCI member companies here: 
// https://pcisig.com/membership/member-companies
// ---------------------------------------------------------------------------

#include <unordered_map>
#include <string_view>
#include <cstdint>
#include <utility>

{% for vendor_name, vendor_id in vendors -%}
#define WARP_PCI_VENDOR_ID_{{ '{:<64}'.format(vendor_name) }} 0x{{ vendor_id }}
{% endfor %}

namespace nri::pci_vendor_id
{
    inline std::unordered_map<int32_t, std::string_view> g_vendorNameTable = {
{% for vendor_name, vendor_id in vendors %}
        std::make_pair(WARP_PCI_VENDOR_ID_{{ '{:<64}'.format(vendor_name) }}, "{{ vendor_name }}"),
{% endfor %}
    };

} // nri::pci_vendor_id namespace

"""


def sanitize_name(name: str) -> str:
    """strip symbols, convert to upper-case"""
    macro = re.sub(r"[^0-9A-Za-z]+", "_", name).strip("_").upper()
    return "INVALID" if not macro else macro


def fetch_pci_ids(url: str) -> Dict[str, str]:
    """Return a dict of vendor_name to vendor_id_hex mapping."""
    text = requests.get(url, timeout=15).text
    vendors: Dict[str, str] = {}
    for line in text.splitlines():
        # skip comments, devices, sub-vendors
        # All vendor IDs start immediately without any whitespaces, additional characters, blanks, etc.
        if not line or line.startswith("#") or line[0].isspace():
            continue
        m = re.match(r"^([0-9A-Fa-f]{4})\s+(.+)", line)
        if m:
            vendor_name = sanitize_name(m.group(2).strip())
            vendor_id_hex = m.group(1).upper()
            #print(f"{vendor_name} to {vendor_id_hex}")
            vendors[vendor_name] = vendor_id_hex
    return vendors


def render_header(vendors: Dict[str, str]) -> str:
    # build {vid, macro} list and render via Jinja
    env = Environment(loader=BaseLoader(), autoescape=True, trim_blocks=True, lstrip_blocks=True)
    tmpl = env.from_string(_HEADER_TEMPLATE)
    return tmpl.render(
        timestamp=datetime.date.today().isoformat(),
        source_url=PCI_IDS_URL,
        vendors=[(vendor_name, vendor_id) for vendor_name, vendor_id in vendors.items()],
    )


def main(argv: List[str] | None = None) -> None:
    parser = argparse.ArgumentParser(
        description=f"Generate a header of all PCI Vendor ID defintions based on open ID repository {PCI_IDS_URL}."
    )
    parser.add_argument("-o", "--output", default="nri_pci_vendor_ids.h", help="Header output path")
    ns = parser.parse_args(argv)

    try:
        vendors = fetch_pci_ids(PCI_IDS_URL)
        output_path = pathlib.Path(ns.output)
        output_path.write_text(render_header(vendors), encoding="utf-8", newline="\n")
        print(f"Succesfully wrote {output_path} with {len(vendors)} vendors from '{PCI_IDS_URL}'")
    except Exception as exc:
        print("Error:", exc, file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()