"""Fan-made trainer avatars, from two sources, in this order:

  1. RegentofRaios's DeviantArt gallery (CROSSOVERS below): characters from
     other franchises drawn as Pokemon Black/White overworld sprites, the
     Pokemon Conquest warlords, and HeartGold/SoulSilver-style overworld sheets
     of the Kanto and Johto leaders, the Elite 4, Silver, Eusine and the Rocket
     executives.
  2. PurpleZaffre's "ULTIMATE Gen 4 / Gen 5 Overworlds Pack" on Eevee Expo
     (avatars_zaffre.py, PACKS below): every human overworld of
     Diamond/Pearl/Platinum, HeartGold/SoulSilver, Black/White and B2W2, the
     Diamond/Pearl beta trainers and PurpleZaffre's own Gen 4-style characters.
     Credit PurpleZaffre wherever these are seen.

    https://www.deviantart.com/regentofraios/gallery/all

Imported by gen_avatars.py, which appends them AFTER every existing avatar
(the free set, the shop, the starters and CUSTOM): avatar indices are stored
raw in NVS and shop ownership is a bit per shop index, so nothing that already
exists may move. They are Game Corner prizes, shop indices from
SHOP_AVATAR_SPLIT on -- see shopAvatar() in avatar_names.h.

Unlike every other avatar these are NOT 16x16 three-colour sprites: they keep
the artist's own pixels and palette (about 22x30, up to ~40 colours), and
drawAvatar() fits them into the same square the others fill. Squeezing them
into the Gen 1 format would have thrown away exactly what makes them
recognisable.

What this file does to the art, and nothing else:
  * every sheet is published pixel-doubled; it is reduced back to its own
    pixels (the offset of the 2x2 grid is found, not assumed);
  * a WALKING SHEET ('ow': 4x4 frames, rows = facing) gives its first frame,
    the front-facing stand, without its half-transparent drop shadow. Where the sheet has an opaque background it is
    keyed out by flood fill from the frame's edges, so a white eye or
    highlight INSIDE the sprite stays white;
  * EVIL RYU is the exception: his body is drawn at 4x and his fire at 2x, in
    a 188x174 sheet that is mostly flames. He is reduced to the same scale as
    RYU and the fire is trimmed to what hugs him -- two pixels to the sides,
    seven above -- so the aura still reads as flames rising off him without
    shrinking him to a dot inside the square.

The images are fetched with curl through DeviantArt's RSS feed (the image
URLs are signed and expire, so they cannot be pinned) and cached in
tools/crossover_cache/, which is gitignored.

SOURCE AND LICENCE: fan art of Capcom, Square Enix, Type-Moon and Shueisha
characters in Game Freak's sprite style, by a DeviantArt user, and sprites
ripped from the Nintendo DS games by PurpleZaffre, who asks for credit and that
the pack itself not be redistributed. NOT licensed to this project -- see
CREDITS.md. Ship this script, not its output.
"""
import os
import re
import struct
import subprocess
import time
import zipfile

from avatars_zaffre import ZAFFRE
import zlib
from collections import Counter

FEED = ('https://backend.deviantart.com/rss.xml?type=deviation'
        '&q=gallery%3Aregentofraios&offset={off}')
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'crossover_cache')

