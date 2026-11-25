#!/usr/bin/env python3
"""The SECOND batch of Gen 1-7 moves (v3.37): the ones that need battle rules
the engine did not have until now -- Protect, weather, hazards, screens,
Substitute, flinching, accuracy/evasion, variable and fixed damage, two-turn
moves, and the rest listed in SPECIAL below.

    python3 tools/pick_moves2.py            # report
    python3 tools/pick_moves2.py --emit     # write dex_moves_more2.py and dex_moves_meta.py

Reads the cached /move/<id> payloads (tools/pokeapi_cache/moves/). Two files:

  dex_moves_more2.py   the new moves, APPENDED after dex_moves_more.py. Once
                       written it is frozen like the others: a move's position
                       is its saved ID. This tool refuses to overwrite it.
  dex_moves_meta.py    priority, critical-hit stage and flinch chance for EVERY
                       move in the table, old ones included, plus the few
                       per-move flags (hits a flying target, ignores stages...).
                       This one CAN be regenerated: it changes how a move
                       plays, never which move an ID is.

A move is taken only if it can be played as the games play it. Left out on
purpose: anything that needs abilities, held-item tricks or a second creature
on the field (doubles), PP, the moves that force or pass a switch (a later
batch -- they need the switch screen), and the Z-moves.
"""
import json, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import dex_moves as D
from dex_data import DEX

CACHE = os.path.join(HERE, 'pokeapi_cache', 'moves')
DEX_COUNT = len(DEX)
AIL = {'paralysis': 'AIL_PARA', 'burn': 'AIL_BURN', 'poison': 'AIL_POISON',
       'sleep': 'AIL_SLEEP', 'freeze': 'AIL_FREEZE', 'confusion': 'AIL_CONFUSE'}
STAT = {'attack': 'ST_ATK', 'defense': 'ST_DEF', 'special-attack': 'ST_SPA',
        'special-defense': 'ST_SPD', 'speed': 'ST_SPE', 'accuracy': 'ST_ACC', 'evasion': 'ST_EVA'}
CAT = {'physical': 'MC_PHYS', 'special': 'MC_SPEC', 'status': 'MC_STATUS'}

# slug -> the rule. Keys: eff, param, mask, stages, target, ail, ach (anything
# omitted comes from PokeAPI or the plain default). Each EF_ is implemented in
# battle.cpp and named in dex_moves.py.
S = {}
def sp(slugs, **kw):
    for s in slugs.split():
        S[s] = kw
