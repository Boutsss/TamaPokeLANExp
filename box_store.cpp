#include "box_store.h"
#include "party.h"
#include "dex.h"
#include <stdio.h>
#include <string.h>

#if defined(ESP32)
#include <FFat.h>
static char PATH[40] = "/ffat/pcbox.bin";
static char TMP[40] = "/ffat/pcbox.tmp";
static bool mountFs() {
  static int8_t st = -1;
  // formatOnFail: the very first mount on a board formats the partition, which
  // takes a few seconds, once.
  if (st < 0) {
    st = FFat.begin(true) ? 1 : 0;
    Serial.printf("box: FFat %s\n", st ? "mounted" : "MOUNT FAILED");
  }
  return st == 1;
}
#else
static char PATH[300] = "tamapoke-box.bin";
static char TMP[300] = "tamapoke-box.tmp";
static bool mountFs() { return true; }
#endif

void boxStoreSetDir(const char *dir) {
#if !defined(ESP32)
  if (!dir || !dir[0]) return;
  snprintf(PATH, sizeof(PATH), "%s/tamapoke-box.bin", dir);
  snprintf(TMP, sizeof(TMP), "%s/tamapoke-box.tmp", dir);
#else
  (void)dir;
#endif
}

#define BOX_MAGIC 0x58425054u   // "TPBX"
struct BoxHead { uint32_t magic; uint16_t version; uint16_t recSize; uint16_t slots; uint16_t count; };

bool boxStoreReady() { return mountFs(); }

// True only for a WHOLE file: every record the header promises.
static bool readInto(const char *path, PartyMon *box) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  BoxHead h;
  bool good = fread(&h, sizeof(h), 1, f) == 1 && h.magic == BOX_MAGIC &&
              h.recSize >= sizeof(int16_t) && h.recSize <= 128;
  uint8_t buf[128];
  for (uint16_t i = 0; good && i < h.count; i++) {
    uint16_t slot = 0;
    good = fread(&slot, sizeof(slot), 1, f) == 1 && fread(buf, h.recSize, 1, f) == 1;
    if (!good || slot >= BOX_SLOTS) continue;    // a later build's extra slots: not ours
    PartyMon m;
    memcpy(&m, buf, h.recSize < sizeof(PartyMon) ? h.recSize : sizeof(PartyMon));
    if (m.dex < 1 || m.dex > DEX_COUNT) continue; // never index a table with junk
    m.nick[sizeof(m.nick) - 1] = 0;
    box[slot] = m;
  }
  fclose(f);
  return good;
}

bool boxStoreLoad(PartyMon *box) {
  if (!mountFs()) return false;
  // The tmp file is only ever the NEWER copy: it is renamed over the real one
  // as the last step. Found WHOLE, the power went before that step, so it wins;
  // found cut short, it went while writing, and the real one still stands.
  if (readInto(TMP, box)) return true;
  for (uint16_t i = 0; i < BOX_SLOTS; i++) box[i] = PartyMon();
  return readInto(PATH, box);
}

bool boxStoreSave(const PartyMon *box) {
  if (!mountFs()) return false;
  FILE *f = fopen(TMP, "wb");
  if (!f) return false;
  uint16_t count = 0;
  for (uint16_t i = 0; i < BOX_SLOTS; i++) if (!box[i].empty()) count++;
  BoxHead h = { BOX_MAGIC, 1, (uint16_t)sizeof(PartyMon), (uint16_t)BOX_SLOTS, count };
  bool good = fwrite(&h, sizeof(h), 1, f) == 1;
  for (uint16_t i = 0; good && i < BOX_SLOTS; i++) {
    if (box[i].empty()) continue;
    good = fwrite(&i, sizeof(i), 1, f) == 1 && fwrite(&box[i], sizeof(PartyMon), 1, f) == 1;
  }
  good = (fclose(f) == 0) && good;
  if (!good) { remove(TMP); return false; }
  remove(PATH);                     // FAT will not rename over an existing file
  return rename(TMP, PATH) == 0;
}

bool boxStoreSlotIs(uint16_t slot, const PartyMon &m) {
  if (!mountFs() || slot >= BOX_SLOTS) return false;
  FILE *f = fopen(PATH, "rb");
  if (!f) return false;
  BoxHead h;
  bool found = false, same = false;
  if (fread(&h, sizeof(h), 1, f) == 1 && h.magic == BOX_MAGIC && h.recSize == sizeof(PartyMon)) {
    PartyMon r;
    for (uint16_t i = 0; i < h.count; i++) {
      uint16_t s;
      if (fread(&s, sizeof(s), 1, f) != 1 || fread(&r, sizeof(r), 1, f) != 1) break;
      if (s == slot) { found = true; same = memcmp(&r, &m, sizeof(PartyMon)) == 0; break; }
    }
  }
  fclose(f);
  return m.empty() ? !found : (found && same);   // an EMPTY slot is one with no record
}

void boxStoreWipe() {
  if (!mountFs()) return;
  remove(PATH);
  remove(TMP);
}
