#include "save.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include "party.h"   // MAX_VAL is derived from the box, see below
#include "ghost.h"
#include "card.h"
#include "box_store.h"
#include "nvsinfo.h" // logKeyFailure(): writeField() (the IMPORT path) used to be silent too

// Every key the firmware persists. Adding one here is the whole job of adding
// it to the backup; save_test fails if a key exists in NVS and not in this list.
const SaveField SAVE_FIELDS[] = {
  // the creature
  { "init", SK_BOOL },  { "full", SK_U8 },    { "joy", SK_U8 },
  { "ene", SK_U8 },     { "hyg", SK_U8 },     { "poop", SK_U8 },
  { "wgt", SK_U8 },     { "age", SK_U32 },    { "dexn", SK_I16 },
  { "eggT2", SK_I16 },  { "crack", SK_U8 },   { "mist", SK_U8 },
  { "sleep", SK_BOOL }, { "lend", SK_U8 },    { "seen", SK_U32 },
  // The alternating checkpoints. Redundant with the legacy scalars beside them
  // on purpose: these are what the firmware actually BELIEVES on load, so a
  // backup without them would restore a save the device then half-ignores.
  { "petA", SK_BYTES }, { "petB", SK_BYTES },
  { "plyA", SK_BYTES }, { "plyB", SK_BYTES },
  { "bond", SK_U8 },    { "nick", SK_STR },   { "froz", SK_BOOL },
  // individual values and training
  { "ivat", SK_U8 },    { "ivdf", SK_U8 },    { "ivsp", SK_U8 },
  { "ivhp", SK_U8 },    { "tatk", SK_U8 },    { "tdef", SK_U8 },
  { "tspe", SK_U8 },
  // moves
  { "mvs", SK_BYTES },  { "mvlv", SK_U8 },
  // flags
  { "bk", SK_BOOL },    { "shy", SK_BOOL },   { "eshy", SK_BOOL },
  { "stpk", SK_BOOL },  { "evop", SK_U8 },    { "slpa", SK_U8 },    { "rtpn", SK_BOOL },
  // the player: outlives every creature, which is exactly why it must be here
  { "tnam", SK_STR },   { "avtr", SK_U8 },    { "badg", SK_U16 },
  { "reg", SK_U8 },     { "eggR", SK_BYTES }, { "regn", SK_U8 },
  { "badgX", SK_BYTES },{ "badhX", SK_BYTES },
  { "badh", SK_U16 },   { "dexreg", SK_BYTES }, { "dexsh", SK_BYTES },
  { "strk", SK_U16 },   { "bstrk", SK_U16 },  { "cday", SK_U32 },
  { "medal", SK_U16 },  { "tmedal", SK_U16 }, { "mstone", SK_U16 },
  { "ghi", SK_U16 },    { "shi", SK_U16 },    { "qhi", SK_U16 },
  // the Poke Mart: 1 pedometer step = $1
  { "wlt", SK_U32 },    { "stps", SK_U32 },
  // the banked creatures, and what you are carrying
  { "party", SK_BYTES }, { "box", SK_BYTES }, { "bag", SK_BYTES },
  // remembered LAN opponents -- its own key, same reasoning as the box
  { "rivals", SK_BYTES },
  // settings, so a restored device plays the way it did
  { "lang", SK_U8 },    { "snd", SK_BOOL },   { "vol", SK_U8 },
  { "tasks", SK_BYTES },
  // Pokedex rewards paid so far (dex_rewards.cpp)
  { "dxgf", SK_U8 },    { "dxrw", SK_U8 },
  // the daycare's queued egg, waiting IVs and cooldown (breed.cpp)
  { "dcare", SK_BYTES },
  // the live creature's personality (legacy mirror of the checkpoint byte)
  { "prsn", SK_U8 },
  // the shiny chain (chain.cpp)
  { "wchain", SK_BYTES },
  // Battle Tower run and gym rematches won (tower.cpp)
  // the weekly gym challenge (challenge.cpp)
  { "chal", SK_BYTES },
  // trainer rank (rank.cpp)
  // the arcade (ui_arcade.ino): bests, the prize day, grades paid today + first-S bits
  { "arcHi", SK_BYTES },  { "arcDay", SK_U32 },  { "arcRw", SK_BYTES },
  { "rkTop", SK_U8 },  { "rkCat", SK_U16 },  { "rkChW", SK_U16 },  { "rkChL", SK_U16 },  { "rkAvG", SK_U8 },
  { "twStrk", SK_U16 },  { "twBest", SK_U16 },  { "twSeed", SK_U32 },  { "rmWin", SK_BYTES },
  // friends: gifts and visits between two boards (friends.cpp)
  // the Game Corner: coins ride the player checkpoint (legacy "coin");
  // prizes owned and equipped (casino.cpp)
  { "coin", SK_U16 },  { "casino", SK_BYTES },
  // LAN rivals remembered for PRACTICE (ghost.cpp)
  { "ghosts", SK_BYTES },
  // trainer cards collected from friends (card.cpp)
  { "cards", SK_BYTES },
  { "frId", SK_U32 },  { "frGday", SK_U32 },  { "frVday", SK_U32 },  { "frJrn", SK_BYTES },
};
const uint16_t SAVE_FIELD_COUNT = sizeof(SAVE_FIELDS) / sizeof(SAVE_FIELDS[0]);