sp('protect detect', eff='EF_PROTECT', target='TG_SELF')
sp('endure', eff='EF_ENDURE', target='TG_SELF')
sp('substitute', eff='EF_SUBSTITUTE', target='TG_SELF')
sp('rest', eff='EF_REST', target='TG_SELF')
sp('sleep-talk', eff='EF_SLEEPTALK', target='TG_SELF')
sp('rain-dance', eff='EF_WEATHER', param=1, target='TG_SELF')
sp('sunny-day', eff='EF_WEATHER', param=2, target='TG_SELF')
sp('sandstorm', eff='EF_WEATHER', param=3, target='TG_SELF')
sp('hail', eff='EF_WEATHER', param=4, target='TG_SELF')
sp('stealth-rock', eff='EF_HAZARD', param=1)
sp('spikes', eff='EF_HAZARD', param=2)
sp('toxic-spikes', eff='EF_HAZARD', param=3)
sp('sticky-web', eff='EF_HAZARD', param=4)
sp('defog', eff='EF_DEFOG', mask='ST_EVA', stages=-1)
sp('rapid-spin', eff='EF_SPIN')
sp('reflect', eff='EF_SCREEN', param=1, target='TG_SELF')
sp('light-screen', eff='EF_SCREEN', param=2, target='TG_SELF')
sp('aurora-veil', eff='EF_SCREEN', param=3, target='TG_SELF')
sp('tailwind', eff='EF_TAILWIND', target='TG_SELF')
sp('trick-room', eff='EF_TRICKROOM', target='TG_SELF')
sp('haze', eff='EF_HAZE', target='TG_SELF')
sp('clear-smog', eff='EF_CLEARSMOG')
sp('psych-up', eff='EF_PSYCHUP', target='TG_SELF')
sp('pain-split', eff='EF_PAINSPLIT')
sp('belly-drum', eff='EF_BELLYDRUM', target='TG_SELF')
sp('shell-smash', eff='EF_SHELLSMASH', target='TG_SELF')
sp('curse', eff='EF_CURSE', target='TG_SELF')
sp('leech-seed', eff='EF_LEECHSEED')
sp('yawn', eff='EF_YAWN')
sp('taunt', eff='EF_TAUNT')
sp('encore', eff='EF_ENCORE')
sp('swagger', eff='EF_SWAGGER', mask='ST_ATK', stages=2)
sp('flatter', eff='EF_SWAGGER', mask='ST_SPA', stages=1)
sp('destiny-bond', eff='EF_DESTINYBOND', target='TG_SELF')
sp('perish-song', eff='EF_PERISH', target='TG_SELF')
sp('aqua-ring', eff='EF_AQUARING', param=0, target='TG_SELF')
sp('ingrain', eff='EF_AQUARING', param=1, target='TG_SELF')
sp('wish', eff='EF_WISH', target='TG_SELF')
sp('safeguard', eff='EF_SAFEGUARD', target='TG_SELF')
sp('mist', eff='EF_MIST', target='TG_SELF')
sp('focus-energy', eff='EF_FOCUSENERGY', param=2, target='TG_SELF')
sp('laser-focus', eff='EF_FOCUSENERGY', param=3, target='TG_SELF')
sp('splash', eff='EF_SPLASH', target='TG_SELF')
sp('acupressure', eff='EF_ACUPRESSURE', target='TG_SELF')
sp('topsy-turvy', eff='EF_TOPSYTURVY')
sp('charge', eff='EF_CHARGEUP', target='TG_SELF')
sp('synthesis morning-sun moonlight', eff='EF_HEAL_WEATHER', param=0, target='TG_SELF')
sp('shore-up', eff='EF_HEAL_WEATHER', param=1, target='TG_SELF')
sp('mean-look block spider-web', eff='EF_TRAP')
sp('electric-terrain', eff='EF_TERRAIN', param=1, target='TG_SELF')
sp('grassy-terrain', eff='EF_TERRAIN', param=2, target='TG_SELF')
sp('misty-terrain', eff='EF_TERRAIN', param=3, target='TG_SELF')
sp('psychic-terrain', eff='EF_TERRAIN', param=4, target='TG_SELF')
sp('memento', eff='EF_MEMENTO', mask='ST_ATK|ST_SPA', stages=-2)
# damaging
sp('giga-impact blast-burn frenzy-plant hydro-cannon rock-wrecker roar-of-time prismatic-laser', eff='EF_RECHARGE')
sp('facade', eff='EF_VARPOW', param=1)
sp('return', eff='EF_VARPOW', param=2)
sp('frustration', eff='EF_VARPOW', param=3)
sp('payback', eff='EF_VARPOW', param=4)
sp('assurance', eff='EF_VARPOW', param=5)
sp('avalanche revenge', eff='EF_VARPOW', param=6)
sp('hex', eff='EF_VARPOW', param=7)
sp('venoshock', eff='EF_VARPOW', param=8)
sp('brine', eff='EF_VARPOW', param=9)
sp('acrobatics', eff='EF_VARPOW', param=10)
sp('gyro-ball', eff='EF_VARPOW', param=11)
sp('electro-ball', eff='EF_VARPOW', param=12)
sp('reversal flail', eff='EF_VARPOW', param=13)
sp('eruption water-spout', eff='EF_VARPOW', param=14)
sp('stored-power power-trip', eff='EF_VARPOW', param=15)
sp('punishment', eff='EF_VARPOW', param=16)
sp('weather-ball', eff='EF_VARPOW', param=17)
sp('wring-out crush-grip', eff='EF_VARPOW', param=18)
sp('stomping-tantrum', eff='EF_VARPOW', param=19)
sp('low-kick grass-knot', eff='EF_VARPOW', param=20)
sp('heavy-slam heat-crash', eff='EF_VARPOW', param=21)
sp('hidden-power', eff='EF_HIDDENPOWER')
sp('magnitude', eff='EF_MAGNITUDE')
sp('knock-off', eff='EF_KNOCKOFF')
sp('endeavor', eff='EF_ENDEAVOR')
sp('super-fang natures-madness', eff='EF_HALFHP')
sp('final-gambit', eff='EF_FINALGAMBIT')
sp('fissure guillotine horn-drill sheer-cold', eff='EF_OHKO')
sp('sonic-boom', eff='EF_FIXED', param=20)
sp('counter', eff='EF_COUNTER', param=1)
sp('mirror-coat', eff='EF_COUNTER', param=2)
sp('metal-burst', eff='EF_COUNTER', param=3)
sp('focus-punch', eff='EF_FOCUSPUNCH')
sp('sucker-punch', eff='EF_SUCKER')
sp('fake-out', eff='EF_FAKEOUT', param=0)
sp('first-impression', eff='EF_FAKEOUT', param=1)
sp('explosion self-destruct', eff='EF_SELFKO')
# switching (v3.40): the battle screen performs the switch the engine asks for
sp('roar whirlwind dragon-tail circle-throw', eff='EF_FORCESWITCH')
sp('u-turn volt-switch', eff='EF_UTURN')
sp('parting-shot', eff='EF_UTURN', mask='ST_ATK|ST_SPA', stages=-1)
sp('baton-pass', eff='EF_BATONPASS', target='TG_SELF')
# plain damage whose one special rule is a flag (FLAGS below) or the crit column
sp('brick-break psychic-fangs sacred-sword chip-away darkest-lariat psyshock psystrike secret-sword '
   'foul-play freeze-dry storm-throw frost-breath smart-strike', eff='EF_NONE')
