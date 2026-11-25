#include "party.h"
#include <stdlib.h>
#include <string.h>
#include "dex.h"
#include "items.h"
#include "held_items.h"
#include "nvsinfo.h"   // logKeyFailure(): save()/boxSave() used to be silent on failure
#include "box_store.h"

Party party;

// Same NVS namespace as the pet on purpose: WIPE (Pet::factoryReset) calls
// clear() on it, and a factory reset that left the party behind would be a lie.
void Party::begin() {
  // Start from empty: getBytes() leaves the destination untouched when the key
  // is missing, so without this a reload after a wipe would keep showing the
  // old party out of RAM.
  for (auto &s : slots) s = PartyMon();
  // Checked so a failure is at least visible. The panel's warning comes from
  // Pet::saveHealthy(), which shares this namespace and so fails with it.
  if (!prefs.begin("tamapoke", false))
    Serial.println("save: NVS would not open for the party");
  // The blob is raw structs, so growing PartyMon (moves[] was appended in v1.9)
  // changes its stride. Reading an older, shorter blob straight into the new
  // array would land slot 1 onward at the wrong offset and quietly invent a
  // party out of misaligned bytes -- and the dex-range check below would not
  // reliably catch it, since a stray byte is often a valid Pokedex number.
  // So migrate by length: copy each old record into the front of the new one
  // and leave moves[] zeroed for the learnset to fill in.
  size_t stored = prefs.getBytesLength("party");
  if (stored == sizeof(slots)) {
    prefs.getBytes("party", slots, sizeof(slots));
  } else if (stored > sizeof(slots) && stored % sizeof(PartyMon) == 0) {
    // A blob from a build with MORE SLOTS at our own stride. Only this case is
    // unambiguous: stored/sizeof(PartyMon) records, each laid out as we lay them
    // out, so the first PARTY_SLOTS of them are ours to keep. Anything else that
    // is merely "too long" could equally be the same slot count at a BIGGER
    // stride, where a prefix read would land slot 1 at the wrong offset and
    // invent a party out of misaligned bytes -- so that is left empty instead.
    //
    // getBytes copies NOTHING when the stored blob exceeds the buffer, so this
    // has to go through a temporary of the stored size.
    uint8_t *tmp = (uint8_t *)malloc(stored);
    if (tmp) {
      if (prefs.getBytes("party", tmp, stored) == stored)
        memcpy(slots, tmp, sizeof(slots));
      free(tmp);
    }
  } else if (stored && stored % PARTY_SLOTS == 0 && stored < sizeof(slots)) {
    size_t oldStride = stored / PARTY_SLOTS;
    uint8_t old[sizeof(slots)];
    prefs.getBytes("party", old, stored);
    for (int i = 0; i < PARTY_SLOTS; i++)
      memcpy(&slots[i], old + i * oldStride, oldStride);
    save();   // rewrite in the current layout so this only happens once
  }
  // a blob written by an older/newer build could hold nonsense; drop anything
  // that is not a real Pokedex number rather than indexing DEX_TBL with it
  for (auto &s : slots) {
    if (s.dex < 1 || s.dex > DEX_COUNT) s.dex = 0;
    s.nick[sizeof(s.nick) - 1] = 0;
    if (!itemIsHeld(s.held)) s.held = ITEM_NONE;
  }
  loadBox();
}

// The box: allocated once, read from its file, and the old 18-slot NVS box
// moved into it the first time this build boots.
void Party::loadBox() {
  if (!box) {
#if defined(ESP32)
    box = (PartyMon *)ps_malloc(sizeof(PartyMon) * BOX_SLOTS);
#else
    box = (PartyMon *)malloc(sizeof(PartyMon) * BOX_SLOTS);
#endif
    if (!box) { Serial.println("box: no memory for the PC box"); return; }
  }
  for (uint16_t i = 0; i < BOX_SLOTS; i++) box[i] = PartyMon();
  boxStoreLoad(box);
  boxGen++;
  for (uint16_t i = 0; i < BOX_SLOTS; i++) {
    PartyMon &s = box[i];
    if (s.dex < 1 || s.dex > DEX_COUNT) s.dex = 0;
    s.nick[sizeof(s.nick) - 1] = 0;
    if (!itemIsHeld(s.held)) s.held = ITEM_NONE;
  }
  migrateLegacyBox();
}

