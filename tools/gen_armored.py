#!/usr/bin/env python3
"""The armored skin for MEWTWO: tools/art/armored_mewtwo_sheet.png -> sprites.

    python3 tools/gen_armored.py          # writes both outputs below

Outputs:
  tools/sdcard/mons/pa150.bin   TPK2 (pack_pmd.py's format), streamed from the SD
                                like every other sprite; pack_bundle.py files it
                                in the Kanto pack by the 150 in its name.
  armored_art.h                 one still frame, for the prize counter's tile.

The sheet was SUPPLIED BY THE PROJECT OWNER, who names its source as a
Pokemon3D forum thread: https://forum.pokemon3d.net/threads/11097/ . Its terms
of use were NOT checked (the forum could not be reached from the build
environment) -- confirm them, and credit the artist as they ask, before
publishing anything built from it. It is a ~3x enlargement with smoothing, so
the native pixels are recovered first (3x3 cells, best phase), then reduced to
a 16-colour palette.

Layout of the sheet, four rows of three frames on white:
  row 1 front, row 2 facing LEFT, row 3 the owner's chosen pose, row 4 facing RIGHT.
Actions written: IDLE (row 3), WALKL (row 2), WALKR (row 4), and four DERIVED
from row 3 by moving and recolouring it -- the sheet has no such frames, and
standing still asleep or mid-attack read as a frozen sprite: SLEEP (a slow
breathing dip, aura dimmed), EAT (a bob), HURT (a red tint and a shake),
ATTACK (a rise and a lunge, aura at full). Anything else falls back to IDLE
(PmdMon::has()).

The purple aura is BAKED into the frames -- an inner ring that breathes and a
sparse outer sparkle -- so it shows wherever the sprite is drawn without any
firmware drawing code. It never extends below the feet: PmdMon anchors a sprite
by its lowest opaque row, and an aura there would float the creature.
"""
import math
import os
import struct
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SHEET = os.path.join(HERE, 'art', 'armored_mewtwo_sheet.png')
OUT_BIN = os.path.join(HERE, 'sdcard', 'mons', 'pa150.bin')
OUT_H = os.path.join(HERE, '..', 'armored_art.h')

COLS = [(8, 86), (104, 182), (200, 278)]
ROWS = {1: (2, 89), 2: (100, 186), 3: (193, 281), 4: (292, 377)}
SCALE = 3
COLOURS = 16
PAD = 3                      # aura room on the sides and top
AURA = (170, 70, 255)
IDLE_MS = 180
WALK_MS = 160


def rgb565(c):
    r, g, b = c
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def native(im, x0, y0, x1, y1):
    """The original pixel grid under a SCALE-times enlargement: the phase whose
    cell centres reproduce the crop best."""
    c = im.crop((x0, y0, x1, y1))
    best = None
    for ox in range(SCALE):
        for oy in range(SCALE):
            nw, nh = (c.width - ox) // SCALE, (c.height - oy) // SCALE
            g = [[c.getpixel((ox + SCALE * i + 1, oy + SCALE * j + 1)) for i in range(nw)] for j in range(nh)]
            err = 0
            for j in range(nh):
                for i in range(nw):
                    p = g[j][i]
                    for dx in range(SCALE):
                        for dy in range(SCALE):
                            q = c.getpixel((ox + SCALE * i + dx, oy + SCALE * j + dy))
                            err += sum(abs(a - b) for a, b in zip(p, q))
            if best is None or err < best[0]:
                best = (err, g)
    return best[1]


def is_bg(p):
    return p[0] > 225 and p[1] > 225 and p[2] > 225


def frames_from_sheet():
    im = Image.open(SHEET).convert('RGB')
    raw = {r: [native(im, x0, ROWS[r][0], x1, ROWS[r][1]) for x0, x1 in COLS] for r in (2, 3, 4)}
    # one shared 16-colour palette for every frame, background excluded
    flat = [p for r in raw for f in raw[r] for row in f for p in row if not is_bg(p)]
    strip = Image.new('RGB', (len(flat), 1))
    strip.putdata(flat)
    q = strip.quantize(colors=COLOURS, method=Image.Quantize.MEDIANCUT)
    qpal = q.getpalette()[:COLOURS * 3]
    pal = [tuple(qpal[i * 3:i * 3 + 3]) for i in range(COLOURS)]
    lut = {}
    def index(p):
        if p not in lut:
            lut[p] = min(range(COLOURS), key=lambda k: sum((a - b) ** 2 for a, b in zip(p, pal[k])))
        return lut[p]
    out = {}
    for r in raw:
        out[r] = [[[0xFF if is_bg(p) else index(p) for p in row] for row in f] for f in raw[r]]
    return pal, out


def trim(frames):
    """Crop every frame of an action to one shared bounding box."""
    xs = [x for f in frames for row in f for x, v in enumerate(row) if v != 0xFF]
    ys = [y for f in frames for y, row in enumerate(f) if any(v != 0xFF for v in row)]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    return [[row[x0:x1 + 1] for row in f[y0:y1 + 1]] for f in frames]


