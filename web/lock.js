// The installer's password lock: the browser half of tools/lock_firmware.py.
//
// Like savefile.js this may NOT touch window, document or navigator -- node
// imports it for tools/check_lock.mjs, which decrypts what the Python tool
// encrypted. Only WebCrypto (globalThis.crypto.subtle), which both have.
//
// The app part is b"TPLK" + 12-byte IV + AES-256-GCM ciphertext, under a key of
// PBKDF2-SHA256(password, lock.salt, lock.iterations). GCM is authenticated, so
// a wrong password or a damaged download throws -- it can never hand the
// flasher garbage to write.

const MAGIC = [0x54, 0x50, 0x4c, 0x4b];   // "TPLK"
const CHECK_TEXT = 'TAMAPOKE-LOCK-OK';
const te = new TextEncoder();

export function b64decode(text) {
  const raw = atob(text);
  const out = new Uint8Array(raw.length);
  for (let i = 0; i < raw.length; i++) out[i] = raw.charCodeAt(i);
  return out;
}

// True when a manifest's lock block is one this page knows how to open.
export function lockIsValid(lock) {
  return !!lock && lock.v === 1 && lock.kdf === 'PBKDF2-SHA256'
    && Number.isInteger(lock.iterations) && lock.iterations >= 100000
    && typeof lock.salt === 'string' && typeof lock.check === 'string';
}

export async function deriveKey(password, lock) {
  if (!lockIsValid(lock)) throw new Error('this firmware has no lock this page understands');
  const base = await crypto.subtle.importKey('raw', te.encode(password), 'PBKDF2', false, ['deriveKey']);
  return crypto.subtle.deriveKey(
    { name: 'PBKDF2', hash: 'SHA-256', salt: b64decode(lock.salt), iterations: lock.iterations },
    base, { name: 'AES-GCM', length: 256 }, false, ['decrypt']);
}

export async function decryptBlob(key, bytes) {
  const data = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  if (data.length < 4 + 12 + 16 || MAGIC.some((b, i) => data[i] !== b)) {
    throw new Error('not a locked TamaPoke file');
  }
  const iv = data.subarray(4, 16);
  const plain = await crypto.subtle.decrypt({ name: 'AES-GCM', iv }, key, data.subarray(16));
  return new Uint8Array(plain);
}

// Is this the right password? Decrypts the small known plaintext in the lock
// block, so a typo is reported before the 1.7 MB download.
export async function checkKey(key, lock) {
  try {
    const plain = await decryptBlob(key, b64decode(lock.check));
    return new TextDecoder().decode(plain) === CHECK_TEXT;
  } catch {
    return false;
  }
}

export async function sha256Hex(bytes) {
  const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', bytes));
  return [...digest].map((b) => b.toString(16).padStart(2, '0')).join('');
}

// The whole unlock: key, check, decrypt, and the plaintext's own size and hash
// against what the lock block recorded when it was locked.
export async function unlockApp(password, lock, encrypted) {
  const key = await deriveKey(password, lock);
  if (!(await checkKey(key, lock))) throw new Error('wrong password');
  const plain = await decryptBlob(key, encrypted);
  if (lock.size !== undefined && plain.length !== lock.size) throw new Error('firmware size does not match its lock');
  if (lock.sha256 && (await sha256Hex(plain)) !== lock.sha256) throw new Error('firmware hash does not match its lock');
  return plain;
}