sp('dream-eater', eff='EF_DREAMEATER', param=50)
sp('tri-attack', eff='EF_TRIATTACK', ach=20)
sp('smelling-salts', eff='EF_WAKEUP', param=1)
sp('wake-up-slap', eff='EF_WAKEUP', param=4)
sp('fury-cutter', eff='EF_ESCALATE', param=3)
sp('rollout ice-ball', eff='EF_ESCALATE', param=4)
sp('echoed-voice', eff='EF_ESCALATE', param=4)
sp('thrash petal-dance', eff='EF_LOCKIN')
sp('wrap bind fire-spin whirlpool sand-tomb clamp infestation magma-storm', eff='EF_BIND')
sp('snore', eff='EF_SNORE')
sp('future-sight doom-desire', eff='EF_FUTURE')
# two-turn: param bit 1 = out of reach while charging, 2 = no charge in sun,
# 4 = Defense +1 on the charging turn
sp('bounce dive phantom-force shadow-force', eff='EF_CHARGE', param=1)
sp('sky-attack razor-wind freeze-shock ice-burn', eff='EF_CHARGE', param=0)
sp('solar-blade', eff='EF_CHARGE', param=2)
sp('skull-bash', eff='EF_CHARGE', param=4)

# Flags (MF_ in dex_moves.py), for every move in the table that has one.
FLAGS = {}
def fl(slugs, f):
    for s in slugs.split():
        FLAGS[s] = FLAGS.get(s, []) + [f]
fl('sacred-sword chip-away darkest-lariat', 'MF_IGNORE_STAGES')
fl('psyshock psystrike secret-sword', 'MF_PHYS_DEF')
fl('foul-play', 'MF_FOUL')
fl('brick-break psychic-fangs', 'MF_BREAK_SCREEN')
fl('freeze-dry', 'MF_FREEZE_DRY')
fl('gust twister thunder hurricane sky-uppercut smack-down', 'MF_HITS_AIR')
fl('earthquake magnitude fissure', 'MF_HITS_GROUND')
fl('surf whirlpool', 'MF_HITS_WATER')
# the rain-and-hail accuracy rules
fl('thunder hurricane', 'MF_RAIN_SURE')
fl('blizzard', 'MF_HAIL_SURE')

# Names longer than the 12 the battle buttons fit, shortened by hand.
SHORT = {'spectral-thief': 'SPECTRALTHF', 'parabolic-charge': 'PARABOLICCHG',
         'precipice-blades': 'PRECIPICEBLD', 'electric-terrain': 'ELEC TERRAIN',
         'grassy-terrain': 'GRASSTERRAIN', 'psychic-terrain': 'PSY TERRAIN',
         'misty-terrain': 'MISTYTERRAIN', 'double-iron-bash': 'DBL IRONBASH',
         'natures-madness': 'NATUREMADNES', 'prismatic-laser': 'PRISMATICLSR',
         'stomping-tantrum': 'STOMPTANTRUM', 'first-impression': 'FIRSTIMPRESS',
         'self-destruct': 'SELFDESTRUCT', 'smelling-salts': 'SMELLINGSALT', 'petal-blizzard': 'PETALBLIZZRD',
         'disarming-voice': 'DISARM VOICE', 'baby-doll-eyes': 'BABYDOLLEYES', 'high-horsepower': 'HIGHHORSEPWR',
         'steam-eruption': 'STEAMERUPTN', 'darkest-lariat': 'DARK LARIAT', 'power-up-punch': 'POWERUPPUNCH', 'clanging-scales': 'CLANGSCALES'}

