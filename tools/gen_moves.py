#!/usr/bin/env python3
"""Genera moves.h (tabla de movimientos + learnsets) desde dex_moves.py.

  python3 tools/gen_moves.py

Reads the hand-authored move list in dex_moves.py and the learnsets fetched
into dex_learnsets.py by fetch_pokeapi.py. Run fetch_pokeapi.py first if
dex_learnsets.py is missing or the move list changed.

Learnsets are stored CSR-style: one flat LEARN_TBL of (move, level) pairs
plus a LEARN_OFS index, so species n owns LEARN_OFS[n] .. LEARN_OFS[n+1].
That is a few KB of flash for the whole dex, against far more for a fixed 2D array.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from dex_moves import (MOVES, MC_PHYS, MC_SPEC, MC_STATUS, EF_STAGE,
                       ST_ATK, ST_DEF, ST_SPA, ST_SPD, ST_SPE, TG_SELF, TG_FOE,
                       AIL_NONE, AIL_PARA, AIL_BURN, AIL_POISON, AIL_SLEEP,
                       AIL_FREEZE, AIL_CONFUSE)
from dex_move_names import MOVE_NAME_LANGS, MOVE_NAMES
import dex_moves as _D
# Every EF_ and MF_ constant, by value, straight from dex_moves.py: the C enums
# below are generated from this, so the two can never disagree.
EF_NAMES = sorted(((v, k) for k, v in vars(_D).items() if k.startswith('EF_') and isinstance(v, int)))
MF_NAMES = sorted(((v, k) for k, v in vars(_D).items() if k.startswith('MF_') and isinstance(v, int)))
# Priority, critical stage, flinch chance and flags per move (pick_moves2.py).
try:
    from dex_moves_meta import META
except ImportError:
    META = {}

# The ailment pair is optional in MOVES: only the ~17 moves that inflict one
# spell it out, the rest are (AIL_NONE, 0). Keeps the table readable and means
# adding an ailment touches one line rather than all 77.
def unpack(m):
    return tuple(m) + ((AIL_NONE, 0) if len(m) == 11 else ())
from dex_learnsets import LEARNSETS
# The status moves' TM-style entries (tools/fetch_status_learnsets.py), merged
# in rather than written into dex_learnsets.py, so re-running fetch_pokeapi.py
# cannot drop them -- nor this the other's.
try:
    from dex_learnsets_tm import LEARNSETS_TM
except ImportError:
    LEARNSETS_TM = {}
# Appended AFTER each species' own rows, in their original order, so nothing a
# species already learned moves in its table (the picker's order and every tie
# in relearnFromLevel() stay exactly as they were).
# ...and the Gen 1-7 moves' rows (tools/fetch_more_learnsets.py), after those.
try:
    from dex_learnsets_more import LEARNSETS_MORE
except ImportError:
    LEARNSETS_MORE = {}
try:   # the second batch (tools/fetch_more_learnsets.py --batch 2), after those
    from dex_learnsets_more2 import LEARNSETS_MORE2
    for _n, _rows in LEARNSETS_MORE2.items():
        LEARNSETS_MORE[_n] = list(LEARNSETS_MORE.get(_n, [])) + [r for r in _rows if r[0] not in {x[0] for x in LEARNSETS_MORE.get(_n, [])}]
except ImportError:
    pass
def _append(base, extra):
    have = {x[0] for x in base}
    return list(base) + [r for r in extra if r[0] not in have]
LEARNSETS = {n: _append(_append(LEARNSETS.get(n, []), LEARNSETS_TM.get(n, [])), LEARNSETS_MORE.get(n, []))
             for n in set(LEARNSETS) | set(LEARNSETS_TM) | set(LEARNSETS_MORE)}

# Derived, not hardcoded: this file and dex.h must agree on how many species
# there are, or LEARN_OFS is short and every lookup past the end reads garbage.
from dex_data import DEX
DEX_COUNT = len(DEX)
MAX_NAME = 12  # four move buttons across a 466 px round panel at text size 2
MAX_LOCAL_NAME = 23  # 160 px battle cell minus 10 px padding on each side


def ident(name):
    """DOUBLE-EDGE -> MV_DOUBLE_EDGE"""
    out = ''.join(c if c.isalnum() else '_' for c in name)
    return 'MV_' + out.upper()


def main():
    # --- validation: a bad row here is a silent wrong-damage bug on hardware
    seen = set()
    for name, slug, typ, cat, power, acc, eff, param, mask, stages, tgt, _ail, _ach in map(unpack, MOVES):
        if len(name) > MAX_NAME:
            sys.exit('nombre demasiado largo (%d > %d): %s' % (len(name), MAX_NAME, name))
        if ident(name) in seen:
            sys.exit('nombre duplicado: %s' % name)
        seen.add(ident(name))
        if cat == MC_STATUS and power:
            sys.exit('%s: un movimiento de estado no puede tener potencia' % name)
        if cat != MC_STATUS and not power and eff not in (_D.EF_FIXED_LVL, _D.EF_FIXED, _D.EF_HALFHP,
                                                           _D.EF_ENDEAVOR, _D.EF_OHKO, _D.EF_COUNTER,
                                                           _D.EF_FINALGAMBIT):
            sys.exit('%s: potencia 0 sin dano fijo' % name)
        # a stage mask belongs to EF_STAGE, and only to the few rules that carry one
        if mask and eff not in (EF_STAGE, _D.EF_SWAGGER, _D.EF_DEFOG, _D.EF_MEMENTO, _D.EF_UTURN):
            sys.exit('%s: EF_STAGE y statMask no concuerdan' % name)
        if mask and not stages:
            sys.exit('%s: statMask sin stages' % name)
        if not 0 <= acc <= 100:
            sys.exit('%s: precision fuera de rango' % name)

    move_keys = [slug or name.lower() for name, slug, *_ in MOVES]
    if set(move_keys) != set(MOVE_NAMES):
        missing = sorted(set(move_keys) - set(MOVE_NAMES))
        extra = sorted(set(MOVE_NAMES) - set(move_keys))
        sys.exit('move-name keys differ: missing=%s extra=%s' % (missing, extra))
    if len(MOVE_NAME_LANGS) != 6:
        sys.exit('move names must have the six firmware languages')
    for key in move_keys:
        row = MOVE_NAMES[key]
        if len(row) != len(MOVE_NAME_LANGS):
            sys.exit('%s: expected %d localized names' % (key, len(MOVE_NAME_LANGS)))
        for lang, value in zip(MOVE_NAME_LANGS, row):
            if not value or not value.isascii():
                sys.exit('%s/%s: localized name must be non-empty ASCII' % (key, lang))
            if len(value) > MAX_LOCAL_NAME:
                sys.exit('%s/%s: localized name too long (%d > %d): %s'
                         % (key, lang, len(value), MAX_LOCAL_NAME, value))

    idx = {name: i + 1 for i, (name, *_) in enumerate(MOVES)}  # 0 = MV_NONE
    by_slug = {slug: name for name, slug, *_ in MOVES if slug}

    o = []
    o.append('#pragma once\n#include <stdint.h>\n#include "dex.h"\n\n')
    o.append('// GENERADO por tools/gen_moves.py desde tools/dex_moves.py - no editar\n\n')

    o.append('// Categoria. No hay IV de ataque especial: los movimientos\n'
             '// especiales usan ivAtk/trAtk contra bSpA de la especie, y la\n'
             '// defensa especial ivDef/trDef contra bSpD. El reparto\n'
             '// fisico/especial vive en la especie, no en el individuo.\n')
    o.append('enum : uint8_t { MC_PHYS = %d, MC_SPEC = %d, MC_STATUS = %d };\n\n'
             % (MC_PHYS, MC_SPEC, MC_STATUS))

    o.append('// Efectos. param lleva la carga util de cada uno.\n')
    # generated from dex_moves.py (EF_NAMES): what each one does is documented there
    o.append('enum : uint8_t {\n' + ''.join('  %s = %d,\n' % (k, v) for v, k in EF_NAMES) + '};\n\n')
    o.append('// EF_VARPOW kinds (dex_moves.py VP)\n')
    o.append('enum : uint8_t {\n' + ''.join('  VP_%s = %d,\n' % (k, i) for i, k in enumerate(_D.VP)) + '};\n\n')
    o.append('// MoveEntry::flags (dex_moves.py MF_)\n')
    o.append('enum : uint16_t {\n' + ''.join('  %s = %d,\n' % (k, v) for v, k in MF_NAMES) + '};\n\n')

    o.append('// Mascara de stats para EF_STAGE: un solo delta se aplica a\n'
             '// todos los bits activos (DRAGON DANCE = ATK|SPE +1).\n')
    o.append('enum : uint8_t { ST_ATK = %d, ST_DEF = %d, ST_SPA = %d, ST_SPD = %d, ST_SPE = %d, ST_ACC = %d, ST_EVA = %d };\n'
             % (ST_ATK, ST_DEF, ST_SPA, ST_SPD, ST_SPE, _D.ST_ACC, _D.ST_EVA))
    o.append('enum : uint8_t { TG_SELF = %d, TG_FOE = %d };\n\n' % (TG_SELF, TG_FOE))
    o.append('// Status ailments. Battle-only: they live in the battle state and\n'
             '// clear when it ends, so nothing is written to the pet or replayed\n'
             '// by the RTC offline catch-up.\n'
             'enum : uint8_t {\n'
             '  AIL_NONE = %d, AIL_PARA = %d, AIL_BURN = %d, AIL_POISON = %d,\n'
             '  AIL_SLEEP = %d, AIL_FREEZE = %d, AIL_CONFUSE = %d,\n};\n\n'
             % (AIL_NONE, AIL_PARA, AIL_BURN, AIL_POISON, AIL_SLEEP, AIL_FREEZE, AIL_CONFUSE))

    o.append('struct MoveEntry {\n'
             '  const char *name;\n'
             '  uint8_t type;      // PkType\n'
             '  uint8_t cat;       // MC_*\n'
             '  uint8_t power;     // 0 en estado y en dano fijo\n'
             '  uint8_t acc;       // porcentaje; 0 = no puede fallar\n'
             '  uint8_t effect;    // EF_*\n'
             '  int8_t  param;     // carga util del efecto\n'
             '  uint8_t statMask;  // ST_* (solo EF_STAGE)\n'
             '  int8_t  stages;    // +/- niveles (solo EF_STAGE)\n'
             '  uint8_t target;    // TG_*\n'
             '  uint8_t ailment;   // AIL_*, 0 = none\n'
             '  uint8_t ailChance; // percent, 0 = never\n'
             '  // v3.37 (dex_moves_meta.py)\n'
             '  int8_t  prio;      // priority bracket; EF_PRIORITY moves carry theirs here too\n'
             '  uint8_t crit;      // critical-hit stage the move adds (0 = the base rate)\n'
             '  uint8_t flinch;    // percent chance to flinch the target\n'
             '  uint16_t flags;    // MF_*\n'
             '};\n\n')

    o.append('// Move IDs are 16-bit since v3.37: stored creatures keep the low byte where\n'
             '// it always was and the high byte in an appended field (party.h).\n'
             'enum MoveId : uint16_t {\n  MV_NONE = 0,\n')
    for name, *_ in MOVES:
        o.append('  %s,\n' % ident(name))
    o.append('};\n')
    o.append('#define MOVE_COUNT %d\n\n' % (len(MOVES) + 1))

    def cname(m):
        return 'MC_PHYS' if m == MC_PHYS else ('MC_SPEC' if m == MC_SPEC else 'MC_STATUS')

    def maskname(m):
        if not m:
            return '0'
        bits = [n for n, b in (('ST_ATK', ST_ATK), ('ST_DEF', ST_DEF), ('ST_SPA', ST_SPA),
                               ('ST_SPD', ST_SPD), ('ST_SPE', ST_SPE), ('ST_ACC', _D.ST_ACC),
                               ('ST_EVA', _D.ST_EVA)) if m & b]
        return ' | '.join(bits)

    EFN = {v: k for v, k in EF_NAMES}

    o.append('static const MoveEntry MOVE_TBL[MOVE_COUNT] = {\n')
    o.append('  { "-", T_NORMAL, MC_STATUS, 0, 0, EF_NONE, 0, 0, 0, TG_SELF, AIL_NONE, 0, 0, 0, 0, 0 },  // 0: sin usar\n')
    MFV = {k: v for v, k in MF_NAMES}
    def meta_of(slug, eff, param):
        p, c, f, fl = META.get(slug, (None, 0, 0, []))
        if p is None: p = param if eff == _D.EF_PRIORITY else 0
        return p, c, f, ' | '.join(fl) if fl else '0'
    AILN = {AIL_NONE: 'AIL_NONE', AIL_PARA: 'AIL_PARA', AIL_BURN: 'AIL_BURN',
            AIL_POISON: 'AIL_POISON', AIL_SLEEP: 'AIL_SLEEP',
            AIL_FREEZE: 'AIL_FREEZE', AIL_CONFUSE: 'AIL_CONFUSE'}
    for i, m in enumerate(MOVES):
        (name, slug, typ, cat, power, acc, eff, param, mask, stages, tgt,
         ail, ach) = unpack(m)
        pr, cr, fli, fls = meta_of(slug, eff, param)
        o.append('  { "%s", T_%s, %s, %d, %d, %s, %d, %s, %d, %s, %s, %d, %d, %d, %d, %s },  // %d\n'
                 % (name, typ.upper(), cname(cat), power, acc, EFN[eff], param,
                    maskname(mask), stages, 'TG_SELF' if tgt == TG_SELF else 'TG_FOE',
                    AILN[ail], ach, pr, cr, fli, fls, i + 1))
    o.append('};\n\n')

    # --- learnsets, CSR ---------------------------------------------------
    o.append('// Learnsets en formato CSR: la especie n ocupa\n'
             '// LEARN_TBL[LEARN_OFS[n] .. LEARN_OFS[n + 1]).\n'
             '// nivel 0 = MT, sin requisito. El nivel se guarda pero todavia\n'
             '// no filtra nada: lo decide la fase de UI.\n')
    o.append('struct __attribute__((packed)) LearnEntry { uint16_t move; uint8_t level; };\n\n')

    flat, ofs = [], [0]
    empties = []
    for n in range(0, DEX_COUNT + 1):
        rows = LEARNSETS.get(n, []) if n else []
        for slug, lv in rows:
            flat.append((idx[by_slug[slug]], min(lv, 255), by_slug[slug]))
        if n and not rows:
            empties.append(n)
        ofs.append(len(flat))

    o.append('static const LearnEntry LEARN_TBL[%d] = {\n' % len(flat))
    line = []
    for mid, lv, nm in flat:
        line.append('{ %4d, %3d },' % (mid, lv))
        if len(line) == 6:
            o.append('  ' + ' '.join(line) + '\n')
            line = []
    if line:
        o.append('  ' + ' '.join(line) + '\n')
    o.append('};\n\n')

    # the offsets outgrow 16 bits once the learnsets pass 65535 rows
    o.append('static const %s LEARN_OFS[DEX_COUNT + 2] = {\n' % ('uint16_t' if len(flat) <= 65535 else 'uint32_t'))
    for i in range(0, len(ofs), 12):
        o.append('  ' + ' '.join('%5d,' % v for v in ofs[i:i + 12]) + '\n')
    o.append('};\n\n')

    o.append('// Cuantos movimientos puede usar la especie, y cual es el i-esimo.\n'
             '// DITTO y las dos crisalidas no aprenden ninguno de la lista: para\n'
             '// ellos moveOf() devuelve MV_STRUGGLE en vez de nada.\n')
    o.append('// 16-bit counts: MEW alone learns well over 255 moves once the table grows.\n'
             'static inline uint16_t learnCount(int16_t dex) {\n'
             '  if (dex < 1 || dex > DEX_COUNT) return 0;\n'
             '  return (uint16_t)(LEARN_OFS[dex + 1] - LEARN_OFS[dex]);\n'
             '}\n\n')
    o.append('static inline uint16_t learnMove(int16_t dex, uint16_t i) {\n'
             '  if (i >= learnCount(dex)) return MV_STRUGGLE;\n'
             '  return LEARN_TBL[LEARN_OFS[dex] + i].move;\n'
             '}\n\n')
    o.append('static inline uint8_t learnLevel(int16_t dex, uint16_t i) {\n'
             '  if (i >= learnCount(dex)) return 0;\n'
             '  return LEARN_TBL[LEARN_OFS[dex] + i].level;\n'
             '}\n')

    path = os.path.join(os.path.dirname(__file__), '..', 'moves.h')
    open(path, 'w').write(''.join(o))

    def cstr(value):
        return value.replace('\\', '\\\\').replace('"', '\\"')

    names = []
    names.append('#pragma once\n#include <stdint.h>\n#include "moves.h"\n\n')
    names.append('// GENERATED by tools/gen_moves.py from tools/dex_move_names.py - do not edit\n\n')
    names.append('#define MOVE_NAME_LANG_COUNT %d\n' % len(MOVE_NAME_LANGS))
    names.append('static const char *const MOVE_NAME_TBL[MOVE_NAME_LANG_COUNT][MOVE_COUNT] = {\n')
    for lang_index, lang in enumerate(MOVE_NAME_LANGS):
        names.append('  {  // %s\n    "-",  // 0 MV_NONE\n' % lang)
        for move_index, (move, key) in enumerate(zip(MOVES, move_keys), 1):
            names.append('    "%s",  // %d %s\n'
                         % (cstr(MOVE_NAMES[key][lang_index]), move_index, ident(move[0])))
        names.append('  },\n')
    names.append('};\n')
    names_path = os.path.join(os.path.dirname(__file__), '..', 'move_names.h')
    open(names_path, 'w').write(''.join(names))

    tbl_bytes = len(flat) * 2 + len(ofs) * 2
    print('movimientos: %d (+MV_NONE)' % len(MOVES))
    print('learnsets: %d filas, %.1f de media, %d bytes de tabla'
          % (len(flat), len(flat) / DEX_COUNT, tbl_bytes))
    if empties:
        print('sin movimientos (usaran STRUGGLE): %s' % empties)
    print('guardado %s' % os.path.normpath(path))
    print('guardado %s' % os.path.normpath(names_path))


if __name__ == '__main__':
    main()
