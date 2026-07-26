#!/usr/bin/env python3

import io
import os
import re
import sys

ICON_FILES = (
    "check.png",
    "nm_device_loopback.png",
    "nm_device_vpn.png",
    "nm_device_wired.png",
    "nm_not_connected.png",
    "nm_connect_stage0.png",
    "nm_connect_stage1.png",
    "nm_connect_stage2.png",
    "nm_connect_stage3.png",
    "nm_connect_stage4.png",
    "nm_connect_stage5.png",
    "nm_connect_stage6.png",
    "nm_signal_0.png",
    "nm_signal_25.png",
    "nm_signal_50.png",
    "nm_signal_75.png",
    "nm_signal_100.png",
    "nmtde2.png",
    "unplugged.png",
    "wifi_off.png",
)

BUILD_SIZES = {
    "check.png": 14,
}


def embed_png_bytes(filename, resize_to=None):
    """Normalize every asset to true-color RGBA PNG before embedding."""
    try:
        from PIL import Image
    except ImportError:
        with open(filename, "rb") as handle:
            return handle.read()

    img = Image.open(filename).convert("RGBA")
    if resize_to:
        img = img.resize((resize_to, resize_to), Image.Resampling.LANCZOS)

    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def process_file(filename):
    basename = os.path.basename(filename)
    varname = re.sub(r"[^a-zA-Z0-9_]", "_", os.path.splitext(basename)[0])
    data = embed_png_bytes(filename, resize_to=BUILD_SIZES.get(basename))

    output = f"// Data for {basename} (embedded as RGBA PNG)\n"
    output += f"static const unsigned char {varname}_data[] = {{\n    "

    for index, byte in enumerate(data):
        output += f"0x{byte:02x}, "
        if (index + 1) % 12 == 0:
            output += "\n    "

    output += "\n};\n"
    output += f"static const size_t {varname}_size = sizeof({varname}_data);\n\n"
    return output


def generate_header_file(directory, output_path):
    header_content = "/* header file auto generated for icons */\n"
    header_content += "#ifndef NM_ICONS_H\n"
    header_content += "#define NM_ICONS_H\n\n"
    header_content += "#include <stddef.h>\n\n"

    for basename in ICON_FILES:
        path = os.path.join(directory, basename)
        if not os.path.isfile(path):
            print(f"Missing icon: {path}")
            return False
        header_content += process_file(path)

    header_content += "#endif /* NM_ICONS_H */\n"

    with open(output_path, "w", encoding="utf-8") as handle:
        handle.write(header_content)

    print(f"Generated {output_path}")
    return True


def main():
    directory = sys.argv[1] if len(sys.argv) > 1 else "icons"
    output_path = sys.argv[2] if len(sys.argv) > 2 else "src/nm_icons.h"
    if not generate_header_file(directory, output_path):
        sys.exit(1)


if __name__ == "__main__":
    main()
