#!/usr/bin/env python3
"""Player avatars: pret/pokered + pret/pokecrystal overworld sprites -> avatars.h

    python3 tools/gen_avatars.py                 # fetch with curl
    python3 tools/gen_avatars.py --local DIR     # DIR/pokered, DIR/pokecrystal clones

The first AVATAR_BASE_COUNT (8) are the free starting set and their order is
FROZEN: Pet::avatar stores the index raw in NVS. The Game Corner's
prize avatars (SHOP) are APPENDED after them, in a frozen order of their own --
casino.cpp stores ownership as a bit per shop index. New ones go at the END.
It also emits all six frames of the Team Rocket grunt for the casino dealer,
and avatar_names.h (names only, no pixels) for code that must not pull the art.

Downloads eight 16x16 overworld sprites from the pokered disassembly, takes the
first (front-facing) frame of each sheet and packs them 8bpp with a four-entry
RGB565 palette; index 0xFF is transparent.

The sheets are 2bpp greyscale, six frames stacked vertically. Shade 3 is the
white background and becomes transparent; 0 is the outline, 1 the large fill,
2 the highlight. Each avatar gets an accent colour on shade 1 -- the same thing
Pokemon Yellow did when it colourised these sprites for the GBC -- because eight
identical grey silhouettes are unreadable at a glance in the picker.

SOURCE AND LICENCE -- read CREDITS.md before publishing anything built from
this. These are Nintendo / Game Freak / Creatures artwork, taken from a
disassembly. They are NOT licensed for redistribution the way the PMD sprites
(CC BY-NC) or the badges (CC BY 3.0) are. They are here because the project
owner asked for the real sprites, and CREDITS.md records that honestly rather
than leaving it undocumented.
"""
import os
import struct
import sys
import subprocess
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from avatars_original import ORIGINAL   # drawn for this project; appended after SHOP
from avatars_original import STARTERS   # more of the free set; after everything, never sold
from avatars_original import YOU        # the customisable trainer, after the starters
from avatars_costume import costumes    # composed creature costumes, after ORIGINAL
from avatars_crossover import crossovers   # other franchises' characters, after YOU (own size)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'avatars.h')
BASE = 'https://raw.githubusercontent.com/pret/%s/master/gfx/sprites/%s.png'
OUT_NAMES = os.path.join(HERE, '..', 'avatar_names.h')
SIZE = 16

# (file, label shown in the UI, accent for shade 1)
# Eight, so the stored `avatar` byte still fits comfortably and cycling by tap
# stays short. Labels are unaccented ASCII: the bitmap font has nothing else.
PICKS = [
    ('red',           'RED',      0xEA87),   # the classic player
    ('blue',          'BLUE',     0x4C98),   # the rival
    ('girl',          'GIRL',     0xF454),
    ('cooltrainer_m', 'ACE M',    0x5DCD),
    ('cooltrainer_f', 'ACE F',    0xC2F0),
    ('hiker',         'HIKER',    0xB3C8),
    # NOT swimmer: it is drawn low in its cell (the character is in water),
    # which reads as a misaligned sprite once it is out of context.
    ('sailor',        'SAILOR',   0x7E3D),
    ('youngster',     'YOUNGSTER',0xFECB),
]

