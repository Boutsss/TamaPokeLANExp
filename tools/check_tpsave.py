#!/usr/bin/env python3
"""The desktop save editor (tools/debugger/tpsave.py) against what the REAL
firmware writes: the emulator builds a save -- a party, creatures in two PC
boxes, a named box -- and EXPORTs it; tpsave must decode every one of them,
and encoding the PC box back must reproduce the firmware's own pmNN bytes.

    python3 tools/check_tpsave.py        # also runs inside tools/emu/tests/run.sh

A second copy of the format written by the same hand would only prove the two
copies agree; this compares against the C++.
"""
import os, subprocess, sys, tempfile
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

def main():
    cmds = ['EGG 6', 'PARTY 25', 'PARTY 26', 'BOXADD 7 1', 'BOXADD 143 30', 'BOXADD 150 1200', 'EXPORT', 'WIPE']
    with tempfile.TemporaryDirectory() as d:
        env = dict(os.environ, SDL_VIDEODRIVER='dummy')
        out = subprocess.run([EMU, '--save', os.path.join(d, 'f.nvs'), '--wipe'], input='\n'.join(cmds) + '\n',
                             capture_output=True, text=True, env=env, timeout=120, cwd=d).stdout
    lines = [l.strip() for l in out.splitlines() if l.startswith('IMPORT ')]
    raw = bytes.fromhex(''.join(l[7:] for l in lines))
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
    print('all good' if not bad else 'FAILURES')
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
