#!/usr/bin/env python3
"""Gera atlas de fontes (alpha 8-bit) em C para o renderizador do tablet."""
import sys
from PIL import Image, ImageFont, ImageDraw

FONTS = [
    ("F_SANS", "/usr/share/fonts/opentype/inter/InterDisplay-SemiBold.otf"),
    ("F_BOLD", "/usr/share/fonts/opentype/inter/InterDisplay-Bold.otf"),
    ("F_COND", "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"),
]
SIZES = [96, 48, 22]
CHARS = [chr(c) for c in range(32, 127)] + ["°"]
ATLAS_W = 1024

out = []
out.append("/* gerado por tools/gen_font.py - nao editar */")
out.append("#include <stdint.h>")
out.append("typedef struct { int16_t x, y, w, h, xoff, yoff, adv; } glyph_t;")
out.append("typedef struct { int px, ascent, descent, atlas_w, atlas_h; const uint8_t *atlas; const glyph_t *g; } fontsize_t;")
index_lines = []
for fname, path in FONTS:
    for sz in SIZES:
        font = ImageFont.truetype(path, sz)
        ascent, descent = font.getmetrics()
        glyphs = []
        imgs = []
        for ch in CHARS:
            bbox = font.getbbox(ch, anchor="ls")  # relativo a baseline
            adv = font.getlength(ch)
            x0, y0, x1, y1 = bbox
            w, h = max(0, x1 - x0), max(0, y1 - y0)
            if w and h:
                im = Image.new("L", (w + 2, h + 2), 0)
                ImageDraw.Draw(im).text((1 - x0, 1 - y0), ch, font=font, fill=255, anchor="ls")
            else:
                im = None
            imgs.append(im)
            glyphs.append([0, 0, (w + 2) if im else 0, (h + 2) if im else 0, x0 - 1, y0 - 1, round(adv)])
        # empacota em linhas
        x = y = rowh = 0
        for i, im in enumerate(imgs):
            if im is None:
                continue
            if x + im.width > ATLAS_W:
                x = 0; y += rowh; rowh = 0
            glyphs[i][0], glyphs[i][1] = x, y
            x += im.width
            rowh = max(rowh, im.height)
        H = y + rowh
        atlas = Image.new("L", (ATLAS_W, H), 0)
        for i, im in enumerate(imgs):
            if im is not None:
                atlas.paste(im, (glyphs[i][0], glyphs[i][1]))
        name = f"{fname.lower()}_{sz}"
        data = atlas.tobytes()
        out.append(f"static const uint8_t {name}_atlas[{len(data)}] = {{")
        for i in range(0, len(data), 40):
            out.append(",".join(str(b) for b in data[i:i+40]) + ",")
        out.append("};")
        out.append(f"static const glyph_t {name}_g[{len(CHARS)}] = {{")
        for g in glyphs:
            out.append("{%d,%d,%d,%d,%d,%d,%d}," % tuple(g))
        out.append("};")
        index_lines.append(f"{{{sz},{ascent},{descent},{ATLAS_W},{H},{name}_atlas,{name}_g}}")
        print(name, ATLAS_W, H, file=sys.stderr)
out.append(f"#define FONT_COUNT {len(FONTS)}")
out.append(f"#define FONT_SIZES {len(SIZES)}")
out.append("static const fontsize_t g_fonts[%d][%d] = {" % (len(FONTS), len(SIZES)))
for fi in range(len(FONTS)):
    out.append("{" + ",".join(index_lines[fi*len(SIZES):(fi+1)*len(SIZES)]) + "},")
out.append("};")
open(sys.argv[1], "w").write("\n".join(out) + "\n")
