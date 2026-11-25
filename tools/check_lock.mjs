// The installer's password lock, end to end: tools/lock_firmware.py (Python,
// the `cryptography` package) encrypts, web/lock.js (WebCrypto) decrypts. Two
// independent implementations of the same format, so this proves they agree
// rather than that one agrees with itself.
//
//   node tools/check_lock.mjs        # also run by tools/emu/tests/run.sh
import { execFileSync } from 'node:child_process';
import { mkdtempSync, writeFileSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { randomBytes } from 'node:crypto';
import { unlockApp, lockIsValid, deriveKey, checkKey } from '../web/lock.js';

const root = dirname(dirname(fileURLToPath(import.meta.url)));
let bad = 0;
const ck = (ok, what) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${what}`); if (!ok) bad++; };
const rejects = async (p) => { try { await p; return false; } catch { return true; } };

const dir = mkdtempSync(join(tmpdir(), 'tplock-'));
try {
  const plain = randomBytes(70000);
  const src = join(dir, 'app.bin');
  const dst = join(dir, 'app.bin.enc');
  writeFileSync(src, plain);
  const lock = JSON.parse(execFileSync('python3', [join(root, 'tools', 'lock_firmware.py'),
    '--in', src, '--out', dst, '--password', 'correct horse 42', '--iterations', '100000', '--print-lock'],
  { encoding: 'utf-8' }));
  const enc = new Uint8Array(readFileSync(dst));

  ck(lockIsValid(lock), 'the lock block is one the page understands');
  ck(!Buffer.from(enc).includes(plain.subarray(0, 64)), 'the locked file does not contain the firmware in the clear');
  const out = await unlockApp('correct horse 42', lock, enc);
  ck(Buffer.compare(Buffer.from(out), plain) === 0, 'the right password gives back the exact bytes');

  const wrong = await deriveKey('correct horse 43', lock);
  ck(!(await checkKey(wrong, lock)), 'a wrong password fails the quick check');
  ck(await rejects(unlockApp('correct horse 43', lock, enc)), 'and cannot unlock the firmware');

  const flipped = enc.slice(); flipped[flipped.length - 100] ^= 1;
  ck(await rejects(unlockApp('correct horse 42', lock, flipped)), 'a damaged download is refused, never flashed');
  ck(await rejects(unlockApp('correct horse 42', lock, enc.subarray(0, enc.length - 1))), 'so is a short one');
  ck(await rejects(unlockApp('correct horse 42', { ...lock, sha256: '0'.repeat(64) }, enc)),
    'the plaintext is checked against the hash recorded at lock time');
  ck(!lockIsValid({ ...lock, iterations: 1000 }), 'a lock with a weak KDF is not accepted');

  let refused = false;
  try {
    execFileSync('python3', [join(root, 'tools', 'lock_firmware.py'), '--in', src, '--out', dst, '--print-lock'],
      { encoding: 'utf-8', env: { ...process.env, TAMAPOKE_PASSWORD: '' }, cwd: dir, stdio: 'pipe' });
  } catch { refused = true; }
  // .install-password at the repo root would legitimately satisfy it
  const hasFile = (() => { try { readFileSync(join(root, '.install-password')); return true; } catch { return false; } })();
  ck(refused || hasFile, 'with no password at all the lock tool refuses instead of shipping it unlocked');
} finally {
  rmSync(dir, { recursive: true, force: true });
}
console.log(bad ? 'FAILURES' : 'all good');
process.exit(bad ? 1 : 0);
