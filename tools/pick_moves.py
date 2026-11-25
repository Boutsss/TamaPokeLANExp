#!/usr/bin/env python3
"""Choose which Gen 1-7 moves this game can play FAITHFULLY, from PokeAPI.

    python3 tools/pick_moves.py            # report what qualifies
    python3 tools/pick_moves.py --emit     # write tools/dex_moves_more.py

Reads the cached /move/<id> payloads (tools/pokeapi_cache/moves/, fetched with
curl; see the README of tools/). A move is taken only if the battle engine can
do what the games do with it:

  plain damage, a high critical rate (played as plain damage), priority,
  2-5 hits, recoil, drain; a CHANCE of an ailment (paralysis, burn, poison,
  sleep, freeze, confusion) or of one stage change on the foe or the user;
  a status move that changes stats (not accuracy/evasion), inflicts one of
  those ailments, or heals the user.

Anything else -- weather, hazards, switching, protection, fixed or variable
damage, two-turn moves, OHKOs, abilities, items -- is left out rather than
played wrongly. Move storage is one byte, so the table stops at 255: when more
qualify than fit, the ones the most species can learn go first, with every
type kept represented.
"""
import json, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from dex_moves import MOVES_BASE as MOVES   # the hand-picked list: dex_moves_more.py is what this writes
from dex_data import DEX

CACHE = os.path.join(HERE, 'pokeapi_cache', 'moves')
DEX_COUNT = len(DEX)
ROOM = 255 - (len(MOVES) + 1)   # MOVE_COUNT stays <= 255: several loops and IDs are uint8_t
AIL = {'paralysis': 'AIL_PARA', 'burn': 'AIL_BURN', 'poison': 'AIL_POISON',
       'sleep': 'AIL_SLEEP', 'freeze': 'AIL_FREEZE', 'confusion': 'AIL_CONFUSE'}
STAT = {'attack': 'ST_ATK', 'defense': 'ST_DEF', 'special-attack': 'ST_SPA',
        'special-defense': 'ST_SPD', 'speed': 'ST_SPE'}
PLAIN = re.compile(r'^(Inflicts regular damage( with no additional effect)?\.|'
                   r'Has an increased chance for a critical hit\.|'
                   r'Inflicts regular damage\.\s+User attacks first\.|'
                   r'Inflicts regular damage\.\s+Has an increased chance for a critical hit\.)$')

def load(i):
    with open(os.path.join(CACHE, '%d.json' % i)) as f:
        return json.load(f)

def short(m):
    for e in m.get('effect_entries', []):
        if e['language']['name'] == 'en':
            return ' '.join(e['short_effect'].split())
    return ''

