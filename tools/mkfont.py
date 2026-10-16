#!/usr/bin/env python3
"""Generate the SymbOS 4x8 font, the UTF-8 glyph map and the colour table.

Outputs (written next to the C sources, one directory up):
  font4x8.h   - SymbOS font (data segment), 4 px advance, 8 px tall
  glyphmap.h  - Unicode code point -> font byte table for the non-ASCII glyphs
  colmap.h    - xterm 256-colour index -> SymbOS 16-colour palette index

SymbOS font format: byte 0 = height, byte 1 = first character code, then
16 bytes per character: 1 byte width (advance in pixels) + 15 bitmap rows,
MSB = leftmost pixel.

Glyphs are drawn below as 8 rows (row 0 = top), '#' = pixel set.
Text glyphs use columns 0-2 (column 3 is the gap); box-drawing glyphs use
all 4 columns so lines join up.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.dirname(HERE)

# ---------------------------------------------------------------- ASCII glyphs
# Each entry: dict {row: pattern}. Rows not given are blank.
def g(*rows, start=1):
    return {start + i: r for i, r in enumerate(rows)}

ASCII = {
    ' ': {},
    '!': g('.#.', '.#.', '.#.', '...', '.#.'),
    '"': g('#.#', '#.#'),
    '#': g('#.#', '###', '#.#', '###', '#.#'),
    '$': g('.##', '##.', '.#.', '.##', '##.'),
    '%': g('#..', '..#', '.#.', '#..', '..#'),
    '&': g('.#.', '#.#', '.#.', '#.#', '.##'),
    "'": g('.#.', '.#.'),
    '(': g('..#', '.#.', '.#.', '.#.', '..#'),
    ')': g('#..', '.#.', '.#.', '.#.', '#..'),
    '*': g('...', '#.#', '.#.', '#.#'),
    '+': g('...', '.#.', '###', '.#.'),
    ',': g('.#.', '#..', start=5),
    '-': g('###', start=3),
    '.': g('.#.', start=5),
    '/': g('..#', '..#', '.#.', '#..', '#..'),
    '0': g('###', '#.#', '#.#', '#.#', '###'),
    '1': g('.#.', '##.', '.#.', '.#.', '###'),
    '2': g('##.', '..#', '.#.', '#..', '###'),
    '3': g('##.', '..#', '.#.', '..#', '##.'),
    '4': g('#.#', '#.#', '###', '..#', '..#'),
    '5': g('###', '#..', '##.', '..#', '##.'),
    '6': g('.##', '#..', '###', '#.#', '###'),
    '7': g('###', '..#', '.#.', '.#.', '.#.'),
    '8': g('###', '#.#', '###', '#.#', '###'),
    '9': g('###', '#.#', '###', '..#', '##.'),
    ':': g('.#.', '...', '.#.', start=2),
    ';': g('.#.', '...', '.#.', '#..', start=2),
    '<': g('..#', '.#.', '#..', '.#.', '..#'),
    '=': g('###', '...', '###', start=2),
    '>': g('#..', '.#.', '..#', '.#.', '#..'),
    '?': g('##.', '..#', '.#.', '...', '.#.'),
    '@': g('.#.', '#.#', '###', '#..', '.##'),
    'A': g('.#.', '#.#', '###', '#.#', '#.#'),
    'B': g('##.', '#.#', '##.', '#.#', '##.'),
    'C': g('.##', '#..', '#..', '#..', '.##'),
    'D': g('##.', '#.#', '#.#', '#.#', '##.'),
    'E': g('###', '#..', '##.', '#..', '###'),
    'F': g('###', '#..', '##.', '#..', '#..'),
    'G': g('.##', '#..', '#.#', '#.#', '.##'),
    'H': g('#.#', '#.#', '###', '#.#', '#.#'),
    'I': g('###', '.#.', '.#.', '.#.', '###'),
    'J': g('..#', '..#', '..#', '#.#', '.#.'),
    'K': g('#.#', '#.#', '##.', '#.#', '#.#'),
    'L': g('#..', '#..', '#..', '#..', '###'),
    'M': g('#.#', '###', '###', '#.#', '#.#'),
    'N': g('##.', '#.#', '#.#', '#.#', '#.#'),
    'O': g('.#.', '#.#', '#.#', '#.#', '.#.'),
    'P': g('##.', '#.#', '##.', '#..', '#..'),
    'Q': g('.#.', '#.#', '#.#', '##.', '.##'),
    'R': g('##.', '#.#', '##.', '#.#', '#.#'),
    'S': g('.##', '#..', '.#.', '..#', '##.'),
    'T': g('###', '.#.', '.#.', '.#.', '.#.'),
    'U': g('#.#', '#.#', '#.#', '#.#', '###'),
    'V': g('#.#', '#.#', '#.#', '#.#', '.#.'),
    'W': g('#.#', '#.#', '###', '###', '#.#'),
    'X': g('#.#', '#.#', '.#.', '#.#', '#.#'),
    'Y': g('#.#', '#.#', '.#.', '.#.', '.#.'),
    'Z': g('###', '..#', '.#.', '#..', '###'),
    '[': g('##.', '#..', '#..', '#..', '##.'),
    '\\': g('#..', '#..', '.#.', '..#', '..#'),
    ']': g('.##', '..#', '..#', '..#', '.##'),
    '^': g('.#.', '#.#'),
    '_': g('####', start=6),          # full width so "___" joins up
    '`': g('#..', '.#.'),
    'a': g('.##', '#.#', '.##', start=3),
    'b': g('#..', '#..', '##.', '#.#', '##.'),
    'c': g('.##', '#..', '.##', start=3),
    'd': g('..#', '..#', '.##', '#.#', '.##'),
    'e': g('.##', '###', '#..', '.##', start=2),
    'f': g('..#', '.#.', '###', '.#.', '.#.'),
    'g': g('.##', '#.#', '.##', '..#', '##.', start=3),
    'h': g('#..', '#..', '##.', '#.#', '#.#'),
    'i': g('.#.', '...', '##.', '.#.', '###'),
    'j': g('..#', '...', '..#', '..#', '..#', '#.#', '.#.'),
    'k': g('#..', '#..', '#.#', '##.', '#.#'),
    'l': g('##.', '.#.', '.#.', '.#.', '###'),
    'm': g('###', '###', '#.#', start=3),
    'n': g('##.', '#.#', '#.#', start=3),
    'o': g('.#.', '#.#', '.#.', start=3),
    'p': g('##.', '#.#', '##.', '#..', '#..', start=3),
    'q': g('.##', '#.#', '.##', '..#', '..#', start=3),
    'r': g('.##', '#..', '#..', start=3),
    's': g('.##', '.#.', '##.', start=3),
    't': g('.#.', '.#.', '###', '.#.', '.##'),
    'u': g('#.#', '#.#', '.##', start=3),
    'v': g('#.#', '#.#', '.#.', start=3),
    'w': g('#.#', '###', '###', start=3),
    'x': g('#.#', '.#.', '#.#', start=3),
    'y': g('#.#', '#.#', '.##', '..#', '##.', start=3),
    'z': g('##.', '.#.', '.##', start=3),
    '{': g('.##', '.#.', '##.', '.#.', '.##'),
    '|': g('.#.', '.#.', '.#.', '.#.', '.#.', '.#.'),
    '}': g('##.', '.#.', '.##', '.#.', '##.'),
    '~': g('.##', '##.', start=2),
}

# ------------------------------------------------- box drawing (full 4x8 cell)
# vertical stroke in column 1, horizontal stroke in row 3
def box(up, down, left, right):
    rows = [['.'] * 4 for _ in range(8)]
    for r in range(8):
        if (up and r <= 3) or (down and r >= 3):
            rows[r][1] = '#'
    for c in range(4):
        if (left and c <= 1) or (right and c >= 1):
            rows[3][c] = '#'
    return {i: ''.join(r) for i, r in enumerate(rows)}

# --------------------------------------- non-ASCII glyphs used by wttr.in (v1)
# Order defines the font byte: first entry -> 0x80, next -> 0x81, ...
EXTRA = [
    (0x2500, box(0, 0, 1, 1)),   # ─
    (0x2502, box(1, 1, 0, 0)),   # │
    (0x250C, box(0, 1, 0, 1)),   # ┌
    (0x2510, box(0, 1, 1, 0)),   # ┐
    (0x2514, box(1, 0, 0, 1)),   # └
    (0x2518, box(1, 0, 1, 0)),   # ┘
    (0x251C, box(1, 1, 0, 1)),   # ├
    (0x2524, box(1, 1, 1, 0)),   # ┤
    (0x252C, box(0, 1, 1, 1)),   # ┬
    (0x2534, box(1, 0, 1, 1)),   # ┴
    (0x253C, box(1, 1, 1, 1)),   # ┼
    (0x2191, g('.#.', '###', '.#.', '.#.', '.#.')),          # ↑
    (0x2193, g('.#.', '.#.', '.#.', '###', '.#.')),          # ↓
    (0x2190, g('..#.', '.#..', '####', '.#..', '..#.')),            # ←
    (0x2192, g('.#..', '..#.', '####', '..#.', '.#..')),            # →
    (0x2196, g('###.', '##..', '#.#.', '...#', start=1)),    # ↖
    (0x2197, g('.###', '..##', '.#.#', '#...', start=1)),    # ↗
    (0x2198, g('#...', '.#.#', '..##', '.###', start=2)),    # ↘
    (0x2199, g('...#', '#.#.', '##..', '###.', start=2)),    # ↙
    (0x2018, g('.#.', '#..', start=2)),                      # ‘ (rain)
    (0x2019, g('.#.', '..#', start=2)),                      # ’
    (0x201A, g('.#.', '#..', start=5)),                      # ‚ (heavy rain)
    (0x26A1, g('..#', '.#.', '###', '.#.', '#..')),          # ⚡
    (0x00B0, g('.#.', '#.#', '.#.', start=1)),               # °
    (0x2026, g('#.#.', start=5)),                            # …
    (0x2015, g('####', start=3)),                            # ― (sun rays)
    (0x2022, g('.#.', '###', '.#.', start=2)),               # •
    # rounded corners (wind arrows in some wttr.in modes) reuse box shapes
    (0x256D, box(0, 1, 0, 1)),   # ╭
    (0x256E, box(0, 1, 1, 0)),   # ╮
    (0x256F, box(1, 0, 1, 0)),   # ╯
    (0x2570, box(1, 0, 0, 1)),   # ╰
]

FIRST = 0x20
HEIGHT = 8
ADVANCE = 4


def glyph_table():
    """Return list of (code, rows-dict) for every font slot from FIRST."""
    slots = []
    for code in range(FIRST, 0x7F):
        slots.append((code, ASCII.get(chr(code), ASCII['?'])))
    slots.append((0x7F, ASCII['?']))
    for i, (_cp, rows) in enumerate(EXTRA):
        slots.append((0x80 + i, rows))
    return slots


def bitmap(rows):
    out = [0] * 15
    for r, pat in rows.items():
        v = 0
        for c, ch in enumerate(pat):
            if ch == '#':
                v |= 0x80 >> c
        out[r] = v
    return out


def write_font():
    slots = glyph_table()
    data = [HEIGHT, FIRST]
    for _code, rows in slots:
        data.append(ADVANCE)
        data += bitmap(rows)
    lines = []
    for i in range(0, len(data), 16):
        lines.append('  ' + ', '.join('0x%02X' % b for b in data[i:i + 16]) + ',')
    with open(os.path.join(OUT, 'font4x8.h'), 'w') as f:
        f.write('/* Generated by tools/mkfont.py - do not edit. */\n')
        f.write('/* SymbOS font: %d px tall, %d px advance, chars 0x%02X-0x%02X */\n'
                % (HEIGHT, ADVANCE, FIRST, slots[-1][0]))
        f.write('#define WT_FONT_SIZE %d\n' % len(data))
        f.write('_data unsigned char wt_font[%d] = {\n' % len(data))
        f.write('\n'.join(lines) + '\n};\n')
    return len(data)


def write_glyphmap():
    with open(os.path.join(OUT, 'glyphmap.h'), 'w') as f:
        f.write('/* Generated by tools/mkfont.py - do not edit. */\n')
        f.write('/* Unicode code point -> font byte for non-ASCII glyphs. */\n')
        f.write('#define WT_GLYPHS %d\n' % len(EXTRA))
        f.write('static const unsigned short wt_glyph_cp[WT_GLYPHS] = {\n')
        f.write(',\n'.join('  0x%04X /* %s */' % (cp, chr(cp)) for cp, _ in EXTRA))
        f.write('\n};\n')
        f.write('/* font byte for entry i is 0x80 + i */\n')


# ------------------------------------------------------------ colour mapping
# SymbOS default 16-colour palette (index: RGB), from SCC graphics docs
SYMBOS = {
    0: (0xF7, 0xF7, 0x90),  # yellow (pale)
    1: (0x06, 0x06, 0x06),  # black
    2: (0xF7, 0x90, 0x06),  # orange
    3: (0x90, 0x06, 0x06),  # red (dark)
    4: (0x06, 0xF7, 0xF7),  # cyan
    5: (0x06, 0x06, 0x90),  # dark blue
    6: (0x90, 0x90, 0xF7),  # light blue
    7: (0x06, 0x06, 0xF7),  # blue
    8: (0xF7, 0xF7, 0xF7),  # white
    9: (0x06, 0x90, 0x06),  # green
    10: (0x06, 0xF7, 0x06),  # light green
    11: (0xF7, 0x06, 0xF7),  # magenta
    12: (0xF7, 0xF7, 0x06),  # bright yellow
    13: (0x90, 0x90, 0x90),  # gray
    14: (0xF7, 0x90, 0x90),  # pink
    15: (0xF7, 0x06, 0x06),  # bright red
}

# Hand-tuned overrides where plain nearest-RGB looks wrong on this palette.
OVERRIDES = {
    250: 8,   # light cloud grey -> white (keeps it distinct from 240)
    240: 13,  # dark cloud grey  -> gray
    226: 12,  # sun yellow       -> bright yellow
    111: 6,   # rain blue        -> light blue
    118: 10,  # 13-15 C / light wind stays green, not yellow
    7: 8,     # ANSI white       -> white
    8: 13,    # ANSI bright black-> gray
}


def xterm_rgb(i):
    base = [(0, 0, 0), (128, 0, 0), (0, 128, 0), (128, 128, 0), (0, 0, 128),
            (128, 0, 128), (0, 128, 128), (192, 192, 192), (128, 128, 128),
            (255, 0, 0), (0, 255, 0), (255, 255, 0), (0, 0, 255),
            (255, 0, 255), (0, 255, 255), (255, 255, 255)]
    if i < 16:
        return base[i]
    if i < 232:
        i -= 16
        lv = [0, 95, 135, 175, 215, 255]
        return (lv[i // 36], lv[(i // 6) % 6], lv[i % 6])
    v = 8 + (i - 232) * 10
    return (v, v, v)


def nearest(rgb):
    best, bd = 0, 1 << 30
    for idx, c in SYMBOS.items():
        # weighted distance: green matters most to the eye
        d = (2 * (rgb[0] - c[0]) ** 2 + 4 * (rgb[1] - c[1]) ** 2
             + 3 * (rgb[2] - c[2]) ** 2)
        if d < bd:
            best, bd = idx, d
    return best


def colmap():
    return [OVERRIDES.get(i, nearest(xterm_rgb(i))) for i in range(256)]


def write_colmap():
    m = colmap()
    with open(os.path.join(OUT, 'colmap.h'), 'w') as f:
        f.write('/* Generated by tools/mkfont.py - do not edit. */\n')
        f.write('/* xterm 256-colour index -> SymbOS palette index (0-15). */\n')
        f.write('static const unsigned char wt_colmap[256] = {\n')
        for i in range(0, 256, 16):
            f.write('  ' + ', '.join('%2d' % v for v in m[i:i + 16]) + ',\n')
        f.write('};\n')


if __name__ == '__main__':
    n = write_font()
    write_glyphmap()
    write_colmap()
    print('font4x8.h: %d bytes, %d extra glyphs; glyphmap.h, colmap.h written'
          % (n, len(EXTRA)))
