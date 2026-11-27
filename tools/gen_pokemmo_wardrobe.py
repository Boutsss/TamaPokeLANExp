#!/usr/bin/env python3
"""The wardrobe's art: PokeMMO's cosmetics, cut out layer by layer -> pokemmo_art.h

    python tools/gen_pokemmo_wardrobe.py           # fetch (cached) and emit
    python tools/gen_pokemmo_wardrobe.py --offline # emit from the cache only

SOURCE AND LICENCE -- read CREDITS.md before publishing anything built from
this. The pixels are PokeMMO's overworld trainer and cosmetic sprites, rendered
by the same public image service pokemmohub.com's Cosmetics Helper uses
(apis.fiereu.de/pokemmoclothes). They are NOT licensed to this project; the
owner chose to use them for a private, non-commercial build shared with a few
friends, exactly as with the Gen 1 trainer sprites. Do not ship them further.

How it works:
  - The catalogue (every cosmetic, its slot, its name, whether it is one of the
    free starting items) is read from the hub's own page script, so nothing is
    restated here that could drift.
  - Every image is the FRONT overworld view at native size (scale 0: a 57x56
    canvas, the trainer ~17x23 px). Nothing is resized.
  - A cosmetic's LAYER is exactly the pixels it changes on the bare body (skin
    0, every slot empty). Composing the bare body plus layers in the game's
    order then redraws the trainer.
  - Skin tones and hair dyes keep the same pixels and change colours, so each
    is stored as a palette over the one pattern, verified pixel by pixel.
  - Every fetch is cached under tools/pokemmo_cache/ (gitignored): a rerun
    downloads nothing it already has.
"""
import io, json, os, re, subprocess, sys, time

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'pokemmo_art.h')
CACHE = os.path.join(HERE, 'pokemmo_cache')
PAGE = 'https://pokemmohub.com/tools/cosmetics/'
KNOWN_CHUNK = 'https://pokemmohub.com/fe52bad7bb0741b4ab6206dac6dbe5a01cd47dd8-85e3d7ffb84e2f6a97f7.js'
API = 'https://apis.fiereu.de/pokemmoclothes/v1/2/0/{skin}/{s6}/{s12}/{s4}/{s5}/{s8}/{s3}/{s2}/{s10}/{s9}/{s7}.png'

# PokeMMO's slot numbers (the hub's own table), the ones a standing trainer wears.
SLOTS = {2: 'hat', 3: 'hair', 4: 'eyes', 5: 'face', 6: 'back', 7: 'top', 8: 'gloves', 9: 'shoes', 10: 'legs'}
SKINS = 5
# The hair colours offered for free, as PokeMMO dye ids (the hub's dyeColors).
# 0xFF = the style's own undyed colour, always first.
HAIR_DYES = [('NATURAL', None), ('BLACK', 5), ('CHARCOAL', 4), ('GRAY', 3), ('WHITE', 1),
             ('DARK BROWN', 32), ('BROWN', 31), ('LIGHT BROWN', 30), ('BLONDE', 24), ('GOLD', 26),
             ('ORANGE', 28), ('RED', 34), ('DARK RED', 35), ('PINK', 37), ('BLUE', 7), ('GREEN', 16), ('PURPLE', 43)]

OFFLINE = '--offline' in sys.argv


def curl(url):
    r = subprocess.run(['curl', '-fsSL', '--max-time', '30', url], capture_output=True)
    if r.returncode != 0:
        raise RuntimeError('fetch failed (%s): %s' % (url, r.stderr.decode(errors='replace').strip()))
    return r.stdout


def cached(name, url, binary=True):
    path = os.path.join(CACHE, name)
    if os.path.exists(path):
        data = open(path, 'rb').read()
        if not name.endswith('.png') or data[:8] == b'\x89PNG\r\n\x1a\n':
            return data
        os.remove(path)   # a cached error page: never trust it, fetch again
    if OFFLINE:
        raise SystemExit('offline and not cached: %s' % name)
    data = curl(url)
    if name.endswith('.png') and data[:8] != b'\x89PNG\r\n\x1a\n':
        time.sleep(0.12)
        raise RuntimeError('not an image (%d bytes): %r' % (len(data), data[:60]))
    os.makedirs(CACHE, exist_ok=True)
    open(path, 'wb').write(data)
    time.sleep(0.12)   # one at a time, gently: it is somebody else's server
    return data


