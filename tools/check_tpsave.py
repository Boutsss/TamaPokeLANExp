#!/usr/bin/env python3
"""The desktop save editor (tools/debugger/tpsave.py) against what the REAL
firmware writes: the emulator builds a save -- a party, creatures in two PC
boxes, a named box -- and EXPORTs it; tpsave must decode every one of them,
and encoding the PC box back must reproduce the firmware's own pmNN bytes.

    python3 tools/check_tpsave.py        # also runs inside tools/emu/tests/run.sh

A second copy of the format written by the same hand would only prove the two
copies agree; this compares against the C++.
"""
import os, re, subprocess, sys, tempfile
from collections import OrderedDict
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(HERE, 'debugger'))
import tpsave

EMU = os.path.join(HERE, 'emu', 'tamapoke-emu')
bad = 0
def ck(ok, what):
    global bad
    print(('PASS  ' if ok else 'FAIL  ') + what)
    if not ok: bad += 1

def emu(cmds, then=None):
    """Runs the REAL firmware in the emulator on a fresh save and returns the
    bytes of the last EXPORT it printed. `then`, if given, is a second boot on
    the SAME save -- an IMPORT restarts the board (exit, in the emulator), so a
    restore and the export that checks it are two boots, as on hardware."""
    with tempfile.TemporaryDirectory() as d:
        env = dict(os.environ, SDL_VIDEODRIVER='dummy')
        out = subprocess.run([EMU, '--save', os.path.join(d, 'f.nvs'), '--wipe'], input='\n'.join(cmds) + '\n',
                             capture_output=True, text=True, env=env, timeout=180, cwd=d).stdout
        if then:
            out = subprocess.run([EMU, '--save', os.path.join(d, 'f.nvs')], input='\n'.join(then) + '\n',
                                 capture_output=True, text=True, env=env, timeout=180, cwd=d).stdout
    # An EXPORT block is 'IMPORT <hex>' lines; the firmware ALSO prints status
    # lines like 'IMPORT OK', which are not data and end a block.
    hexline = re.compile(r'^IMPORT [0-9A-Fa-f]+$')
    blocks, cur = [], []
    for l in out.splitlines():
        if hexline.match(l.strip()):
            cur.append(l.strip())
        elif cur:
            blocks.append(cur); cur = []
    if cur:
        blocks.append(cur)
    return bytes.fromhex(''.join(l[7:] for l in blocks[-1])) if blocks else b''