# (label, deviation id, how the sheet is reduced). The ORDER IS FROZEN once
# shipped (shop ownership is a bit per index): append new ones at the end.
# A trailing "+" marks a character the game already has as a Gen 1/2 avatar:
# this is the HeartGold/SoulSilver-style version of them.
#
# Not taken, on purpose: the battle poses, VS portraits and animation strips
# (the same characters again, as busts or action frames -- not a standing
# avatar) and "The Final Battle", an illustration rather than a sprite.
CROSSOVERS = [
    ('SEPHIROTH',  1079773052, 'x2'),
    ('KUJA',       1079772185, 'x2'),
    ('GILGAMESH',  1079771765, 'x2'),
    ('KIREI',      1079771112, 'x2'),
    ('MADARA',     1079768762, 'x2'),
    ('MADARA WAR', 1079770346, 'x2'),
    ('RYU',        1084852485, 'x2'),
    ('EVIL RYU',   1084853078, 'evilryu'),
    ('SAGAT',      1085039941, 'x2'),
    # Pokemon Conquest
    ('RANMARU',    882572793, 'x2'),
    ('YASUKE',     882571987, 'x2'),
    ('IEYASU',     882571329, 'x2'),
    ('MOTOYASU',   882570721, 'x2'),
    ('MITSUHIDE',  882377798, 'x2'),
    ('NOHIME',     882375443, 'x2'),
    ('NOBUNAGA',   882375022, 'x2'),
    ('NOBU FOOL',  882374477, 'x2'),
    ('NAGAMASA',   882374148, 'x2'),
    ('OICHI',      882373912, 'x2'),
    ('HERO LV2',   882373707, 'x2'),
    ('HEROINE',    882373248, 'x2'),
    ('HERO',       882372081, 'x2'),
    # HeartGold/SoulSilver-style overworld sheets
    ('ROCKET EXM', 636450698, 'ow'),
    ('ROCKET F+',  636450697, 'ow'),
    ('ARCHER',     636447555, 'ow'),
    ('ARCHER HAT', 636447548, 'ow'),
    ('ARIANA',     636447380, 'ow'),
    ('PETREL',     636447230, 'ow'),
    ('PROTON',     636447089, 'ow'),
    ('SILVER+',    636446888, 'ow'),
    ('EUSINE',     636446783, 'ow'),
    ('KAREN+',     636446678, 'ow'),
    ('KOGA+',      636446569, 'ow'),
    ('WILL+',      636446482, 'ow'),
    ('CLAIR CAPE', 636446355, 'ow'),
    ('CLAIR+',     636446353, 'ow'),
    ('PRYCE+',     636446233, 'ow'),
    ('JASMINE+',   636446155, 'ow'),
    ('CHUCK+',     636446055, 'ow'),
    ('MORTY+',     636445982, 'ow'),
    ('WHITNEY+',   636445850, 'ow'),
    ('BUGSY+',     636445764, 'ow'),
    ('FALKNER+',   636445583, 'ow'),
    ('BLUE+',      636445493, 'ow'),
    ('LANCE+',     636445297, 'ow'),
    ('AGATHA+',    636445106, 'ow'),
    ('BRUNO+',     636444981, 'ow'),
    ('LORELEI+',   636444761, 'ow'),
    ('GIOVANNI+',  636444620, 'ow'),
    ('BLAINE+',    636444138, 'ow'),
    ('SABRINA+',   636444045, 'ow'),
    ('JANINE+',    636443906, 'ow'),
    ('ERIKA+',     636443761, 'ow'),
    ('LT SURGE+',  636443608, 'ow'),
    ('MISTY+',     636443343, 'ow'),
    ('BROCK+',     636441844, 'ow'),
]

# PurpleZaffre's two Eevee Expo packs ("ULTIMATE Gen 4/5 Overworlds Pack",
# eeveeexpo.com/resources/609 and /619). The packs are zips on MediaFire, whose
# direct link comes from a different numbered server on every visit -- so
# _fetch_pack() reads it off the page each time. If that host is not reachable
# (a limited network), download the two zips by hand into tools/crossover_cache/
# under these names. Their author asks that the PACK not be redistributed:
# the cache is gitignored, and only the converted avatars are built from it.
PACKS = {
    'gen4': ('zaffre_gen4.zip', 'https://www.mediafire.com/file/vmrjeckhz5dclzr/'
             'Ultimate_Gen_4_Overworlds_Pack.zip/file', 'Ultimate Gen 4 Overworlds Pack/'),
    'gen5': ('zaffre_gen5.zip', 'https://www.mediafire.com/file/ft6mdl0fuxsfxqj/'
             'Ultimate+Gen+5+Overworlds+Pack.zip/file', 'Ultimate Gen 5 Overworlds Pack/'),
}

# Evil Ryu's flame colours, told apart from his own reds (eyes, gloves).
FIRE = {(255, 118, 84), (255, 51, 0), (255, 158, 133)}


def _curl(url):
    # DeviantArt refuses the odd request under load (seen: one 4xx between two
    # 200s), so retry with backoff before calling it a failure.
    for wait in (0, 5, 15, 45):
        time.sleep(wait)
        r = subprocess.run(['curl', '-sSfL', '--max-time', '60', url], capture_output=True)
        if r.returncode == 0:
            return r.stdout
    raise SystemExit('fetch failed after retries: %s\n%s' % (url[:100], r.stderr.decode(errors='replace')))


