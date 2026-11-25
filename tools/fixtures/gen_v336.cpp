// Builds a save with the v3.36 sources (the real old Party/Pet/ghost/save code)
// whose live pet, party, PC box and ghosts all hold real movesets, and writes
// the emulator NVS file, the box file and the EXPORT block.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "battle.h"
#include "ghost.h"
#include "box_store.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=33; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static PartyMon raised(int16_t dex, int lvl, const char *nick) {
  Pet t; t.dbgHatchAs(dex, false); t.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL; t.relearnFromLevel();
  snprintf(t.nick, sizeof(t.nick), "%s", nick);
  return t.toPartyMon();
}
static void show(const char *where, const PartyMon &m) {
  printf("%s dex=%d moves=%u,%u,%u,%u\n", where, m.dex, m.moves[0], m.moves[1], m.moves[2], m.moves[3]);
}
int main() {
  nvs().clear(); remove("tamapoke-box.bin");
  Pet p; p.begin(); p.dbgHatchAs(6, false); p.ageMinutes = 39UL * MINUTES_PER_LEVEL; p.relearnFromLevel();
  snprintf(p.nick, sizeof(p.nick), "BLAZE"); p.saveNow();
  printf("live dex=%d moves=%u,%u,%u,%u\n", p.speciesId, p.moves[0], p.moves[1], p.moves[2], p.moves[3]);
  party.begin();
  const int16_t pd[] = { 25, 149, 94 };
  for (int i = 0; i < 3; i++) { PartyMon m = raised(pd[i], 45 + i * 5, "P"); party.add(m); char w[16]; snprintf(w, sizeof w, "party%d", i); show(w, m); }
  party.save();
  const uint16_t bs[] = { 0, 29, 1199 }; const int16_t bd[] = { 9, 143, 248 };
  for (int i = 0; i < 3; i++) { party.box[bs[i]] = raised(bd[i], 60, "B"); char w[16]; snprintf(w, sizeof w, "box%u", bs[i]); show(w, party.box[bs[i]]); }
  party.boxSave();
  LinkMon lm[2]; Combatant c;
  combatantFromParty(c, party.slots[0]); linkMonFrom(lm[0], c);
  combatantFromParty(c, party.slots[1]); linkMonFrom(lm[1], c);
  uint8_t mac[6] = { 1, 2, 3, 4, 5, 6 };
  ghostsBegin(); ghostRemember(mac, "RIVAL", lm, 2);
  printf("ghost0 moves=%u,%u,%u,%u ghost1 moves=%u,%u,%u,%u\n", lm[0].moves[0], lm[0].moves[1], lm[0].moves[2], lm[0].moves[3],
         lm[1].moves[0], lm[1].moves[1], lm[1].moves[2], lm[1].moves[3]);
  // the emulator's NVS file (main_sdl.cpp nvsSave)
  FILE *f = fopen("v336.nvs", "wb");
  uint32_t n = nvs().size(); fwrite(&n, 4, 1, f);
  for (auto &kv : nvs()) { uint32_t kl = kv.first.size(), vl = kv.second.size();
    fwrite(&kl, 4, 1, f); fwrite(kv.first.data(), 1, kl, f); fwrite(&vl, 4, 1, f); if (vl) fwrite(kv.second.data(), 1, vl, f); }
  fclose(f);
  static uint8_t buf[SAVE_TRANSFER_MAX];
  size_t bn = saveExport(buf, sizeof(buf));
  FILE *t = fopen("v336.tpsave", "w");
  fprintf(t, "# TamaPoke save, %u bytes. Paste this whole block back.\n", (unsigned)bn);
  for (size_t i = 0; i < bn; i += 50) { fprintf(t, "IMPORT "); for (size_t j = i; j < bn && j < i + 50; j++) fprintf(t, "%02X", buf[j]); fprintf(t, "\n"); }
  fclose(t);
  printf("export %u bytes\n", (unsigned)bn);
  return 0;
}