# ---------------------------------------------------------------- the catalogue
def js_string_at(text, i):
    """Decode the single-quoted JS string literal whose body starts at i."""
    out, n = [], len(text)
    while i < n:
        c = text[i]
        if c == "'":
            return ''.join(out)
        if c == '\\':
            d = text[i + 1]
            if d == 'n': out.append('\n'); i += 2
            elif d == 't': out.append('\t'); i += 2
            elif d == 'x': out.append(chr(int(text[i + 2:i + 4], 16))); i += 4
            elif d == 'u': out.append(chr(int(text[i + 2:i + 6], 16))); i += 6
            else: out.append(d); i += 2
            continue
        out.append(c); i += 1
    raise SystemExit('unterminated JS string')


def json_modules(js):
    for m in re.finditer(r"JSON\.parse\('", js):
        body = js_string_at(js, m.end())
        try:
            yield json.loads(body)
        except ValueError:
            continue


def catalogue():
    path = os.path.join(CACHE, 'catalogue.json')
    if os.path.exists(path):
        return json.load(open(path, encoding='utf-8'))
    if OFFLINE:
        raise SystemExit('offline and no cached catalogue')
    # The tables live in a script the page loads on demand, so it is not in the
    # HTML. --chunk URL overrides; else the one found 2026-11-26, then the HTML's.
    html = curl(PAGE).decode('utf-8', 'replace')
    srcs = sorted(set(re.findall(r'src="(/[^"]+\.js)"', html)))
    if '--chunk' in sys.argv:
        srcs.insert(0, sys.argv[sys.argv.index('--chunk') + 1])
    srcs.insert(0, KNOWN_CHUNK)
    meta = names = None
    for s in srcs:
        js = curl(s if s.startswith('http') else 'https://pokemmohub.com' + s).decode('utf-8', 'replace')
        if '"festival":' not in js or 'Sideswept' not in js:
            continue
        for mod in json_modules(js):
            if isinstance(mod, list) and mod and isinstance(mod[0], dict):
                if 'festival' in mod[0] and 'slot' in mod[0] and 'item_id' in mod[0]: meta = mod
                elif 'en_name' in mod[0] and 'id' in mod[0]: names = mod
        if meta and names:
            break
    if not (meta and names):
        raise SystemExit('could not find the cosmetics tables in the hub page script')
    byid = {n['id']: n for n in names}
    cat = []
    for m in meta:
        if m['slot'] not in SLOTS:
            continue
        n = byid.get(m['item_id'], {})
        name = (n.get('en_name') or '').strip() or 'ITEM %d' % m['item_id']
        cat.append({'id': m['item_id'], 'slot': m['slot'], 'attr': m['attribute'],
                    'free': 1 if n.get('description') == '初始' else 0, 'name': name})
    os.makedirs(CACHE, exist_ok=True)
    json.dump(cat, open(path, 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    return cat


def url_for(skin=0, **slots):
    v = {('s%d' % k): 0 for k in (2, 3, 4, 5, 6, 7, 8, 9, 10, 12)}
    for k, val in slots.items():
        v[k] = val
    return API.format(skin=skin, **v)


def rgba(data):
    im = Image.open(io.BytesIO(data)).convert('RGBA')
    return im.size, list(im.getdata())


def rgb565(p):
    r, g, b = p[0], p[1], p[2]
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def ascii_name(s):
    s = s.replace('é', 'e').replace('è', 'e').replace('’', "'")
    s = ''.join(c for c in s if 32 <= ord(c) < 127).strip()
    return re.sub(r'\s+', ' ', s)[:22] or '?'


def main():
    cat = catalogue()
    print('catalogue: %d wearable cosmetics (%d free)' % (len(cat), sum(c['free'] for c in cat)))
    # ---- the bare body, per skin
    (W, H), base = rgba(cached('base_s0.png', url_for(0)))
    skins = [rgba(cached('base_s%d.png' % s, url_for(s)))[1] for s in range(SKINS)]
    # ---- every layer: the pixels it changes on the bare body (skin 0)
    layers = []
    for i, c in enumerate(cat):
        key = 's%d' % c['slot']
        try:
            _, px = rgba(cached('item_%d.png' % c['id'], url_for(0, **{key: c['id']})))
        except RuntimeError as e:
            print('  skip %d %s: %s' % (c['id'], c['name'], e))
            continue
        pts = {}
        for j, (a, b) in enumerate(zip(base, px)):
            if a != b and b[3] >= 128:
                pts[j] = b
        if not pts:
            print('  skip %d %s: draws nothing on the front view' % (c['id'], c['name']))
            continue
        layers.append((c, pts))
        if (i + 1) % 50 == 0:
            print('  %d/%d' % (i + 1, len(cat)))
    # ---- the free hair styles in each dye: a palette over the undyed pattern
    dyes = {}
    for c, pts in layers:
        if c['slot'] != 3 or not c['free']:
            continue
        pals = []
        for dname, d in HAIR_DYES:
            if d is None:
                pals.append({j: p for j, p in pts.items()})
                continue
            try:
                _, px = rgba(cached('item_%d_d%d.png' % (c['id'], d), url_for(0, s3='%dd%d' % (c['id'], d))))
            except RuntimeError:
                pals.append(None)
                continue
            pals.append({j: px[j] for j in pts})
        dyes[c['id']] = pals
    # ---- the canvas: the union of everything drawn, so nothing is clipped
    xs, ys = [], []
    for j, p in enumerate(base):
        if p[3] >= 128: xs.append(j % W); ys.append(j // W)
    for c, pts in layers:
        for j in pts: xs.append(j % W); ys.append(j // W)
    X0, Y0, X1, Y1 = min(xs), min(ys), max(xs), max(ys)
    CW, CH = X1 - X0 + 1, Y1 - Y0 + 1
    print('canvas %dx%d at (%d,%d) of %dx%d' % (CW, CH, X0, Y0, W, H))

    pal_pool, data_pool = [], []

    def add_pal(cols):
        o = len(pal_pool); pal_pool.extend(cols); return o

    def encode(pts, cols_of):
        """bbox + RLE of (run, index) pairs, index 0 = transparent."""
        js = sorted(pts)
        bx0 = min(j % W for j in js); bx1 = max(j % W for j in js)
        by0 = min(j // W for j in js); by1 = max(j // W for j in js)
        cols, idx = [], {}
        for j in js:
            k = cols_of(j)
            if k not in idx:
                idx[k] = len(cols) + 1; cols.append(k)
        seq = []
        for y in range(by0, by1 + 1):
            for x in range(bx0, bx1 + 1):
                j = y * W + x
                seq.append(idx[cols_of(j)] if j in pts else 0)
        rle, i = [], 0
        while i < len(seq):
            v, n = seq[i], 1
            while i + n < len(seq) and seq[i + n] == v and n < 255: n += 1
            rle += [n, v]; i += n
        return (bx0 - X0, by0 - Y0, bx1 - bx0 + 1, by1 - by0 + 1), cols, rle

    # the bare body: one pattern, a palette per skin (checked)
    body = {j: p for j, p in enumerate(base) if p[3] >= 128}
    (bx, by, bw, bh), bcols, brle = encode(body, lambda j: rgb565(base[j]))
    skin_pals = []
    for s in range(SKINS):
        m = {}
        for j, p in body.items():
            k = rgb565(p); v = rgb565(skins[s][j])
            if m.setdefault(k, v) != v:
                raise SystemExit('skin %d recolours one shade two ways' % s)
        skin_pals.append(add_pal([m[k] for k in bcols]))
    body_ofs = len(data_pool); data_pool += brle

    items = []
    for c, pts in layers:
        (lx, ly, lw, lh), cols, rle = encode(pts, lambda j, pts=pts: rgb565(pts[j]))
        dye_pals = []
        if c['id'] in dyes:
            for pal in dyes[c['id']]:
                if pal is None: dye_pals.append(0xFFFF); continue
                m, ok = {}, True
                for j, p in pts.items():
                    k = rgb565(p); v = rgb565(pal[j])
                    if m.setdefault(k, v) != v: ok = False
                dye_pals.append(add_pal([m[k] for k in cols]) if ok else 0xFFFF)
        items.append((c, lx, ly, lw, lh, add_pal(cols), len(cols), len(data_pool), len(rle), dye_pals))
        data_pool += rle

    # ---------------------------------------------------------------- emit
    o = ['// GENERATED by tools/gen_pokemmo_wardrobe.py - do not edit',
         '//',
         '// PokeMMO overworld trainer + cosmetic sprites (front view, native size),',
         '// fetched from the image service pokemmohub.com uses. NOT licensed to this',
         '// project: used by the owner\'s choice for a private build. See CREDITS.md.',
         '#pragma once', '#include <stdint.h>', '',
         '#define PM_CANVAS_W %d' % CW, '#define PM_CANVAS_H %d' % CH,
         '#define PM_SKINS %d' % SKINS, '#define PM_ITEMS %d' % len(items),
         '#define PM_DYES %d' % len(HAIR_DYES), '']
    o.append('struct PmItem { uint16_t id; uint8_t slot, free; int8_t x, y; uint8_t w, h, ncol; uint16_t pal; uint32_t data; uint16_t len; uint16_t dye; const char *name; };')
    o.append('// The bare body: a pattern with one palette per skin tone.')
    o.append('static const uint8_t PM_BODY_X = %d, PM_BODY_Y = %d, PM_BODY_W = %d, PM_BODY_H = %d, PM_BODY_NCOL = %d;' % (bx, by, bw, bh, len(bcols)))
    o.append('static const uint32_t PM_BODY_DATA = %d; static const uint16_t PM_BODY_LEN = %d;' % (body_ofs, len(brle)))
    o.append('static const uint16_t PM_SKIN_PAL[PM_SKINS] = { %s };' % ', '.join(map(str, skin_pals)))
    o.append('static const char *const PM_DYE_NAME[PM_DYES] = { %s };' % ', '.join('"%s"' % d[0] for d in HAIR_DYES))
    # dye palettes: a flat table, PM_DYES per dyeable item
    dyetab, dyeidx = [], {}
    for it in items:
        if it[9]:
            dyeidx[it[0]['id']] = len(dyetab); dyetab += it[9]
    o.append('static const uint16_t PM_DYE_PAL[%d] = {' % max(1, len(dyetab)))
    for i in range(0, len(dyetab), 16): o.append('  ' + ', '.join('0x%04X' % v for v in dyetab[i:i + 16]) + ',')
    if not dyetab: o.append('  0xFFFF,')
    o.append('};')
    o.append('static const uint16_t PM_PAL[%d] = {' % len(pal_pool))
    for i in range(0, len(pal_pool), 16): o.append('  ' + ', '.join('0x%04X' % v for v in pal_pool[i:i + 16]) + ',')
    o.append('};')
    o.append('static const uint8_t PM_DATA[%d] = {' % len(data_pool))
    for i in range(0, len(data_pool), 24): o.append('  ' + ','.join(str(v) for v in data_pool[i:i + 24]) + ',')
    o.append('};')
    o.append('// slot: PokeMMO\'s (2 hat, 3 hair, 4 eyes, 5 face, 6 back, 7 top, 8 gloves, 9 shoes, 10 legs).')
    o.append('// dye: first PM_DYE_PAL entry of its PM_DYES palettes (0xFFFF = no dyes).')
    o.append('static const PmItem PM_ITEM[PM_ITEMS] = {')
    for c, lx, ly, lw, lh, pal, nc, dofs, dlen, dp in items:
        o.append('  { %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %s, "%s" },' % (
            c['id'], c['slot'], c['free'], lx, ly, lw, lh, nc, pal, dofs, dlen,
            dyeidx.get(c['id'], 0xFFFF), ascii_name(c['name']).replace('"', "'")))
    o.append('};')
    open(OUT, 'w', encoding='ascii', newline='\n').write('\n'.join(o) + '\n')
    total = len(pal_pool) * 2 + len(data_pool) + len(dyetab) * 2 + len(items) * 24
    print('wrote %s: %d items, %d palette entries, %d data bytes (~%d KB flash)' % (
        os.path.relpath(OUT), len(items), len(pal_pool), len(data_pool), total // 1024))


if __name__ == '__main__':
    main()