// The largest single value in the backup, which is the box: BOX_SLOTS whole
// records. DERIVED, never a literal. It was written as 768 when a PartyMon was
// 42 bytes, and the moment the record grew the box silently stopped being
// restored while every other field still was -- a backup that is quietly
// partial is worse than none. save_test caught exactly that.
//
// With the 1200-slot PC box the box is no longer one NVS value: it travels as
// one "pcNN" field per box of 30, occupied slots only ([slot in box][record]).
// MAX_VAL is the largest of everything that does travel as one value -- each
// size DERIVED from its own header, never restated here.
#define PC_REC (1 + sizeof(PartyMon))
#define PC_FIELD_MAX (BOX_PER_BOX * PC_REC)
static constexpr size_t cmax(size_t a, size_t b) { return a > b ? a : b; }
#define MAX_VAL cmax(cmax(sizeof(PartyMon) * BOX_LEGACY_SLOTS, PC_FIELD_MAX), \
                     cmax(GHOST_BLOB_MAX, CARD_BOOK_BLOB_MAX))
static_assert(sizeof(PartyMon) * PARTY_SLOTS <= MAX_VAL, "the party must fit a field");

static uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint16_t)p[i] << 8;
    for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021)
                                                : (uint16_t)(c << 1);
  }
  return c;
}

static uint8_t widthOf(uint8_t kind) {
  switch (kind) {
    case SK_U8: case SK_I8: case SK_BOOL: return 1;
    case SK_U16: case SK_I16: return 2;
    case SK_U32: return 4;
    default: return 0;               // variable
  }
}

// Reads one field out of NVS. Returns its length, or -1 if the key is absent --
// absent is normal (a fresh save has no party) and is simply left out.
static int readField(Preferences &p, const SaveField &f, uint8_t *val) {
  if (!p.isKey(f.key)) return -1;
  switch (f.kind) {
    case SK_U8:   val[0] = p.getUChar(f.key, 0); return 1;
    case SK_I8:   val[0] = (uint8_t)p.getChar(f.key, 0); return 1;
    case SK_BOOL: val[0] = p.getBool(f.key, false) ? 1 : 0; return 1;
    case SK_U16: { uint16_t v = p.getUShort(f.key, 0); memcpy(val, &v, 2); return 2; }
    case SK_I16: { int16_t v = p.getShort(f.key, 0); memcpy(val, &v, 2); return 2; }
    case SK_U32: { uint32_t v = p.getUInt(f.key, 0); memcpy(val, &v, 4); return 4; }
    case SK_BYTES: {
      size_t n = p.getBytesLength(f.key);
      if (n > MAX_VAL) return -1;
      p.getBytes(f.key, val, n);
      return (int)n;
    }
    case SK_STR: {
      char tmp[64] = {0};
      p.getString(f.key, tmp, sizeof(tmp));
      size_t n = strlen(tmp);
      memcpy(val, tmp, n);
      return (int)n;
    }
  }
  return -1;
}