def with_aura(f, level, drift, aura_idx):
    """Frame -> canvas with the aura baked in. level 0..3 = how bright the
    inner ring breathes this frame; drift moves the sparkle."""
    h, w = len(f), len(f[0])
    W, H = w + 2 * PAD, h + PAD          # no room below: the feet stay the base
    c = [[0xFF] * W for _ in range(H)]
    for y in range(h):
        for x in range(w):
            if f[y][x] != 0xFF:
                c[y + PAD][x + PAD] = f[y][x]
    solid = {(x, y) for y in range(H) for x in range(W) if c[y][x] != 0xFF}
    feet = max(y for _, y in solid)
    inner, mid, outer = aura_idx
    for y in range(H):
        for x in range(W):
            if (x, y) in solid or y > feet:
                continue
            d = min((max(abs(dx), abs(dy)) for dy in range(-3, 4) for dx in range(-3, 4)
                     if (x + dx, y + dy) in solid), default=9)
            if d == 1:
                c[y][x] = inner[level]
            elif d == 2:
                c[y][x] = mid
            elif d == 3 and (x + y + drift) % 4 == 0:
                c[y][x] = outer
    return c


def shifted(f, dx, dy, m=3):
    """f placed in a canvas m px bigger on every side, moved by (dx, dy):
    every frame of one action shares the canvas, so the motion is real."""
    h, w = len(f), len(f[0])
    c = [[0xFF] * (w + 2 * m) for _ in range(h + 2 * m)]
    for y in range(h):
        for x in range(w):
            if f[y][x] != 0xFF:
                c[y + m + dy][x + m + dx] = f[y][x]
    return c


def main():
    pal, rows = frames_from_sheet()
    # the aura's colours: four breathing levels for the inner ring, then mid, then outer
    levels = [tuple(int(a * (0.45 + 0.4 * k / 3)) for a in AURA) for k in range(4)]
    aura_pal = levels + [tuple(int(a * 0.32) for a in AURA), tuple(int(a * 0.25) for a in AURA)]
    base = len(pal)
    pal = pal + aura_pal
    aura_idx = ([base + k for k in range(4)], base + 4, base + 5)

    acts = []
    seq = [0, 1, 0, 2, 0, 1, 0, 2]             # the sheet's stride, twice per loop
    breath = [0, 1, 2, 3, 3, 2, 1, 0]          # one slow breath over the loop
    for aid, row, ms in ((0, 3, IDLE_MS), (1, 2, WALK_MS), (2, 4, WALK_MS)):
        fr = trim(rows[row])
        canv = [with_aura(fr[seq[k]], breath[k], k, aura_idx) for k in range(len(seq))]
        acts.append((aid, canv, ms))

    # Derived from row 3's first frame (pack_pmd.py's action ids).
    still = trim(rows[3])[0]
    # HURT's red: every sheet colour pulled halfway to red, as extra palette entries
    red_base = len(pal)
    pal = pal + [((c[0] + 255) // 2, c[1] // 2, c[2] // 2) for c in pal[:COLOURS]]
    reddened = [[v if v == 0xFF or v >= COLOURS else red_base + v for v in row] for row in still]
    def act(frames, levels, ms):
        return [with_aura(f, levels[k % len(levels)], k, aura_idx) for k, f in enumerate(frames)], ms
    derived = {
        3: act([shifted(still, 0, 0), shifted(still, 0, 1)], [0, 0], 700),                  # SLEEP
        4: act([shifted(still, 0, d) for d in (0, 1, 2, 1)], [1, 1, 2, 1], 150),             # EAT
        5: act([shifted(reddened, d, 0) for d in (-2, 2, -1, 1)], [3, 3, 2, 2], 90),         # HURT
        6: act([shifted(still, 0, d) for d in (0, -2, -3, 1, 2, 0)], [1, 2, 3, 3, 2, 1], 90),  # ATTACK
    }
    for aid, (canv, ms) in sorted(derived.items()):
        acts.append((aid, canv, ms))
    assert len(pal) <= 256

    os.makedirs(os.path.dirname(OUT_BIN), exist_ok=True)
    with open(OUT_BIN, 'wb') as o:
        o.write(b'TPK2')
        o.write(struct.pack('<BH', len(acts), len(pal)))
        for c in pal:
            o.write(struct.pack('<H', rgb565(c)))
        for aid, canv, ms in acts:
            h, w = len(canv[0]), len(canv[0][0])
            assert all(len(f) == h and len(f[0]) == w for f in canv)
            o.write(struct.pack('<4B', aid, w, h, len(canv)))
            o.write(struct.pack('<%dH' % len(canv), *([ms] * len(canv))))
            for f in canv:
                o.write(bytes(v for row in f for v in row))

    # the prize tile's still: row 3, first frame, no aura (the tile is small)
    still = trim(rows[3])[0]
    h, w = len(still), len(still[0])
    hdr = ['// GENERATED by tools/gen_armored.py - do not edit',
           '#pragma once',
           '#include <stdint.h>',
           '// The armored MEWTWO, one still frame for the prize counter. Indexed;',
           '// 0xFF is transparent. The animated sprite is pa150.bin on the SD card.',
           '#define ARMORED_W %d' % w,
           '#define ARMORED_H %d' % h,
           'static const uint16_t ARMORED_PAL[%d] = {' % COLOURS,
           '  ' + ', '.join('0x%04X' % rgb565(c) for c in pal[:COLOURS]),
           '};',
           'static const uint8_t ARMORED_IDX[ARMORED_W * ARMORED_H] = {']
    for row in still:
        hdr.append('  ' + ' '.join('0x%02X,' % v for v in row))
    hdr += ['};', '']
    with open(OUT_H, 'w') as f:
        f.write('\n'.join(hdr))
    sys.stderr.write('wrote %s (%d actions, %d colours) and %s (%dx%d)\n'
                     % (OUT_BIN, len(acts), len(pal), OUT_H, w, h))


if __name__ == '__main__':
    main()