def _image_urls():
    """deviation id -> current (signed) image URL, from the gallery's feed."""
    urls = {}
    for off in range(0, 600, 60):
        xml = _curl(FEED.format(off=off)).decode('utf-8', 'replace')
        items = re.findall(r'<item>(.*?)</item>', xml, re.S)
        for it in items:
            link = re.search(r'<link>[^<]*-(\d+)</link>', it)
            img = re.search(r'<media:content url="([^"]+)"', it)
            if link and img:
                urls[int(link.group(1))] = img.group(1).replace('&amp;', '&')
        if len(items) < 60:
            break
    return urls


def _fetch_all():
    os.makedirs(CACHE, exist_ok=True)
    need = [d for _, d, _ in CROSSOVERS
            if not os.path.exists(os.path.join(CACHE, '%d.png' % d))]
    if need:
        urls = _image_urls()
        for d in need:
            if d not in urls:
                raise SystemExit('crossover %d: not in the gallery feed any more' % d)
            open(os.path.join(CACHE, '%d.png' % d), 'wb').write(_curl(urls[d]))


def _fetch_pack(key):
    name, page, _ = PACKS[key]
    path = os.path.join(CACHE, name)
    if os.path.exists(path):
        return path
    os.makedirs(CACHE, exist_ok=True)
    html = _curl(page).decode('utf-8', 'replace')
    m = re.search(r'https://download\d+\.mediafire\.com/[^"\s]+', html)
    if not m:
        raise SystemExit('PurpleZaffre pack %s: no download link on %s -- download it by hand '
                         'into %s' % (key, page, path))
    data = _curl(m.group(0))
    if data[:2] != b'PK':
        raise SystemExit('PurpleZaffre pack %s: the download is not a zip' % key)
    open(path, 'wb').write(data)
    return path


def _read_png(data):
    """RGBA rows from an 8-bit paletted or truecolour(+alpha) PNG."""
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise SystemExit('not a PNG')
    pos, idat, plte, trns = 8, b'', None, b''
    while pos < len(data):
        n, typ = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if typ == b'IHDR':
            w, h, depth, ctype, _, _, interlace = struct.unpack('>IIBBBBB', body)
        elif typ == b'PLTE':
            plte = body
        elif typ == b'tRNS':
            trns = body
        elif typ == b'IDAT':
            idat += body
        pos += 12 + n
    if depth != 8 or interlace or ctype not in (2, 3, 6):
        raise SystemExit('unsupported PNG (depth %d, type %d, interlace %d)' % (depth, ctype, interlace))
    bpp = {2: 3, 3: 1, 6: 4}[ctype]
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev, i = [], bytearray(stride), 0
    for _ in range(h):
        f = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        row = []
        for x in range(w):
            if ctype == 3:
                k = line[x]
                r, g, b = plte[3 * k:3 * k + 3]
                row.append((r, g, b, trns[k] if k < len(trns) else 255))
            elif ctype == 2:
                row.append(tuple(line[3 * x:3 * x + 3]) + (255,))
            else:
                row.append(tuple(line[4 * x:4 * x + 4]))
        rows.append(row)
    return rows


def _opaque(p):
    # FULLY opaque only. The walking sheets draw the drop shadow as a
    # half-transparent grey (alpha 128); the panel has no alpha, so keeping it
    # painted a solid black blob under every character's feet.
    return p[3] == 255


def _reduce(rows, k, offsets_from=None):
    """Undo a k x k upscale. The grid offset is the one under which every k x k
    block is a single colour; offsets_from() limits which pixels must agree."""
    h, w = len(rows), len(rows[0])
    best = None
    for oy in range(k):
        for ox in range(k):
            tot = uni = 0
            for y in range(oy, h - k + 1, k):
                for x in range(ox, w - k + 1, k):
                    blk = [rows[y + dy][x + dx] for dy in range(k) for dx in range(k)]
                    if offsets_from and not any(offsets_from(b) for b in blk):
                        continue
                    tot += 1
                    uni += len(set(blk)) == 1
            score = uni / max(tot, 1)
            if best is None or score > best[0]:
                best = (score, ox, oy)
    _, ox, oy = best
    out = []
    for y in range(oy, h - k + 1, k):
        row = []
        for x in range(ox, w - k + 1, k):
            blk = [rows[y + dy][x + dx] for dy in range(k) for dx in range(k)]
            vis = [b for b in blk if _opaque(b)]
            if offsets_from:   # the body wins its block; fire only fills the rest
                body = [b for b in vis if offsets_from(b)]
                if len(body) * 2 >= len(blk):
                    vis = body
            row.append(Counter(vis).most_common(1)[0][0] if len(vis) * 2 >= len(blk) else (0, 0, 0, 0))
        out.append(row)
    return out