# The casino's prize avatars, APPEND-ONLY (see the docstring). (repo, file,
# label, accent). Labels <= 10 characters, unaccented ASCII.
RED, GSC = 'pokered', 'pokecrystal'
SHOP = [
    (RED, 'agatha', 'AGATHA', 0x9A5F), (RED, 'bruno', 'BRUNO', 0xC364),
    (RED, 'lorelei', 'LORELEI', 0x5D7F), (RED, 'lance', 'LANCE', 0xE8E4),
    (RED, 'koga', 'KOGA', 0x8A31), (RED, 'giovanni', 'GIOVANNI', 0x7BCF),
    (RED, 'oak', 'PROF OAK', 0xBDF7), (RED, 'daisy', 'DAISY', 0xFD20),
    (RED, 'mom', 'MOM', 0xFB2C), (RED, 'mr_fuji', 'MR FUJI', 0xA514),
    (RED, 'rocket', 'ROCKET', 0x4208), (RED, 'scientist', 'SCIENTIST', 0x867D),
    (RED, 'gambler', 'GAMBLER', 0xD6A0), (RED, 'gentleman', 'GENTLEMAN', 0x6B4D),
    (RED, 'beauty', 'BEAUTY', 0xF8B2), (RED, 'biker', 'BIKER', 0x52AA),
    (RED, 'rocker', 'ROCKER', 0xC81F), (RED, 'fisher', 'FISHER', 0x2D7F),
    (RED, 'fishing_guru', 'FISH GURU', 0x3D8E), (RED, 'super_nerd', 'NERD', 0x7E0F),
    (RED, 'channeler', 'CHANNELER', 0xB596), (RED, 'captain', 'CAPTAIN', 0x2A9F),
    (RED, 'cook', 'CHEF', 0xE30C), (RED, 'waiter', 'WAITER', 0x39E7),
    (RED, 'nurse', 'NURSE', 0xFC9F), (RED, 'clerk', 'CLERK', 0x2D5B),
    (RED, 'guard', 'GUARD', 0x3A7A), (RED, 'gym_guide', 'GYM GUIDE', 0xFC00),
    (RED, 'warden', 'WARDEN', 0x9C6A), (RED, 'safari_zone_worker', 'SAFARI', 0x8E4A),
    (RED, 'silph_president', 'PRESIDENT', 0x6B6D), (RED, 'silph_worker_m', 'SILPH M', 0x4E7F),
    (RED, 'silph_worker_f', 'SILPH F', 0xFB9A), (RED, 'brunette_girl', 'LASS', 0xF4D7),
    (RED, 'little_boy', 'LITTLE BOY', 0x5E7F), (RED, 'little_girl', 'LITTLEGIRL', 0xFB56),
    (RED, 'gameboy_kid', 'GAME KID', 0x8410), (RED, 'gramps', 'GRAMPS', 0xAD55),
    (RED, 'granny', 'GRANNY', 0xC618), (RED, 'balding_guy', 'BALD GUY', 0xCC83),
    (RED, 'middle_aged_man', 'DAD', 0x6C9F), (RED, 'middle_aged_woman', 'AUNT', 0xE34F),
    (RED, 'link_receptionist', 'TRADE DESK', 0xFA8A), (RED, 'bike_shop_clerk', 'BIKE SHOP', 0x3666),
    (GSC, 'chris', 'GOLD', 0xFEA0), (GSC, 'kris', 'KRIS', 0x4E9F),
    (GSC, 'rival', 'SILVER', 0xC9A6), (GSC, 'red', 'CHAMP RED', 0xF800),
    (GSC, 'elm', 'PROF ELM', 0x9EF7), (GSC, 'bill', 'BILL', 0x7C1F),
    (GSC, 'kurt', 'KURT', 0x8C51), (GSC, 'falkner', 'FALKNER', 0x6D9F),
    (GSC, 'bugsy', 'BUGSY', 0x9E62), (GSC, 'whitney', 'WHITNEY', 0xFBDF),
    (GSC, 'morty', 'MORTY', 0x781F), (GSC, 'chuck', 'CHUCK', 0xB2A5),
    (GSC, 'jasmine', 'JASMINE', 0xA48A), (GSC, 'pryce', 'PRYCE', 0x9F1F),
    (GSC, 'clair', 'CLAIR', 0x2A1F), (GSC, 'will', 'WILL', 0xB01F),
    (GSC, 'karen', 'KAREN', 0x5ACB), (GSC, 'brock', 'BROCK', 0xA3C6),
    (GSC, 'misty', 'MISTY', 0x2C9F), (GSC, 'surge', 'LT SURGE', 0xFFE0),
    (GSC, 'erika', 'ERIKA', 0x4E66), (GSC, 'janine', 'JANINE', 0xA11F),
    (GSC, 'sabrina', 'SABRINA', 0xD99F), (GSC, 'blaine', 'BLAINE', 0xF9E0),
    (GSC, 'rocket_girl', 'ROCKET F', 0x4A49), (GSC, 'black_belt', 'BLACK BELT', 0xD69A),
    (GSC, 'bug_catcher', 'BUG KID', 0x7EE0), (GSC, 'kimono_girl', 'KIMONO', 0xE8C9),
    (GSC, 'lass', 'LASS GSC', 0xFAD2), (GSC, 'officer', 'OFFICER', 0x2A7F),
    (GSC, 'pharmacist', 'PHARMACIST', 0x5F3F), (GSC, 'pokefan_m', 'POKEFAN M', 0xFA60),
    (GSC, 'pokefan_f', 'POKEFAN F', 0xFCB2), (GSC, 'sage', 'SAGE', 0xAC86),
    (GSC, 'teacher', 'TEACHER', 0xF5B6), (GSC, 'twin', 'TWINS', 0xFAAA),
    (GSC, 'elder', 'ELDER', 0x94B2), (GSC, 'cal', 'CAL', 0x5D1F),
]
DEALER = (RED, 'rocket')   # all six frames: the casino dealer animates

