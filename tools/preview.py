#!/usr/bin/env python3
"""Render line buffers produced by ansi_render() the way C_TEXT_CTRL will,
using the generated font and the SymbOS palette, to a PNG preview.

  python3 tools/preview.py lines.bin preview.png

MSX2 512x212 pixels are roughly twice as tall as they are wide on a 4:3
screen, so the preview scales x2 horizontally and x4 vertically.
"""
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mkfont
from PIL import Image

LINES, LINEBUF, HEIGHT, ADV = 44, 200, 8, 4
DEF_ATTR = (8 << 4) | 1


def main(src, dst, sx=2, sy=4):
    data = open(src, 'rb').read()
    glyphs = {code: rows for code, rows in mkfont.glyph_table()}
    rows_out = []
    for r in range(LINES):
        buf = data[r * LINEBUF:(r + 1) * LINEBUF]
        if buf[0] == 0:
            rows_out.append([])
            continue
        cells, attr, i = [], DEF_ATTR, 0
        while i < len(buf) and buf[i] != 0:
            b = buf[i]
            if b == 1:
                attr = buf[i + 1]
                i += 2
                continue
            cells.append((b, attr))
            i += 1
        rows_out.append(cells)
    while rows_out and not rows_out[-1]:
        rows_out.pop()

    w, h = 125 * ADV + 8, len(rows_out) * HEIGHT + 8
    img = Image.new('RGB', (w, h), mkfont.SYMBOS[1])
    px = img.load()
    for y0, cells in enumerate(rows_out):
        for x0, (code, attr) in enumerate(cells):
            fg, bg = mkfont.SYMBOS[attr >> 4], mkfont.SYMBOS[attr & 15]
            g = glyphs.get(code, {})
            for gy in range(HEIGHT):
                pat = g.get(gy, '')
                for gx in range(ADV):
                    on = gx < len(pat) and pat[gx] == '#'
                    px[4 + x0 * ADV + gx, 4 + y0 * HEIGHT + gy] = fg if on else bg
    img = img.resize((w * sx, h * sy), Image.NEAREST)
    img.save(dst)
    print('%s: %dx%d native, %d lines' % (dst, w, h, len(rows_out)))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