def load(i):
    with open(os.path.join(CACHE, '%d.json' % i)) as f:
        return json.load(f)

def short(m):
    for e in m.get('effect_entries', []):
        if e['language']['name'] == 'en':
            return ' '.join(e['short_effect'].split())
    return ''

def learners(m):
    return sorted({int(p['url'].rstrip('/').split('/')[-1]) for p in m['learned_by_pokemon']} & set(range(1, DEX_COUNT + 1)))

def en_name(m):
    if m['name'] in SHORT: return SHORT[m['name']]
    n = [x['name'] for x in m['names'] if x['language']['name'] == 'en'][0].upper().replace('’', "'")
    if len(n) > 12: n = n.replace(' ', '')
    return n if len(n) <= 12 else None

def stage_fields(m, f):
    sc = m['stat_changes']
    if not sc or any(s['stat']['name'] not in STAT for s in sc): return 'stage on a stat we do not track'
    if len({s['change'] for s in sc}) != 1: return 'mixed stages'
    f['mask'] = '|'.join(STAT[s['stat']['name']] for s in sc)
    f['stages'] = sc[0]['change']
    return None

GENERIC_DAMAGE = re.compile(
    r"^(Inflicts regular damage( with no additional effect)?\.|Has an increased chance for a critical hit\.|"
    r"Inflicts regular damage\.\s+User attacks first\.|Never misses\.|Has a chance to make the target flinch\.|"
    r"Hits twice in one turn\.|Hits 2-5 times in one turn\.|Inflicts regular damage\.\s+Has an increased chance for a critical hit\.)$")