// The pre-1200 box: BOX_LEGACY_SLOTS records under the NVS key "box", at
// whatever stride the build that wrote it used. Each goes to its OWN slot
// number if that is free (so a player's box looks the same after the upgrade),
// else the first free one. The key is removed only once the file holding them
// has been written and READ BACK -- a failure keeps the key and tries again at
// the next boot, so a creature is never in neither place.
void Party::migrateLegacyBox() {
  size_t stored = prefs.getBytesLength("box");
  if (!stored) return;
  // Two shapes are understood: the 18 slots at the stride of whichever build
  // wrote them (an older, shorter PartyMon), or MORE slots at our own stride
  // (a build with a bigger box). Anything else is left alone, never guessed at.
  size_t stride, count;
  if (stored % BOX_LEGACY_SLOTS == 0 && stored / BOX_LEGACY_SLOTS <= sizeof(PartyMon)) {
    stride = stored / BOX_LEGACY_SLOTS; count = BOX_LEGACY_SLOTS;
  } else if (stored % sizeof(PartyMon) == 0) {
    stride = sizeof(PartyMon); count = stored / sizeof(PartyMon);
    if (count > BOX_SLOTS) count = BOX_SLOTS;
  } else {
    Serial.printf("box: old box key has an unknown size (%u), left alone\n", (unsigned)stored);
    return;
  }
  uint8_t *raw = (uint8_t *)malloc(stored);
  int16_t *placed = (int16_t *)malloc(count * sizeof(int16_t));
  if (!raw || !placed || prefs.getBytes("box", raw, stored) != stored) { free(raw); free(placed); return; }
  uint16_t moved = 0;
  for (size_t i = 0; i < count; i++) {
    placed[i] = -1;
    PartyMon m;
    memcpy(&m, raw + i * stride, stride);
    if (m.dex < 1 || m.dex > DEX_COUNT) continue;
    m.nick[sizeof(m.nick) - 1] = 0;
    if (!itemIsHeld(m.held)) m.held = ITEM_NONE;
    int at = box[i].empty() ? (int)i : boxFirstFree();
    if (at < 0) {
      Serial.println("box: no room to migrate the old box -- kept for next boot");
      free(raw); free(placed); loadBoxFileOnly(); return;
    }
    box[at] = m;
    placed[i] = (int16_t)at;
    moved++;
  }
  free(raw);
  if (moved && !boxSave()) { free(placed); loadBoxFileOnly(); return; }   // nothing removed: next boot retries
  // Each one READ BACK from the file before the old key goes.
  for (size_t i = 0; i < count; i++)
    if (placed[i] >= 0 && !boxStoreSlotIs((uint16_t)placed[i], box[placed[i]])) {
      Serial.println("box: migration not confirmed on flash -- old box kept");
      free(placed);
      return;
    }
  free(placed);
  prefs.remove("box");
  Serial.printf("box: moved %u from the old box into the PC box\n", (unsigned)moved);
}

void Party::loadBoxFileOnly() {
  for (uint16_t i = 0; i < BOX_SLOTS; i++) box[i] = PartyMon();
  boxStoreLoad(box);
}

void Party::boxWipe() {
  if (box) for (uint16_t i = 0; i < BOX_SLOTS; i++) box[i] = PartyMon();
  boxStoreWipe();
  prefs.remove("box");
}

bool Party::save() {
  if (prefs.putBytes("party", slots, sizeof(slots)) != sizeof(slots)) {
    logKeyFailure("party", "party");
    return false;
  }
  return true;
}

bool Party::boxSave() {
  if (!box) return false;
  boxGen++;
  if (!boxStoreSave(box)) {
    Serial.println("save: the PC box file did not write");
    return false;
  }
  return true;
}

// The same bytes as the record on flash? (PartyMon has padding, so compare the
// key's blob as it is stored rather than field by field.)
static bool readBackMatches(Preferences &pr, const char *key, const void *ram, size_t n) {
  if (pr.getBytesLength(key) != n) return false;
  uint8_t *tmp = (uint8_t *)malloc(n);
  if (!tmp) return false;
  bool ok = pr.getBytes(key, tmp, n) == n && memcmp(tmp, ram, n) == 0;
  free(tmp);
  return ok;
}

static bool sameMon(const PartyMon &a, const PartyMon &b) {
  return a.dex == b.dex && a.level == b.level && a.ageMinutes == b.ageMinutes &&
         a.ivAtk == b.ivAtk && a.ivDef == b.ivDef && a.ivSpe == b.ivSpe && a.ivHp == b.ivHp &&
         a.trAtk == b.trAtk && a.trDef == b.trDef && a.trSpe == b.trSpe &&
         a.shiny == b.shiny && !memcmp(a.moves, b.moves, sizeof(a.moves)) &&
         !strncmp(a.nick, b.nick, sizeof(a.nick));
}