// Was entirely fire-and-forget before -- a failed write during IMPORT used to
// leave that one field silently holding whatever was there before the restore
// (or NVS's own default), with the rest of the blob looking like it landed.
static void writeField(Preferences &p, const SaveField &f,
                       const uint8_t *val, uint16_t n) {
  switch (f.kind) {
    case SK_U8:
      if (n == 1 && !p.putUChar(f.key, val[0])) logKeyFailure("import", f.key);
      break;
    case SK_I8:
      if (n == 1 && !p.putChar(f.key, (int8_t)val[0])) logKeyFailure("import", f.key);
      break;
    case SK_BOOL:
      if (n == 1 && !p.putBool(f.key, val[0] != 0)) logKeyFailure("import", f.key);
      break;
    case SK_U16:
      if (n == 2) {
        uint16_t v; memcpy(&v, val, 2);
        if (!p.putUShort(f.key, v)) logKeyFailure("import", f.key);
      }
      break;
    case SK_I16:
      if (n == 2) {
        int16_t v; memcpy(&v, val, 2);
        if (!p.putShort(f.key, v)) logKeyFailure("import", f.key);
      }
      break;
    case SK_U32:
      if (n == 4) {
        uint32_t v; memcpy(&v, val, 4);
        if (!p.putUInt(f.key, v)) logKeyFailure("import", f.key);
      }
      break;
    case SK_BYTES:
      if (n && p.putBytes(f.key, val, n) != n) logKeyFailure("import", f.key);
      break;
    case SK_STR: {
      char tmp[64] = {0};
      if (n >= sizeof(tmp)) return;
      memcpy(tmp, val, n);
      tmp[n] = 0;
      if (!p.putString(f.key, tmp)) logKeyFailure("import", f.key);
      break;
    }
  }
}

size_t saveExportSize() {
  size_t n = SAVE_HDR + 2;
  for (uint16_t i = 0; i < SAVE_FIELD_COUNT; i++)
    n += 1 + strlen(SAVE_FIELDS[i].key) + 1 + 2 + MAX_VAL / 16;
  n += BOX_BOXES * (1 + 4 + 1 + 2 + PC_FIELD_MAX);   // the PC box, if every box were full
  return n + 1024;                   // the two big blobs
}

size_t saveExport(uint8_t *out, size_t cap) {
  if (cap < SAVE_HDR + 2) return 0;
  Preferences p;
  p.begin("tamapoke", true);
  size_t at = SAVE_HDR;
  uint16_t count = 0;
  static uint8_t val[MAX_VAL];
  for (uint16_t i = 0; i < SAVE_FIELD_COUNT; i++) {
    const SaveField &f = SAVE_FIELDS[i];
    int n = readField(p, f, val);
    if (n < 0) continue;             // absent: nothing to back up
    size_t klen = strlen(f.key);
    if (klen > 15) continue;         // NVS keys cannot be longer anyway
    if (at + 1 + klen + 1 + 2 + (size_t)n + 2 > cap) { p.end(); return 0; }
    out[at++] = (uint8_t)klen;
    memcpy(out + at, f.key, klen); at += klen;
    out[at++] = f.kind;
    out[at++] = (uint8_t)(n & 0xFF);
    out[at++] = (uint8_t)(n >> 8);
    memcpy(out + at, val, (size_t)n); at += (size_t)n;
    count++;
  }
  p.end();
  // The PC box: one field per box that holds anything.
  for (uint16_t b = 0; party.box && b < BOX_BOXES; b++) {
    size_t vn = 0;
    for (uint8_t k = 0; k < BOX_PER_BOX; k++) {
      const PartyMon &m = party.box[b * BOX_PER_BOX + k];
      if (m.empty()) continue;
      val[vn++] = k;
      memcpy(val + vn, &m, sizeof(PartyMon)); vn += sizeof(PartyMon);
    }
    if (!vn) continue;
    char key[8];
    snprintf(key, sizeof(key), "pc%02u", (unsigned)b);
    size_t klen = strlen(key);
    if (at + 1 + klen + 1 + 2 + vn + 2 > cap) return 0;
    out[at++] = (uint8_t)klen;
    memcpy(out + at, key, klen); at += klen;
    out[at++] = SK_BYTES;
    out[at++] = (uint8_t)(vn & 0xFF);
    out[at++] = (uint8_t)(vn >> 8);
    memcpy(out + at, val, vn); at += vn;
    count++;
  }
  out[0] = SAVE_MAGIC0; out[1] = SAVE_MAGIC1;
  out[2] = SAVE_MAGIC2; out[3] = SAVE_MAGIC3;
  out[4] = SAVE_VERSION;
  out[5] = (uint8_t)(count & 0xFF);
  out[6] = (uint8_t)(count >> 8);
  out[7] = 0;
  uint16_t c = crc16(out, at);
  out[at++] = (uint8_t)(c & 0xFF);
  out[at++] = (uint8_t)(c >> 8);
  return at;
}