def classify(m):
    gen = int(m['generation']['url'].rstrip('/').split('/')[-1])
    if gen > 7: return None, 'gen8+'
    meta = m.get('meta')
    if not meta: return None, 'no meta (Z-move)'
    dc = m['damage_class']['name']
    power, acc, prio = m['power'] or 0, m['accuracy'] or 0, m['priority']
    tgt = m['target']['name']
    f = dict(power=power, acc=acc, eff='EF_NONE', param=0, mask='0', stages=0,
             target='TG_FOE', ail='AIL_NONE', ach=0, cat=CAT[dc])
    if m['name'] in S:
        f.update(S[m['name']])
        if f['target'] == 'TG_SELF' and dc == 'status': f['acc'] = 0
        if m['name'] in ('hidden-power', 'magnitude', 'return', 'frustration') and not f['power']: f['power'] = 1
        if f['eff'] == 'EF_VARPOW' and not f['power']: f['power'] = 1   # decided at use time
        ail = meta['ailment']['name']
        if dc != 'status' and ail in AIL and meta['ailment_chance'] and f['ail'] == 'AIL_NONE':
            f['ail'], f['ach'] = AIL[ail], meta['ailment_chance']
        return f, None
    cat = meta['category']['name']
    se = short(m)
    # a condition in the effect text is a rule the generic path cannot play
    if re.search(r"\bif\b|\bonly\b|\bOnly\b|\bunless\b|next turn|ally|allies|gender|switch|[Ss]uper-effective", se):
        return None, 'conditional'
    hits = (meta.get('min_hits'), meta.get('max_hits'))
    drain = meta.get('drain') or 0
    def add_effect(eff, param):
        if f['eff'] != 'EF_NONE': return False
        f['eff'], f['param'] = eff, param
        return True
    if dc != 'status':
        if not power: return None, 'variable/fixed power'
        if tgt not in ('selected-pokemon', 'all-opponents', 'random-opponent', 'all-other-pokemon'):
            return None, 'target %s' % tgt
        if cat not in ('damage', 'damage-ailment', 'damage-lower', 'damage-raise', 'damage-heal'):
            return None, 'category %s' % cat
        if cat == 'damage' and not (GENERIC_DAMAGE.match(se) or drain < 0):
            return None, 'special rule: ' + se[:60]
        if hits == (2, 5): add_effect('EF_MULTI', 0)
        elif hits in ((2, 2), (3, 3)): add_effect('EF_MULTI', hits[0])
        elif hits != (None, None): return None, 'hits %r' % (hits,)
        if prio > 0 and not add_effect('EF_PRIORITY', prio): pass   # prio is its own column now
        if not acc and not add_effect('EF_NEVER_MISS', 0): return None, 'two effects'
        if drain < 0 and not add_effect('EF_RECOIL', max(1, round(100 / -drain))): return None, 'two effects'
        if drain > 0 and not add_effect('EF_DRAIN', drain): return None, 'two effects'
        if cat == 'damage-ailment':
            a = meta['ailment']['name']
            if a not in AIL or not meta['ailment_chance']: return None, 'ailment %s' % a
            f['ail'], f['ach'] = AIL[a], meta['ailment_chance']
        if cat in ('damage-lower', 'damage-raise'):
            if not re.match(r"^(Has a chance to (raise|lower)|(Raises|Lowers)|Inflicts regular damage\.\s+(Raises|Lowers))", se):
                return None, 'stage move with more: ' + se[:40]
            why = stage_fields(m, f)
            if why: return None, why
            if not meta['stat_chance']: return None, 'no stage chance'
            if not add_effect('EF_STAGE', meta['stat_chance']): return None, 'two effects'
            f['target'] = 'TG_SELF' if cat == 'damage-raise' else 'TG_FOE'
        if cat == 'damage-heal' and not drain: return None, 'heal without drain'
        return f, None
    if cat == 'net-good-stats':
        if not re.match(r"^(Raises|Lowers) [^.]*\.$", se): return None, 'stat move with more: ' + se[:50]
        why = stage_fields(m, f)
        if why: return None, why
        f['eff'] = 'EF_STAGE'
        if tgt == 'user': f['target'] = 'TG_SELF'
        elif tgt in ('selected-pokemon', 'all-opponents'): f['target'] = 'TG_FOE'
        else: return None, 'target %s' % tgt
        f['acc'] = 0 if f['target'] == 'TG_SELF' else acc
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

def all_moves():
    for i in range(1, 2000):
        p = os.path.join(CACHE, '%d.json' % i)
        if os.path.exists(p): yield load(i)

def fold(s):
    import unicodedata
    s = unicodedata.normalize('NFKD', s)
    s = ''.join(c for c in s if not unicodedata.combining(c)).upper().replace('’', "'")
    for a, b in (('ß', 'SS'), ('Œ', 'OE'), ('Æ', 'AE'), ('ẞ', 'SS')):
        s = s.replace(a, b)
    return ''.join(c for c in s if 32 <= ord(c) < 127)