def _crop(rows):
    ys = [y for y, r in enumerate(rows) if any(_opaque(p) for p in r)]
    xs = [x for x in range(len(rows[0])) if any(_opaque(r[x]) for r in rows)]
    return [r[xs[0]:xs[-1] + 1] for r in rows[ys[0]:ys[-1] + 1]]


def _key_background(rows):
    """An opaque background (some sheets have white or a flat colour) becomes
    transparent: flood fill from the edges over the corner's exact colour, so
    the same colour INSIDE the sprite -- an eye, a highlight -- is kept."""
    h, w = len(rows), len(rows[0])
    bg = rows[0][0]
    if not _opaque(bg):
        return rows
    rows = [list(r) for r in rows]
    stack = [(x, y) for x in range(w) for y in (0, h - 1)] + \
            [(x, y) for y in range(h) for x in (0, w - 1)]
    while stack:
        x, y = stack.pop()
        if 0 <= x < w and 0 <= y < h and rows[y][x] == bg:
            rows[y][x] = (0, 0, 0, 0)
            stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return rows


def _front_frame(rows):
    """The first frame of a 4x4 walking sheet: facing the viewer, standing."""
    h, w = len(rows), len(rows[0])
    return _key_background([r[:w // 4] for r in rows[:h // 4]])


def _evil_ryu(rows):
    is_body = lambda p: _opaque(p) and p[:3] not in FIRE
    small = _reduce(rows, 4, is_body)
    h, w = len(small), len(small[0])
    body = [[is_body(p) for p in r] for r in small]
    SIDE, UP = 2, 7

    def near(x, y):
        for yy in range(max(0, y - SIDE), min(h, y + UP + 1)):
            for xx in range(max(0, x - SIDE), min(w, x + SIDE + 1)):
                if body[yy][xx]:
                    return True
        return False
    return [[p if _opaque(p) and (body[y][x] or near(x, y)) else (0, 0, 0, 0)
             for x, p in enumerate(r)] for y, r in enumerate(small)]


def _rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def _sources():
    """(label, decoded rows, how) for every avatar, in the frozen shop order:
    the DeviantArt gallery first, then PurpleZaffre's packs (avatars_zaffre.py)."""
    _fetch_all()
    for label, dev, how in CROSSOVERS:
        yield label, _read_png(open(os.path.join(CACHE, '%d.png' % dev), 'rb').read()), how
    zips = {k: zipfile.ZipFile(_fetch_pack(k)) for k in PACKS}
    for label, pack, inner in ZAFFRE:
        z, prefix = zips[pack], PACKS[pack][2]
        try:
            data = z.read(prefix + inner)
        except KeyError:
            raise SystemExit('PurpleZaffre pack %s has no %s' % (pack, inner))
        yield label, _read_png(data), 'ow'


def crossovers():
    """[(label, w, h, palette RGB565, pixel indices 0xFF = transparent)]"""
    out = []
    for label, rows, how in _sources():
        if how == 'evilryu':
            rows = _evil_ryu(rows)
        elif how == 'ow':
            rows = _reduce(_front_frame(rows), 2)
        else:
            rows = _reduce(rows, 2)
        rows = _crop(rows)
        h, w = len(rows), len(rows[0])
        if w > 64 or h > 64:
            raise SystemExit('crossover %s: %dx%d, past the 64 px a draw may assume' % (label, w, h))
        pal, idx = [], []
        for r in rows:
            for p in r:
                if not _opaque(p):
                    idx.append(0xFF)
                    continue
                c = _rgb565(*p[:3])
                if c not in pal:
                    pal.append(c)
                idx.append(pal.index(c))
        if len(pal) > 255:
            raise SystemExit('crossover %s: %d colours, past 8bpp' % (label, len(pal)))
        out.append((label, w, h, pal, idx))
    return out