def main():
    cmds = ['EGG 6', 'LVL 30', 'IV 31 20 25 28', 'TR 40 30 20', 'PARTY 25', 'PARTY 26',
            'BOXADD 7 1', 'BOXADD 143 30', 'BOXADD 150 1200', 'EXPORT', 'WIPE']
    raw = emu(cmds)
    s = tpsave.decode_save(raw, dex_h_path=os.path.join(ROOT, 'dex.h'))
    ck(s['ok'], 'the firmware export parses (%s)' % (s.get('error') or '%d fields' % s.get('field_count', 0)))
    if not s['ok']:
        return 1
    party = [(m['dex'], m['level']) for m in s['party'] if not m['empty']]
    ck(party == [(25, 50), (26, 50)], 'the party decodes at the 56-byte record: %r' % party)
    box = {i: m['dex'] for i, m in enumerate(s['box']) if not m['empty']}
    ck(len(s['box']) == tpsave.BOX_SLOTS and box == {0: 7, 29: 143, 1199: 150},
       'the PC box decodes from its pmNN fields, each in its own slot: %r' % box)
    pc = {k: v[1] for k, v in s['fields'].items() if k[:2] in ('pc', 'pm')}
    ck(tpsave.encode_pc_box(s['box']) == pc, 're-encoding the box reproduces the firmware\'s own pmNN bytes')
    ck(set(pc) == {'pm00', 'pm39'}, 'only boxes holding something travel (%s)' % sorted(pc))

    # ---- v3.50: the checkpoints are the save; the editor reads and writes them
    legacy = sorted(k for k in s['fields'] if k in tpsave.LEGACY_KEYS)
    ck(not legacy, 'the firmware exports no legacy key any more (%s)' % (legacy or 'none'))
    live, ply = s['live_pet'], s['player']
    ck(live.get('dexn') == 6 and (live.get('ivat'), live.get('ivdf'), live.get('ivsp'), live.get('ivhp')) == (31, 20, 25, 28)
       and (live.get('tatk'), live.get('tdef'), live.get('tspe')) == (40, 30, 20),
       'the live pet is shown from its checkpoint: %r' % {k: live.get(k) for k in ('dexn', 'ivat', 'ivhp', 'tatk')})
    ck(all(k in ply for k in tpsave.PLAYER_LEGACY if k in tpsave.SAVE_FIELD_KIND),
       'and the player from theirs, every field (proved both ways by the round trip below)')
    pet0 = s['pet_checkpoint']

    # An edit, as the editor makes one: values written into the legacy view,
    # folded into the checkpoints, then IMPORTED BY THE FIRMWARE and exported again.
    f = OrderedDict(s['fields'])
    f['ivat'] = (tpsave.SK_U8, tpsave.encode_scalar(tpsave.SK_U8, 12))
    f['bond'] = (tpsave.SK_U8, tpsave.encode_scalar(tpsave.SK_U8, 77))
    f['tnam'] = (tpsave.SK_STR, tpsave.encode_scalar(tpsave.SK_STR, 'BROCK'))
    f['badg'] = (tpsave.SK_U16, tpsave.encode_scalar(tpsave.SK_U16, 0x0F))
    folded = tpsave.fold_legacy_into_checkpoints(f)
    ck(not any(k in tpsave.LEGACY_KEYS for k in folded), 'folding leaves no legacy key behind')
    edited = tpsave.encode_export(folded)
    lines = [l for l in tpsave.export_text_from_bytes(edited).splitlines() if l.startswith('IMPORT')]
    back = tpsave.decode_save(emu(lines, then=['EXPORT', 'WIPE']), dex_h_path=os.path.join(ROOT, 'dex.h'))
    ck(back['ok'], 'the firmware accepts the edited save')
    if back['ok']:
        lv, pv, pc2 = back['live_pet'], back['player'], back['pet_checkpoint']
        ck(lv.get('ivat') == 12 and lv.get('bond') == 77 and pv.get('tnam') == 'BROCK' and pv.get('badg') == 0x0F,
           'and its own export shows every edit: ivat=%s bond=%s tnam=%s badg=%s' % (lv.get('ivat'), lv.get('bond'), pv.get('tnam'), pv.get('badg')))
        same = all(pc2[k] == pet0[k] for k in ('speciesId', 'ivDef', 'ivSpe', 'ivHp', 'trAtk', 'trDef', 'trSpe', 'moves', 'nick'))
        ck(same, 'and everything NOT edited came through unchanged (species, IVs, training, moves, nickname)')

    # ---- the Game Corner coins and the armored skin, through the firmware
    raw2 = emu(['EGG 150', 'LVL 20', 'EXPORT', 'WIPE'])
    s2 = tpsave.decode_save(raw2, dex_h_path=os.path.join(ROOT, 'dex.h'))
    f2 = OrderedDict(s2['fields'])
    f2['coin2'] = (tpsave.SK_U32, tpsave.encode_scalar(tpsave.SK_U32, 1234567))
    f2['skin'] = (tpsave.SK_U8, tpsave.encode_scalar(tpsave.SK_U8, 1))
    lines2 = [l for l in tpsave.export_text_from_bytes(tpsave.encode_export(tpsave.fold_legacy_into_checkpoints(f2))).splitlines()
              if l.startswith('IMPORT')]
    b2 = tpsave.decode_save(emu(lines2, then=['EXPORT', 'WIPE']), dex_h_path=os.path.join(ROOT, 'dex.h'))
    ck(b2['ok'] and b2['player'].get('coin2') == 1234567 and b2['live_pet'].get('skin') == 1,
       'coins (past 65,535) and the skin go in through the editor and come back from the firmware: %s / %s'
       % (b2['player'].get('coin2'), b2['live_pet'].get('skin')))
    tower = tpsave.parse_tower_avatars(os.path.join(ROOT, 'avatar_names.h'))
    rec = bytearray(152); rec[0], rec[1] = 1, 152
    t0 = tower[0]                                  # a costume, in the 128..255 block
    rec[44 + (t0 - 128) // 8] |= 1 << (t0 % 8)
    rec[14] |= 1                                   # and shop avatar 0, an ordinary prize
    c = tpsave.decode_casino(bytes(rec), tower)
    ck(len(tower) == 94 and c['avatars_owned'] == 2 and c['legends_owned'] == 1,
       'the Game Corner summary counts owned prizes and, of them, legendary costumes (%s)' % c)
    print('all good' if not bad else 'FAILURES')
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