# Shop entries that are NOT SOLD any more, but keep their index (ownership is a
# bit per shop index, so removing one would hand every later purchase to its
# neighbour). CHAMP RED is GSC's red.png, pixel-for-pixel the free rank avatar
# RED: the rank avatars stay rank-only, so it is not a prize.
RETIRED = {'CHAMP RED'}

AVFX_AURA = 1
OUTLINE = 0x18C4     # same ink as the rest of the UI
HIGHLIGHT = 0xEF7D   # a soft off-white, so it reads on the cream card


def read_png_2bpp(data, path):
    """pokered's sprites are 2-bit greyscale; the shared decoder in
    gen_backs.py only handles 8-bit channels, so this one is separate."""
    pos, idat, w, h, bd, ct = 8, b'', 0, 0, 0, 0
    while pos < len(data):
        ln = struct.unpack('>I', data[pos:pos + 4])[0]
        typ = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + ln]
        if typ == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', chunk[:10])
        elif typ == b'IDAT':
            idat += chunk
        pos += 12 + ln
    if bd != 2 or ct != 0:
        raise SystemExit('%s: expected 2-bit greyscale, got bd=%d ct=%d'
                         % (path, bd, ct))
    raw = zlib.decompress(idat)
    bpr = (w * bd + 7) // 8
    rows, prev, i = [], bytearray(bpr), 0
    for _ in range(h):
        f = raw[i]; i += 1
        line = bytearray(raw[i:i + bpr]); i += bpr
        for x in range(len(line)):
            a = line[x - 1] if x >= 1 else 0
            b = prev[x]
            c = prev[x - 1] if x >= 1 else 0
            if f == 1:   line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        rows.append(bytes(line)); prev = line
    px = []
    for line in rows:
        px.append([(line[x // 4] >> ((3 - (x % 4)) * 2)) & 3 for x in range(w)])
    return w, h, px


LOCAL = None


def fetch(repo, fn):
    if LOCAL:
        return open(os.path.join(LOCAL, repo, 'gfx', 'sprites', fn + '.png'), 'rb').read()
    url = BASE % (repo, fn)
    sys.stderr.write('fetching %s\n' % url)
    # curl rather than urllib: the stock python.org build on macOS ships
    # without a usable CA bundle and fails to verify github.com.
    r = subprocess.run(['curl', '-fsSL', url], capture_output=True)
    if r.returncode != 0:
        raise SystemExit('fetch failed (%s): %s' % (url, r.stderr.decode().strip()))
    return r.stdout


def frames(repo, fn, accent, count=1):
    """Index arrays for the first `count` 16x16 frames of a sheet."""
    w, h, px = read_png_2bpp(fetch(repo, fn), fn)
    if w != SIZE or h % SIZE:
        raise SystemExit('%s: expected a %dpx-wide sheet, got %dx%d' % (fn, SIZE, w, h))
    out = []
    for f in range(min(count, h // SIZE)):
        idx = []
        for y in range(SIZE):
            for x in range(SIZE):
                v = px[f * SIZE + y][x]
                idx.append(0xFF if v == 3 else v)   # 3 = white = transparent
        out.append(idx)
    return out


def emit(out, sym, pal, idx, w=None):
    w = w or SIZE
    out.append('static const uint16_t %s_PAL[%d] = { %s };'
               % (sym, len(pal), ', '.join('0x%04X' % c for c in pal)))
    out.append('static const uint8_t %s_IDX[%d] = {' % (sym, len(idx)))
    for r in range(len(idx) // w):
        row = idx[r * w:(r + 1) * w]
        out.append('  ' + ' '.join('0x%02X,' % v for v in row))
    out.append('};')
    out.append('')


def main():
    global LOCAL
    if '--local' in sys.argv:
        LOCAL = sys.argv[sys.argv.index('--local') + 1]
    outPath = OUT
    namesPath = OUT_NAMES
    if '--out' in sys.argv:
        d = sys.argv[sys.argv.index('--out') + 1]
        outPath = os.path.join(d, 'avatars.h')
        namesPath = os.path.join(d, 'avatar_names.h')
    out = ['// GENERATED by tools/gen_avatars.py - do not edit',
           '#pragma once',
           '#include <stdint.h>',
           '#include "avatar_names.h"   // the counts and the shop names',
           '',
           '// Player avatars: Gen 1 overworld sprites, 16x16, 8bpp indexed with',
           '// a four-entry RGB565 palette. Index 0xFF is transparent.',
           '//',
           '// See CREDITS.md: this is Nintendo / Game Freak artwork from a',
           '// disassembly, not licensed material like the PMD sprites are --',
           '// EXCEPT the entries marked "original art", which were drawn for this',
           '// project (tools/avatars_original.py, tools/avatars_costume.py).',
           '']
    names = []
    sizes = {}   # sym -> (w, h) for anything that is not AVATAR_PX square
    for fn, label, accent in PICKS:
        sym = 'AV_' + fn.upper()
        names.append((sym, label, fn, 0))
        emit(out, sym, [OUTLINE, accent, HIGHLIGHT], frames(RED, fn, accent)[0])
    shopNames = []
    for repo, fn, label, accent in SHOP:
        sym = 'AV_%s_%s' % ('G1' if repo == RED else 'G2', fn.upper())
        names.append((sym, label, repo + '/' + fn, 0))
        shopNames.append(label)
        emit(out, sym, [OUTLINE, accent, HIGHLIGHT], frames(repo, fn, accent)[0])
    # Original art, after every pret entry so no shop index moves.
    CODE = {'.': 0xFF, '#': 0, 'a': 1, 'w': 2}
    seen = {}
    towerShop = []   # shop indices of the legendary costumes: tower prizes, never sold
    for k, (label, accent, grid, *rest) in enumerate(ORIGINAL + costumes()):
        legendary = bool(rest and rest[0])
        if len(grid) != SIZE or any(len(r) != SIZE for r in grid) or len(label) > 10 \
                or not label.isascii() or label in shopNames:
            raise SystemExit('original avatar %s: not 16x16, label too long/non-ASCII or taken' % label)
        key = (accent, tuple(grid))
        if key in seen:
            raise SystemExit('original avatar %s draws exactly like %s' % (label, seen[key]))
        seen[key] = label
        sym = 'AV_OG%03d' % k
        if legendary:
            towerShop.append(len(shopNames))
        names.append((sym, label, 'original art' + (', legendary: aura' if legendary else ''), AVFX_AURA if legendary else 0))
        shopNames.append(label)
        emit(out, sym, [OUTLINE, accent, HIGHLIGHT], [CODE[c] for r in grid for c in r])
    # The extra starters: free from rank 1, so NOT shop entries -- they come
    # after every shop index (casino ownership bits must never move) and carry
    # a full palette, since skin, hair and clothes are separate colours.
    starterFirst = len(names)
    starterLabels = set()
    for k, (label, pal, grid) in enumerate(STARTERS):
        if len(grid) != SIZE or any(len(r) != SIZE for r in grid) or len(label) > 10 \
                or not label.isascii() or label in shopNames or label in starterLabels \
                or label in [p[1] for p in PICKS]:
            raise SystemExit('starter avatar %s: not 16x16, label too long/non-ASCII or taken' % label)
        starterLabels.add(label)
        used = sorted({c for r in grid for c in r} - {'.', '#'})
        missing = [c for c in used if c not in pal]
        if missing:
            raise SystemExit('starter avatar %s: no colour for %s' % (label, ''.join(missing)))
        code = {'.': 0xFF, '#': 0}
        code.update({c: i + 1 for i, c in enumerate(used)})
        sym = 'AV_ST%02d' % k
        names.append((sym, label, 'original art, starter', 0))
        emit(out, sym, [OUTLINE] + [pal[c] for c in used], [code[c] for r in grid for c in r])
    # YOU: composed at draw time from the wardrobe; this is its stand-in art.
    youIndex = len(names)
    label, pal, grid = YOU
    if len(grid) != SIZE or any(len(r) != SIZE for r in grid):
        raise SystemExit('YOU: not 16x16')
    used = sorted({c for r in grid for c in r} - {'.', '#'})
    code = {'.': 0xFF, '#': 0}
    code.update({c: i + 1 for i, c in enumerate(used)})
    names.append(('AV_YOU', label, 'original art, the wardrobe trainer (composed at draw time)', 0))
    emit(out, 'AV_YOU', [OUTLINE] + [pal[c] for c in used], [code[c] for r in grid for c in r])
    # Crossovers: Game Corner prizes, but appended AFTER YOU, because the
    # starters and YOU already sit right after the first shop block and their
    # indices are stored raw in NVS. So the shop is two blocks of AVATARS[],
    # and shopAvatar() / shopIndexOfAvatar() in avatar_names.h are the ONLY
    # mapping between them -- nothing may add SHOP_AVATAR_FIRST to an index.
    # They keep their own size and palette (avatars_crossover.py).
    shopSplit = len(shopNames)
    shop2First = len(names)
    taken = set(shopNames) | starterLabels | {p[1] for p in PICKS} | {YOU[0]}
    for k, (label, w, h, pal, idx) in enumerate(crossovers()):
        if len(label) > 10 or not label.isascii() or label in taken:
            raise SystemExit('crossover avatar %s: label too long/non-ASCII or taken' % label)
        taken.add(label)
        sym = 'AV_XO%02d' % k
        sizes[sym] = (w, h)
        names.append((sym, label, 'crossover fan art (avatars_crossover.py), %dx%d' % (w, h), 0))
        shopNames.append(label)
        emit(out, sym, pal, idx, w)
    # Avatar indices are uint16_t (v3.45); the real ceiling is the casino's
    # ownership bitmask, CASINO_AVATARS_MAX in casino.h, which casino.cpp
    # static_asserts against SHOP_AVATAR_COUNT.
    # The dealer: every frame, back to back (frame f at f*256).
    dl = frames(DEALER[0], DEALER[1], 0x4208, 6)
    emit(out, 'AV_DEALER', [OUTLINE, 0x4208, HIGHLIGHT], [v for f in dl for v in f])
    out.append('#define DEALER_FRAMES %d' % len(dl))
    out.append('')
    out.append('struct AvatarArt {')
    out.append('  const uint8_t *idx;')
    out.append('  const uint16_t *pal;')
    out.append('  const char *name;')
    out.append('  uint8_t fx;               // AVFX_* bits: how drawAvatar() dresses it')
    out.append('  uint8_t w, h;             // AVATAR_PX square, or a crossover\'s own size (fitted)')
    out.append('};')
    out.append('#define AVFX_AURA %d             // a glowing, pulsing outline (the legendary costumes)' % AVFX_AURA)
    out.append('#define AVATAR_PX %d' % SIZE)
    out.append('static const AvatarArt AVATARS[AVATAR_COUNT] = {')
    for sym, label, src, fx in names:
        w, h = sizes.get(sym, (SIZE, SIZE))
        out.append('  { %s_IDX, %s_PAL, "%s", %d, %d, %d },   // %s' % (sym, sym, label, fx, w, h, src))
    out.append('};')
    out.append('')
    open(outPath, 'w').write('\n'.join(out))
    # Names only, for code that must not link the pixels (casino.cpp, tests).
    nm = ['// GENERATED by tools/gen_avatars.py - do not edit',
          '#pragma once',
          '// The Game Corner prize avatars: shop index i is AVATARS[shopAvatar(i)].',
          '// Code that only needs to COUNT or NAME avatars includes this, not avatars.h',
          '// (which carries ~25 KB of pixels into every translation unit that includes it).',
          '#define AVATAR_BASE_COUNT %d   // the free set; index order FROZEN (stored raw in NVS)' % len(PICKS),
          '#define AVATAR_COUNT %d' % len(names),
          '#define SHOP_AVATAR_FIRST AVATAR_BASE_COUNT',
          '#define SHOP_AVATAR_COUNT %d' % len(shopNames),
          'static const char *const SHOP_AVATAR_NAMES[SHOP_AVATAR_COUNT] = {']
    for i in range(0, len(shopNames), 6):
        nm.append('  ' + ' '.join('"%s",' % n for n in shopNames[i:i + 6]))
    nm.append('};')
    ret = [i for i, n in enumerate(shopNames) if n in RETIRED]
    nm.append('// Kept in the frozen order above but never sold: each duplicates a rank avatar.')
    nm.append('#define SHOP_AVATAR_RETIRED_COUNT %d' % len(ret))
    nm.append('static const uint16_t SHOP_AVATAR_RETIRED[SHOP_AVATAR_RETIRED_COUNT] = { %s };'
              % ', '.join('%d' % i for i in ret) + '   // ' + ', '.join(shopNames[i] for i in ret))
    nm.append('// The legendary costumes: won at the Battle Tower (TOWER_AVATAR_FLOOR), never sold.')
    nm.append('#define TOWER_AVATAR_COUNT %d' % len(towerShop))
    nm.append('static const uint16_t TOWER_AVATARS[TOWER_AVATAR_COUNT] = {   // shop indices')
    for i in range(0, len(towerShop), 12):
        nm.append('  ' + ' '.join('%d,' % v for v in towerShop[i:i + 12]))
    nm.append('};')
    nm.append('// The extra starters: free from rank 1 (rank.cpp), never sold, after every shop index.')
    nm.append('#define STARTER_AVATAR_FIRST %d' % starterFirst)
    nm.append('#define STARTER_AVATAR_COUNT %d' % len(STARTERS))
    nm.append('// The customisable trainer: drawn from the wardrobe (wardrobe.cpp), free, never sold.')
    nm.append('#define AVATAR_YOU %d' % youIndex)
    nm.append('// The shop is TWO blocks of AVATARS[]: shop indices below SHOP_AVATAR_SPLIT sit')
    nm.append('// right after the free set; the rest (the crossovers) come after AVATAR_YOU,')
    nm.append('// because the starters and YOU were already stored by index. These two are the')
    nm.append('// only mapping -- never add SHOP_AVATAR_FIRST to a shop index by hand.')
    nm.append('#define SHOP_AVATAR_SPLIT %d' % shopSplit)
    nm.append('#define SHOP2_AVATAR_FIRST %d' % shop2First)
    nm.append('static inline uint16_t shopAvatar(uint16_t i) {')
    nm.append('  return i < SHOP_AVATAR_SPLIT ? (uint16_t)(SHOP_AVATAR_FIRST + i)')
    nm.append('                               : (uint16_t)(SHOP2_AVATAR_FIRST + (i - SHOP_AVATAR_SPLIT));')
    nm.append('}')
    nm.append('static inline int shopIndexOfAvatar(uint16_t a) {   // -1: not a shop avatar')
    nm.append('  if (a >= SHOP_AVATAR_FIRST && a < SHOP_AVATAR_FIRST + SHOP_AVATAR_SPLIT) return a - SHOP_AVATAR_FIRST;')
    nm.append('  if (a >= SHOP2_AVATAR_FIRST && a < SHOP2_AVATAR_FIRST + (SHOP_AVATAR_COUNT - SHOP_AVATAR_SPLIT))')
    nm.append('    return SHOP_AVATAR_SPLIT + (a - SHOP2_AVATAR_FIRST);')
    nm.append('  return -1;')
    nm.append('}')
    nm.append('')
    open(namesPath, 'w').write('\n'.join(nm))
    sys.stderr.write('wrote %s (%d avatars, %d in the shop)\n' % (outPath, len(names), len(shopNames)))


if __name__ == '__main__':
    main()
