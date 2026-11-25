// Does a save made by the RELEASED v3.28 firmware survive this branch?
//
// Both fixtures were written by the real v3.28 build (tag v3.28, the
// reallyjustsomeguy fork), driven through its serial console in the emulator --
// not built by this test -- so this checks the new code against old firmware's
// own bytes rather than against this file's idea of the old format. See
// CLAUDE.md § "A test that proves the transcription rather than the firmware".
//
//   tools/fixtures/v328.nvs     the raw NVS store: what a board holds after an
//                               install WITHOUT erase
//   tools/fixtures/v328.tpsave  that store's EXPORT: what the web installer's
//                               backup hands back
//
// What v3.28 put in it (from its own STATS / PARTY / BAG output):
//   live  CHARIZARD "BLAZE" Lv30, IV 31/20/25/28, training 40/30/20
//   box   SNORLAX "SNOOZE" Lv12, IV 17/25/24/9, banked by a real farewell
//   party PIKACHU MEWTWO EEVEE BULBASAUR CHARMANDER SQUIRTLE, Lv50, IV 20, tr 50
//   bag   POKE BALL x17, POTION x10, PROTEIN x2 · wallet $500, 500 steps
//
// This branch grew PartyMon by a held-item byte (stride 48 -> 52) and reuses the
// pet checkpoint's always-zero pad byte for the live creature's held item, so
// both migrations are exercised here for real.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "box_store.h"
#include "inventory.h"
#include "items.h"
#include "save.h"
#include "tasks.h"
#include "dex_rewards.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
uint32_t g_seed=7; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char *s){printf("%s  %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}

static std::string fixture(const char *name) {
  std::string f = __FILE__;
  return f.substr(0, f.rfind("/tools/emu/tests/")) + "/tools/fixtures/" + name;
}

// The emulator's own on-disk NVS format (main_sdl.cpp nvsSave/nvsLoad).
static bool loadNvs(const std::string &path) {
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) return false;
  nvs().clear();
  uint32_t n = 0;
  bool ok = fread(&n, 4, 1, f) == 1;
  for (uint32_t i = 0; ok && i < n; i++) {
    uint32_t kl = 0, vl = 0;
    ok = fread(&kl, 4, 1, f) == 1 && kl <= 64;
    std::string k(kl, 0);
    ok = ok && fread(&k[0], 1, kl, f) == kl && fread(&vl, 4, 1, f) == 1;
    std::vector<uint8_t> v(vl);
    ok = ok && (!vl || fread(v.data(), 1, vl, f) == vl);
    if (ok) nvs()[k] = v;
  }
  fclose(f);
  return ok && n > 0;
}

static bool loadExport(const std::string &path, std::vector<uint8_t> &out) {
  FILE *f = fopen(path.c_str(), "r");
  if (!f) return false;
  char line[512];
  while (fgets(line, sizeof line, f)) {
    if (strncmp(line, "IMPORT ", 7)) continue;
    for (char *p = line + 7; p[0] && p[1] && p[0] != '\n'; p += 2) {
      unsigned v; if (sscanf(p, "%2x", &v) != 1) { fclose(f); return false; }
      out.push_back((uint8_t)v);
    }
  }
  fclose(f);
  return !out.empty();
}

static const int16_t PARTY_DEX[PARTY_SLOTS] = { 25, 150, 133, 1, 4, 7 };

// Everything v3.28 wrote, read back by THIS branch's code.
static void checkAll(const char *how) {
  printf("--- %s\n", how);
  Pet p; p.begin();
  party.begin(); bag.begin();
  char m[160];
#define CK(c, s) do { snprintf(m, sizeof m, "%s: %s", how, s); ck(c, m); } while (0)
  CK(p.speciesId == 6 && p.level() == 30 && !strcmp(p.nick, "BLAZE"), "the live CHARIZARD 'BLAZE' Lv30 is still live");
  CK(p.ivAtk == 31 && p.ivDef == 20 && p.ivSpe == 25 && p.ivHp == 28, "with its IVs 31/20/25/28");
  CK(p.trAtk == 40 && p.trDef == 30 && p.trSpe == 20, "and its training 40/30/20");
  CK(p.held == ITEM_NONE, "and holds nothing (the reused pad byte reads as empty)");
  // v3.28 banked SIX. The creature you raise is party slot 1 now, so five stay
  // in the party and the sixth moves to the box -- kept, never dropped.
  auto memberOk = [](const PartyMon &s, int i) {
    return s.dex == PARTY_DEX[i] && s.level == 50 && s.ivAtk == 20 &&
           s.ivHp == 20 && s.trAtk == 50 && s.trSpe == 50 && s.held == ITEM_NONE;
  };
  bool partyOk = party.count() == PARTY_BANKED && party.slots[PARTY_SLOTS - 1].empty();
  for (int i = 0; i < PARTY_BANKED; i++) partyOk = partyOk && memberOk(party.slots[i], i);
  bool sixthBoxed = false;
  for (uint16_t i = 0; i < BOX_SLOTS; i++) if (memberOk(party.box[i], PARTY_SLOTS - 1)) sixthBoxed = true;
  CK(partyOk && sixthBoxed, "all six party members, Lv50 with IV 20 / tr 50, holding nothing: five in order, the sixth moved to the box");
  const PartyMon &b = party.box[0];
  CK(party.boxCount() == 2 && b.dex == 143 && b.level == 12 && !strcmp(b.nick, "SNOOZE"),
     "the boxed SNORLAX 'SNOOZE' Lv12 is still in the box");
  CK(b.ivAtk == 17 && b.ivDef == 25 && b.ivSpe == 24 && b.ivHp == 9 && b.move(0) == 1,
     "with its IVs and moveset");
  CK(b.stateVersion == 1 && b.fullness == 80 && b.hygiene == 100 && b.ageMinutes == 660 &&
     b.held == ITEM_NONE, "and its care state and age, holding nothing");
  CK(bag.count(IT_POKEBALL) == 17 && bag.count(IT_POTION) == 10 && bag.count(IT_PROTEIN) == 2,
     "the bag: 17 POKE BALLS, 10 POTIONS, 2 PROTEIN");
  CK(p.wallet == 500 && p.stepsTotal == 500, "the wallet and step count");
}

int main() {
  // --- 1. install WITHOUT erase: the store v3.28 left on the board
  ck(loadNvs(fixture("v328.nvs")), "the v3.28 NVS fixture loads");
  ck(nvs().count("box") && nvs()["box"].size() == 48u * BOX_LEGACY_SLOTS,
     "it really is the old 48-byte record (so the migration is exercised)");
  checkAll("first boot on v3.28 storage");

  // The new modules come up on a store that has never heard of them.
  {
    Pet p; p.begin(); bag.begin();
    dexRewardsReset(); tasksForget();
    tasksBegin(p); dexRewardsBegin(&p);
    ck(dexGiftsGranted() == p.registeredCount() / DEX_GIFT_EVERY,
       "dex rewards catch up without inventing anything");
    tasksForget(); dexRewardsReset();
  }

  // Write everything back in the NEW layout, then read it again.
  {
    Pet p; p.begin(); party.begin(); bag.begin();
    ck(p.saveNow() && party.save() && party.boxSave(), "the upgraded save writes back");
    ck(!nvs().count("box"), "the old box key is gone: its creatures moved into the PC box file");
  }
  checkAll("second boot, after writing the new layout");

  // --- 2. restore from a v3.28 BACKUP onto a wiped board
  std::vector<uint8_t> blob;
  ck(loadExport(fixture("v328.tpsave"), blob), "the v3.28 EXPORT fixture parses");
  nvs().clear();
  ck(saveImport(blob.data(), blob.size()), "this branch accepts the v3.28 backup");
  checkAll("restored from a v3.28 backup");

  // And this branch's own export of it carries every key v3.28 had.
  {
    std::vector<uint8_t> out(SAVE_TRANSFER_MAX);
    size_t n = saveExport(out.data(), out.size());
    // Not byte-for-byte comparable any more: v3.28 sent all 18 box slots,
    // empties included, and the PC box sends occupied slots only. What matters
    // is that it carries the same save -- so restore it and check it all again.
    ck(n > 0, "this branch exports it");
    out.resize(n);
    nvs().clear(); boxStoreWipe();
    ck(saveImport(out.data(), out.size()), "and takes its own export back");
  }
  checkAll("restored from this branch's re-export");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