def main():
    # Only the batches BEFORE this one count as already taken. Reading D.MOVES
    # here would include this file's own previous output, and a --force re-run
    # would then rewrite it with only whatever was new (it did, once).
    have = {slug for _n, slug, *_ in D.MOVES_BASE + D.MORE_MOVES if slug}
    ok, why = [], {}
    by_slug = {}
    for m in all_moves():
        by_slug[m['name']] = m
        if m['name'] in have: continue
        f, no = classify(m)
        if no: why[no.split(':')[0]] = why.get(no.split(':')[0], 0) + 1; continue
        L = learners(m)
        if not L: why['nobody learns it'] = why.get('nobody learns it', 0) + 1; continue
        nm = en_name(m)
        if not nm: why['name too long'] = why.get('name too long', 0) + 1; print('  NAME', m['name']); continue
        ok.append((m, f, L, nm))
    # most learnable first, so the table reads in a sensible order
    ok.sort(key=lambda t: (-len(t[2]), t[0]['name']))
    print('qualify: %d' % len(ok))
    for k, v in sorted(why.items(), key=lambda kv: -kv[1]): print('  left out, %-28s %d' % (k, v))
    missing = sorted(set(S) - {t[0]['name'] for t in ok} - have)
    if missing: print('SPECIAL but not taken:', missing)
    if '--emit' not in sys.argv:
        for m, f, L, nm in ok:
            print('  %-13s %-8s %-9s %3s %3s %-15s %3s %s/%s %s %4d | %s' % (nm, m['type']['name'], f['cat'], f['power'], f['acc'],
                  f['eff'], f['param'], f['ail'][4:], f['ach'], f['target'][3:], len(L), short(m)[:60]))
        return
    out = os.path.join(HERE, 'dex_moves_more2.py')
    if '--append' in sys.argv and os.path.exists(out):
        # The file is in saves: its rows never move. New moves go on the end,
        # in this run's order, and nothing already there is touched.
        import dex_moves_more2 as M2
        old = {slug for _n, slug, *_ in M2.MORE2_MOVES}
        new = [t for t in ok if t[0]['name'] not in old]
        txt = open(out).read()
        rows = ''.join('    (%r, %r, %r, %s, %d, %d, %s, %d, %s, %d, %s, %s, %d),\n' % (
            nm, m['name'], m['type']['name'], f['cat'], f['power'], f['acc'], f['eff'], f['param'],
            f['mask'], f['stages'], f['target'], f['ail'], f['ach']) for m, f, L, nm in new)
        names = ''
        for m, f, L, nm in new:
            n = {x['language']['name']: x['name'] for x in m['names']}
            row = [fold(n.get('es', nm))[:23], nm, fold(n.get('fr', nm))[:23], fold(n.get('de', nm))[:23],
                   fold(n.get('it', nm))[:23], nm]
            names += '    %r: %r,\n' % (m['name'], tuple(row))
        a = txt.index('\n]\n\nMORE2_NAMES')
        txt = txt[:a + 1] + rows + txt[a + 1:]
        b = txt.rindex('\n}')
        txt = txt[:b + 1] + names + txt[b + 1:]
        open(out, 'w').write(txt)
        print('appended %d moves: %s' % (len(new), ', '.join(t[3] for t in new)))
    elif os.path.exists(out) and '--force' not in sys.argv:
        print('dex_moves_more2.py exists and is APPEND-ONLY (its order is in saves); not rewriting it')
    else:
        with open(out, 'w') as o:
            o.write('# GENERATED ONCE by tools/pick_moves2.py from PokeAPI -- APPEND-ONLY from now on\n')
            o.write('# The second batch of Gen 1-7 moves (v3.37), after dex_moves_more.py. Same tuple\n')
            o.write('# shape as dex_moves.py; a move\'s position here is its saved ID.\n')
            o.write('from dex_moves import *\n\nMORE2_MOVES = [\n')
            for m, f, L, nm in ok:
                o.write('    (%r, %r, %r, %s, %d, %d, %s, %d, %s, %d, %s, %s, %d),\n' % (
                    nm, m['name'], m['type']['name'], f['cat'], f['power'], f['acc'], f['eff'], f['param'],
                    f['mask'], f['stages'], f['target'], f['ail'], f['ach']))
            o.write(']\n\nMORE2_NAMES = {\n')
            for m, f, L, nm in ok:
                n = {x['language']['name']: x['name'] for x in m['names']}
                row = [fold(n.get('es', nm))[:23], nm, fold(n.get('fr', nm))[:23], fold(n.get('de', nm))[:23],
                       fold(n.get('it', nm))[:23], nm]   # PT: no official locale; English
                o.write('    %r: %r,\n' % (m['name'], tuple(row)))
            o.write('}\n')
        print('wrote dex_moves_more2.py (%d moves)' % len(ok))
    # the per-move meta, for every move in the table
    taken = {t[0]['name'] for t in ok} | have
    with open(os.path.join(HERE, 'dex_moves_meta.py'), 'w') as o:
        o.write('# GENERATED by tools/pick_moves2.py from PokeAPI -- safe to regenerate:\n')
        o.write('# how a move plays (priority, critical stage, flinch chance, flags), never\n')
        o.write('# which move an ID is.\n')
        o.write('# slug -> (priority, crit stage, flinch percent, [MF_ flag names])\n')
        o.write('META = {\n')
        for slug in sorted(taken):
            m = by_slug.get(slug)
            if not m: continue
            meta = m.get('meta') or {}
            o.write('    %r: (%d, %d, %d, %r),\n' % (slug, m['priority'], meta.get('crit_rate') or 0,
                                                     meta.get('flinch_chance') or 0, FLAGS.get(slug, [])))
        o.write('}\n')
    print('wrote dex_moves_meta.py')

if __name__ == '__main__':
    main()