bool Party::storeDurably(const PartyMon &m, int16_t *where) {
  if (m.empty()) return false;
  int i = firstFree();
  if (i >= 0) {
    PartyMon old = slots[i];
    slots[i] = m;
    if (save() && readBackMatches(prefs, "party", slots, sizeof(slots))) { if (where) *where = (int16_t)i; return true; }
    slots[i] = old;
    save();
    return false;
  }
  i = boxFirstFree();
  if (i < 0) return false;
  PartyMon old = box[i];
  box[i] = m;
  if (boxSave() && boxStoreSlotIs((uint16_t)i, box[i])) { if (where) *where = (int16_t)(PARTY_SLOTS + i); return true; }
  box[i] = old;
  boxSave();
  return false;
}

bool Party::takeOutExact(int16_t where, const PartyMon &expect) {
  if (where < 0 || where >= PARTY_SLOTS + BOX_SLOTS) return false;
  bool inBox = where >= PARTY_SLOTS;
  PartyMon &s = inBox ? box[where - PARTY_SLOTS] : slots[where];
  if (s.empty() || !sameMon(s, expect)) return false;
  PartyMon old = s;
  s = PartyMon();
  bool ok = inBox ? (boxSave() && boxStoreSlotIs((uint16_t)(where - PARTY_SLOTS), PartyMon()))
                  : (save() && readBackMatches(prefs, "party", slots, sizeof(slots)));
  if (ok) return true;
  s = old;
  if (inBox) boxSave(); else save();
  return false;
}

uint16_t Party::boxCount() const {
  uint16_t n = 0;
  if (box) for (uint16_t i = 0; i < BOX_SLOTS; i++)
    if (!box[i].empty()) n++;
  return n;
}

int Party::boxFirstFree() const {
  for (int i = 0; i < BOX_SLOTS; i++)
    if (box[i].empty()) return i;
  return -1;
}

bool Party::boxAdd(const PartyMon &m) {
  int i = boxFirstFree();
  if (i < 0) return false;
  box[i] = m;
  boxSave();
  return true;
}

void Party::boxReleaseAt(uint16_t i) {
  if (i >= BOX_SLOTS) return;
  heldReturn(box[i].held);
  box[i] = PartyMon();
  boxSave();
}

void Party::swapPartyBox(uint8_t partyIdx, uint16_t boxIdx) {
  if (partyIdx >= PARTY_SLOTS || boxIdx >= BOX_SLOTS) return;
  PartyMon t = slots[partyIdx];
  slots[partyIdx] = box[boxIdx];
  box[boxIdx] = t;
  save();
  boxSave();
}

uint8_t Party::count() const {
  uint8_t n = 0;
  for (auto &s : slots)
    if (!s.empty()) n++;
  return n;
}

int Party::firstFree() const {
  for (int i = 0; i < PARTY_SLOTS; i++)
    if (slots[i].empty()) return i;
  return -1;
}

bool Party::add(const PartyMon &m) {
  int i = firstFree();
  if (i < 0) return false;
  slots[i] = m;
  save();
  return true;
}

void Party::replaceAt(uint8_t i, const PartyMon &m) {
  if (i >= PARTY_SLOTS) return;
  slots[i] = m;
  save();
}

void Party::releaseAt(uint8_t i) {
  if (i >= PARTY_SLOTS) return;
  heldReturn(slots[i].held);
  slots[i] = PartyMon();
  save();
}

// Mirrors calcStat() in pet.cpp: base + level + IV contribution + training.
// Kept in step with it by hand; there is no shared home for it that both the
// live pet and a frozen party member could use without dragging Pet in here.
static uint16_t calcStat(uint8_t base, uint8_t iv, uint16_t lvl, uint8_t tr) {
  return (uint16_t)base + lvl + (uint32_t)iv * lvl / 100 + tr;
}

uint16_t Party::atkOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bAtk, m.ivAtk, m.level, m.trAtk);
}
uint16_t Party::defOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bDef, m.ivDef, m.level, m.trDef);
}
uint16_t Party::speOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bSpe, m.ivSpe, m.level, m.trSpe);
}
uint16_t Party::vitOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bHp, m.ivHp, m.level, 10);
}
// Special reuses the physical IV and training, same rule as Pet::spaStat().
uint16_t Party::spaOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bSpA, m.ivAtk, m.level, m.trAtk);
}
uint16_t Party::spdOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bSpD, m.ivDef, m.level, m.trDef);
}