def classify(m):
    """(fields dict, None) or (None, why not)."""
    gen = int(m['generation']['url'].rstrip('/').split('/')[-1])
    if gen > 7: return None, 'gen8+'
    meta = m.get('meta')
    if not meta: return None, 'no meta'
    cat = meta['category']['name'].replace('-', '+')
    # a condition in the effect text means a rule the engine does not have
    se0 = short(m)
    if re.search(r"\bif\b|\bonly\b|\bOnly\b|\bunless\b|next turn|ally|allies|weather|switch|[Ss]uper-effective|Stockpile", se0):
        return None, 'conditional: ' + se0[:50]
    dc = m['damage_class']['name']
    power, acc, prio = m['power'], m['accuracy'], m['priority']
    tgt = m['target']['name']
    f = dict(power=power or 0, acc=acc or 0, eff='EF_NONE', param=0, mask='0', stages=0,
             target='TG_FOE', ail='AIL_NONE', ach=0,
             cat={'physical': 'MC_PHYS', 'special': 'MC_SPEC', 'status': 'MC_STATUS'}[dc])
    hits = (meta.get('min_hits'), meta.get('max_hits'))
    drain = meta.get('drain') or 0
    se = short(m)
    def add_effect(eff, param):
        if f['eff'] != 'EF_NONE': return False
        f['eff'], f['param'] = eff, param
        return True
    if dc != 'status':
        if not power: return None, 'variable/fixed power'
        if tgt not in ('selected-pokemon', 'all-opponents', 'random-opponent', 'all-other-pokemon'):
            return None, 'target %s' % tgt
        if cat in ('damage', 'damage+ailment', 'damage+lower', 'damage+raise', 'damage+heal'):
            if prio < 0: return None, 'negative priority (special rules)'
            if cat == 'damage' and not (PLAIN.match(se) or hits == (2, 5) or drain < 0):
                return None, 'special rule: ' + se[:60]
            if hits not in ((None, None), (2, 5)): return None, 'fixed hits'
            if hits == (2, 5): add_effect('EF_MULTI', 0)
            if prio and not add_effect('EF_PRIORITY', prio): return None, 'two effects'
            if drain < 0 and not add_effect('EF_RECOIL', max(1, round(100 / -drain))): return None, 'two effects'
            if drain > 0 and not add_effect('EF_DRAIN', drain): return None, 'two effects'
            if cat == 'damage+ailment':
                a = meta['ailment']['name']
                if a not in AIL or not meta['ailment_chance']: return None, 'ailment %s' % a
                f['ail'], f['ach'] = AIL[a], meta['ailment_chance']
            if cat in ('damage+lower', 'damage+raise'):
                if not re.match(r"^(Has a chance to (raise|lower)|(Raises|Lowers)|Inflicts regular damage\.\s+(Raises|Lowers))", se):
                    return None, 'stage move with more: ' + se[:40]
                sc = m['stat_changes']
                if not sc or any(s['stat']['name'] not in STAT for s in sc): return None, 'acc/evasion stage'
                if len({s['change'] for s in sc}) != 1: return None, 'mixed stages'
                if not meta['stat_chance']: return None, 'no stage chance'
                if not add_effect('EF_STAGE', meta['stat_chance']): return None, 'two effects'
                f['mask'] = '|'.join(STAT[s['stat']['name']] for s in sc)
                f['stages'] = sc[0]['change']
                f['target'] = 'TG_SELF' if cat == 'damage+raise' else 'TG_FOE'
            return f, None
        return None, 'category %s' % cat
    # status moves
    if cat == 'net+good+stats':
        if not re.match(r"^(Raises|Lowers) [^.]*\.$", se):
            return None, 'stat move with more: ' + se[:50]
        sc = m['stat_changes']
        if not sc or any(s['stat']['name'] not in STAT for s in sc): return None, 'acc/evasion stage'
        if len({s['change'] for s in sc}) != 1: return None, 'mixed stages'
        f['eff'], f['mask'], f['stages'] = 'EF_STAGE', '|'.join(STAT[s['stat']['name']] for s in sc), sc[0]['change']
        if tgt == 'user': f['target'] = 'TG_SELF'
        elif tgt in ('selected-pokemon', 'all-opponents'): f['target'] = 'TG_FOE'
        else: return None, 'target %s' % tgt
        f['acc'] = 0 if f['target'] == 'TG_SELF' else (acc or 0)
        return f, None
    if cat == 'ailment':
        a = meta['ailment']['name']
        if a not in AIL or tgt not in ('selected-pokemon', 'all-opponents'): return None, 'ailment %s' % a
        f['ail'], f['ach'] = AIL[a], 100
        return f, None
    if cat == 'heal' and tgt == 'user' and meta.get('healing') and re.match(r"^Heals the user by half its max HP\.$", se):
        f['eff'], f['param'], f['target'], f['acc'] = 'EF_HEAL', meta['healing'], 'TG_SELF', 0
        return f, None
    return None, 'category %s' % cat

def learners(m):
    return sorted({int(p['url'].rstrip('/').split('/')[-1]) for p in m['learned_by_pokemon']} & set(range(1, DEX_COUNT + 1)))

def en_name(m):
    n = [x['name'] for x in m['names'] if x['language']['name'] == 'en'][0].upper()
    if len(n) > 12: n = n.replace(' ', '')
    return n if len(n) <= 12 else None

