#!/usr/bin/env python3
"""Converte PNGs OpenMoji 72x72 em headers C (RGB565 + alpha como LVGL image_dsc)."""
from PIL import Image
import os

SRC = "/home/pu2peg/projects/estacao_meteo/assets"
DST = "/home/pu2peg/projects/estacao_meteo/src_icons"
os.makedirs(DST, exist_ok=True)

names = {
    "2600": "sun",        # sol
    "2601": "cloud",      # nuvem
    "26C5": "suncloud",   # sol atras da nuvem
    "26C8": "storm",      # tempestade
    "1F326": "sunrain",   # sol com chuva
    "1F327": "rain",      # nuvem com chuva
    "1F327": "rain",
    "2744": "snow",       # neve/frio
    "1F32B": "fog",       # nevoa
}

for hexcode, name in names.items():
    path = f"{SRC}/{hexcode}.png"
    if not os.path.exists(path):
        print(f"pulado {hexcode} ({name})")
        continue
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    px = im.load()
    # RGB565 com alpha binario (threshold 128) — fundo preto da UI
    lines = []
    for y in range(h):
        row = []
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 128:
                row.append(0x0000)  # transparente -> preto
            else:
                c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                row.append(c)
        lines.append(row)
    with open(f"{DST}/icon_{name}.h", "w") as f:
        f.write(f"// OpenMoji {hexcode} CC-BY-SA 4.0 — gerado por icons.py\n")
        f.write(f"#pragma once\n#include <lvgl.h>\n\n")
        f.write(f"static const uint16_t icon_{name}_map[{w*h}] = {{\n")
        for row in lines:
            f.write(",".join(f"0x{c:04X}" for c in row) + ",\n")
        f.write("};\n\n")
        f.write(f"static const lv_image_dsc_t icon_{name} = {{\n")
        f.write(f"  .header = {{.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, .w = {w}, .h = {h}}},\n")
        f.write(f"  .data_size = {w*h}*2,\n  .data = (const uint8_t*)icon_{name}_map }};\n")
    print(f"icon_{name}.h: {w}x{h}")
print("OK")