// Looks a key up in the table. An unknown key is skipped rather than trusted:
// the kind on the wire decides nothing, the table does.
static const SaveField *fieldFor(const char *key, uint8_t kind) {
  for (uint16_t i = 0; i < SAVE_FIELD_COUNT; i++)
    if (SAVE_FIELDS[i].kind == kind && !strcmp(SAVE_FIELDS[i].key, key))
      return &SAVE_FIELDS[i];
  return nullptr;
}

// "pc00".."pc39": a box of the PC box. -1 if the key is anything else.
static int pcBoxOf(const char *key, uint8_t klen) {
  if (klen != 4 || key[0] != 'p' || key[1] != 'c' || key[2] < '0' || key[2] > '9' || key[3] < '0' || key[3] > '9') return -1;
  int b = (key[2] - '0') * 10 + (key[3] - '0');
  return b < BOX_BOXES ? b : -1;
}

bool saveImport(const uint8_t *in, size_t n) {
  if (n < SAVE_HDR + 2) return false;
  if (in[0] != SAVE_MAGIC0 || in[1] != SAVE_MAGIC1 ||
      in[2] != SAVE_MAGIC2 || in[3] != SAVE_MAGIC3) return false;
  if (in[4] != SAVE_VERSION) return false;
  uint16_t count = (uint16_t)in[5] | ((uint16_t)in[6] << 8);
  uint16_t want = (uint16_t)in[n - 2] | ((uint16_t)in[n - 1] << 8);
  if (crc16(in, n - 2) != want) return false;

  // PASS ONE: walk the whole thing and prove it parses. Nothing is written
  // yet -- a restore that fell over halfway would leave a save that is neither
  // the old one nor the new one, which is the one outcome worse than no backup.
  size_t at = SAVE_HDR;
  uint16_t seen = 0;
  while (at + 4 <= n - 2) {
    uint8_t klen = in[at];
    if (!klen || klen > 15 || at + 1 + klen + 3 > n - 2) return false;
    size_t vat = at + 1 + klen;
    uint8_t kind = in[vat];
    uint16_t vlen = (uint16_t)in[vat + 1] | ((uint16_t)in[vat + 2] << 8);
    if (vlen > MAX_VAL) return false;
    if (vat + 3 + vlen > n - 2) return false;
    uint8_t w = widthOf(kind);
    if (w && vlen != w) return false;          // a scalar of the wrong width
    // a box of the PC box: whole records, each in a real slot of that box
    if (pcBoxOf((const char *)in + at + 1, klen) >= 0) {
      if (kind != SK_BYTES || vlen % PC_REC) return false;
      for (uint16_t r = 0; r < vlen; r += PC_REC)
        if (in[vat + 3 + r] >= BOX_PER_BOX) return false;
    }
    at = vat + 3 + vlen;
    seen++;
  }
  if (at != n - 2 || seen != count) return false;

  // PASS TWO: it parses, so commit. Cleared first, or keys the backup does not
  // contain would survive from whatever save happened to be on the device.
  Preferences p;
  p.begin("tamapoke", false);
  p.clear();
  // The PC box is replaced too: emptied here, filled from the pcNN fields
  // below, written ONCE at the end. A backup from before the PC box carries
  // the old "box" key instead, which the next boot migrates.
  if (party.box) for (uint16_t i = 0; i < BOX_SLOTS; i++) party.box[i] = PartyMon();
  boxStoreWipe();
  bool pcAny = false;
  at = SAVE_HDR;
  while (at + 4 <= n - 2) {
    uint8_t klen = in[at];
    char key[16] = {0};
    memcpy(key, in + at + 1, klen);
    size_t vat = at + 1 + klen;
    uint8_t kind = in[vat];
    uint16_t vlen = (uint16_t)in[vat + 1] | ((uint16_t)in[vat + 2] << 8);
    int pb = pcBoxOf(key, klen);
    if (pb >= 0 && party.box) {
      for (uint16_t r = 0; r < vlen; r += PC_REC) {
        PartyMon m;
        memcpy(&m, in + vat + 3 + r + 1, sizeof(PartyMon));
        if (m.dex < 1 || m.dex > DEX_COUNT) continue;   // never index a table with junk
        m.nick[sizeof(m.nick) - 1] = 0;
        party.box[pb * BOX_PER_BOX + in[vat + 3 + r]] = m;
        pcAny = true;
      }
    } else {
      const SaveField *f = fieldFor(key, kind);
      if (f) writeField(p, *f, in + vat + 3, vlen);
    }
    at = vat + 3 + vlen;
  }
  p.end();
  if (pcAny) party.boxSave();
  return true;
}
