#!/usr/bin/env python3
"""Pokedex entries for the SD card: each species' category, height and
description in the firmware's six languages.

    python3 tools/gen_dexinfo.py SPECIES_DIR POKEMON_DIR   # from PokeAPI, writes dex_entries.json
    python3 tools/gen_dexinfo.py                           # just rebuild dexinfo.bin from it

SPECIES_DIR holds /pokemon-species/<n> as s<n>.json and POKEMON_DIR /pokemon/<n>
as p<n>.json (for the height). The source text is committed as
tools/dex_entries.json, so a rebuild needs no network.

Text is the newest entry the games wrote in each language, folded to ASCII
(the panel font has no accents). Portuguese has no official Pokedex text, so
it uses the English one, as the move names do.

dexinfo.bin (TDX1), read by dexinfo.cpp -- ONE parser for board and emulator:
  char[4] "TDX1", u16 count, u8 langs, u8 0
  u16 heightDm[count]                       (decimetres; weight is in weights.h)
  u32 offset[count * langs]                 (from the start of the file)
  then at each offset: genus NUL text NUL
It has no digits in its name on purpose: pack_bundle.py ships digit-less files
with every region's pack, so any one pack carries the whole Pokedex.
"""
import json, os, struct, sys, unicodedata
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from dex_data import DEX
N = len(DEX)
LANGS = ('es', 'en', 'fr', 'de', 'it', 'pt')   # Lang order in i18n.h
SRC = os.path.join(HERE, 'dex_entries.json')
OUT = os.path.join(HERE, 'sdcard', 'mons', 'dexinfo.bin')
MAX_GENUS, MAX_TEXT = 24, 240

GERMAN = {'\u00e4': 'ae', '\u00f6': 'oe', '\u00fc': 'ue', '\u00c4': 'Ae', '\u00d6': 'Oe', '\u00dc': 'Ue'}

def fold(s, lang=''):
    # German spells an umlaut out ("Fluegeln"), as the UI's own German strings do
    if lang == 'de':
        for a, b in GERMAN.items(): s = s.replace(a, b)
    s = s.replace('­', '').replace('\f', ' ').replace('\n', ' ').replace('’', "'").replace('‘', "'")
    s = s.replace('“', '"').replace('”', '"').replace('—', '-').replace('–', '-').replace('…', '...')
    s = unicodedata.normalize('NFKD', s)
    s = ''.join(c for c in s if not unicodedata.combining(c))
    for a, b in (('ß', 'ss'), ('œ', 'oe'), ('æ', 'ae'), ('Œ', 'OE')):
        s = s.replace(a, b)
    s = ''.join(c for c in s if 32 <= ord(c) < 127)
    return ' '.join(s.split())

if len(sys.argv) > 2:
    entries = []
    for i in range(1, N + 1):
        sp = json.load(open(os.path.join(sys.argv[1], 's%d.json' % i)))
        pk = json.load(open(os.path.join(sys.argv[2], 'p%d.json' % i)))
        row = {'h': int(pk['height'])}
        for lang in LANGS:
            src = 'en' if lang == 'pt' else lang
            g = [x['genus'] for x in sp['genera'] if x['language']['name'] == src]
            t = [x['flavor_text'] for x in sp['flavor_text_entries'] if x['language']['name'] == src]
            if not t and src != 'en':      # a few late species have no text in every language yet
                t = [x['flavor_text'] for x in sp['flavor_text_entries'] if x['language']['name'] == 'en']
            if not g and src != 'en':
                g = [x['genus'] for x in sp['genera'] if x['language']['name'] == 'en']
            row[lang] = [fold(g[0], src) if g else '', fold(t[-1], src) if t else '']
        entries.append(row)
    missing = [i + 1 for i, r in enumerate(entries) if not r['en'][1]]
    if len(missing) > 5: sys.exit('%d species with no English text: a partial fetch is not data' % len(missing))
    json.dump(entries, open(SRC, 'w'), ensure_ascii=True, separators=(',', ':'))
    print('wrote dex_entries.json (%d species, %d without text: %s)' % (len(entries), len(missing), missing))

entries = json.load(open(SRC))
assert len(entries) == N, 'dex_entries.json is out of step with DEX_COUNT -- re-fetch'
langs = len(LANGS)
head = b'TDX1' + struct.pack('<HBB', N, langs, 0)
heights = struct.pack('<%dH' % N, *[min(r['h'], 65535) for r in entries])
base = len(head) + len(heights) + 4 * N * langs
blob, offs = bytearray(), []
for r in entries:
    for lang in LANGS:
        g, t = r[lang]
        offs.append(base + len(blob))
        blob += g[:MAX_GENUS].encode('ascii') + b'\0' + t[:MAX_TEXT].encode('ascii') + b'\0'
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, 'wb').write(head + heights + struct.pack('<%dI' % len(offs), *offs) + bytes(blob))
print('dexinfo.bin: %d species x %d languages, %d KB' % (N, langs, os.path.getsize(OUT) // 1024))