def main():
    have = {slug for _n, slug, *_ in MOVES if slug}
    ok, why = [], {}
    for i in range(1, 743):
        p = os.path.join(CACHE, '%d.json' % i)
        if not os.path.exists(p): continue
        m = load(i)
        if m['name'] in have: continue
        f, no = classify(m)
        if no: why[no.split(':')[0]] = why.get(no.split(':')[0], 0) + 1; continue
        L = learners(m)
        if len(L) < 3: why['few learners'] = why.get('few learners', 0) + 1; continue
        nm = en_name(m)
        if not nm: why['name too long'] = why.get('name too long', 0) + 1; continue
        ok.append((m, f, L, nm))
    print('qualify: %d (room for %d)' % (len(ok), ROOM))
    for k, v in sorted(why.items(), key=lambda kv: -kv[1]): print('  left out, %-22s %d' % (k, v))
    # pick: every type keeps its best few, then the most learnable
    ok.sort(key=lambda t: -len(t[2]))
    picked, per_type = [], {}
    for t in ok:
        ty = t[0]['type']['name']
        if per_type.get(ty, 0) < 3:
            picked.append(t); per_type[ty] = per_type.get(ty, 0) + 1
    for t in ok:
        if len(picked) >= ROOM: break
        if t not in picked: picked.append(t)
    picked = picked[:ROOM]
    print('picked %d' % len(picked))
    if '--emit' in sys.argv:
        # A move's POSITION is its saved ID, so once written this file is
        # frozen: re-picking could reorder it and rewrite every saved moveset.
        # New moves are appended to it by hand, never by a re-run.
        if os.path.exists(os.path.join(HERE, 'dex_moves_more.py')) and '--force' not in sys.argv:
            sys.exit('dex_moves_more.py exists and is APPEND-ONLY (its order is in saves); not overwriting')
        emit(picked)
    else:
        for m, f, L, nm in picked[:400]:
            print('  %-13s %-8s %-9s %3s %3s %-11s %3s %s/%s %s %4d | %s' % (nm, m['type']['name'], f['cat'], f['power'], f['acc'],
                  f['eff'], f['param'], f['ail'][4:], f['ach'], f['target'][3:], len(L), short(m)[:70]))

def fold(s):
    import unicodedata
    s = unicodedata.normalize('NFKD', s)
    s = ''.join(c for c in s if not unicodedata.combining(c)).upper().replace('’', "'")
    for a, b in (('ß', 'SS'), ('Œ', 'OE'), ('Æ', 'AE'), ('ẞ', 'SS')):
        s = s.replace(a, b)
    return s

def emit(picked):
    with open(os.path.join(HERE, 'dex_moves_more.py'), 'w') as o:
        o.write('# GENERATED by tools/pick_moves.py from PokeAPI - do not edit by hand\n')
        o.write('# Gen 1-7 moves the engine plays faithfully (see pick_moves.py), APPENDED to\n')
        o.write('# MOVES after the six status moves. Same tuple shape as dex_moves.py.\n')
        o.write('from dex_moves import *\n\nMORE_MOVES = [\n')
        for m, f, L, nm in picked:
            o.write('    (%r, %r, %r, %s, %d, %d, %s, %d, %s, %d, %s, %s, %d),\n' % (
                nm, m['name'], m['type']['name'], f['cat'], f['power'], f['acc'], f['eff'], f['param'],
                f['mask'], f['stages'], f['target'], f['ail'], f['ach']))
        o.write(']\n\nMORE_NAMES = {\n')
        for m, f, L, nm in picked:
            n = {x['language']['name']: x['name'] for x in m['names']}
            row = [fold(n.get('es', nm))[:23], nm, fold(n.get('fr', nm))[:23], fold(n.get('de', nm))[:23],
                   fold(n.get('it', nm))[:23], nm]   # PT: no official locale; English
            o.write('    %r: %r,\n' % (m['name'], tuple(row)))
        o.write('}\n')
    print('wrote dex_moves_more.py')

if __name__ == '__main__':
    main()
