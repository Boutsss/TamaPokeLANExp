// Move IDs went 8 -> 16 bits in v3.37. Does a save made by v3.36 keep EVERY
// moveset -- the live pet, the party, the PC box and the saved LAN rivals?
//
// The fixtures were written by the v3.36 sources themselves (commit 9a8b9f3),
// not by this test: tools/fixtures/gen_v336.cpp, compiled against that
// checkout, raised real creatures through its own Pet/Party/ghost code and
// wrote its NVS file, its box file and its EXPORT. So this checks the new
// readers against old firmware's bytes, not against this file's idea of them
// (CLAUDE.md "A test that proves the transcription rather than the firmware").
//
//   v336.nvs / v336-box.bin   what a board holds after an install WITHOUT erase
//   v336.tpsave               that save's EXPORT (old "pcNN" box fields)
//
// The movesets below are what the generator printed from the v3.36 build.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "ghost.h"
#include "box_store.h"
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
static bool copyFile(const std::string &from, const char *to) {
  FILE *a = fopen(from.c_str(), "rb"); if (!a) return false;
  FILE *b = fopen(to, "wb"); if (!b) { fclose(a); return false; }
  char buf[4096]; size_t n; while ((n = fread(buf, 1, sizeof buf, a)) > 0) fwrite(buf, 1, n, b);
  fclose(a); fclose(b); return true;
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

struct Want { int16_t dex; uint16_t mv[4]; };
static const Want LIVE = { 6, { 13, 45, 119, 133 } };
static const Want PARTY_W[3] = { { 25, { 24, 23, 183, 101 } }, { 149, { 239, 160, 177, 143 } }, { 94, { 139, 64, 115, 89 } } };
static const uint16_t BOX_AT[3] = { 0, 29, 1199 };
static const Want BOX_W[3] = { { 9, { 121, 160, 163, 103 } }, { 143, { 9, 8, 7, 192 } }, { 248, { 108, 69, 89, 60 } } };

static bool same(const PartyMon &m, const Want &w) {
  if (m.dex != w.dex) return false;
  for (int i = 0; i < 4; i++) if (m.move(i) != w.mv[i]) return false;
  return true;
}
static bool sameL(const LinkMon &m, const Want &w) {
  if (m.dex != w.dex) return false;
  for (int i = 0; i < 4; i++) if (m.move(i) != w.mv[i]) return false;
  return true;
}

static void checkAll(const char *how) {
  printf("--- %s\n", how);
  char s[160];
#define CK(c, t) do { snprintf(s, sizeof s, "%s: %s", how, t); ck(c, s); } while (0)
  Pet p; p.begin();
  bool live = p.speciesId == LIVE.dex;
  for (int i = 0; i < 4; i++) live = live && p.moves[i] == LIVE.mv[i];
  CK(live, "the live CHARIZARD keeps its four moves");
  party.begin();
  bool pt = true;
  for (int i = 0; i < 3; i++) pt = pt && same(party.slots[i], PARTY_W[i]);
  CK(pt, "every party member keeps its moves");
  bool bx = true;
  for (int i = 0; i < 3; i++) bx = bx && same(party.box[BOX_AT[i]], BOX_W[i]);
  CK(bx, "every PC box creature keeps its moves, slot 1200 included");
  ghostsBegin();
  const Ghost &g = ghostAt(0);
  CK(ghostCount() == 1 && !strcmp(g.name, "RIVAL") && g.n == 2 && sameL(g.mon[0], PARTY_W[0]) && sameL(g.mon[1], PARTY_W[1]),
     "the saved LAN rival's team keeps both creatures and their moves (the second is where a misread would show)");
  CK(g.mon[1].level == 50 && g.mon[1].maxHp > 100 && g.mon[1].base[0] > 100 && !strcmp(g.mon[1].name, "P"),
     "and the second creature's other fields are not shifted (level 50, its stats, its name)");
}

int main() {
  // ---- an install WITHOUT erase: the old store, read in place
  ck(loadNvs(fixture("v336.nvs")) && copyFile(fixture("v336-box.bin"), "tamapoke-box.bin"), "the v3.36 fixtures load");
  remove("tamapoke-box.tmp");
  checkAll("upgraded in place");
  // ...and once this build has saved everything back in its own format
  { Pet p; p.begin(); p.saveNow(); party.begin(); party.save(); party.boxSave(); }
  checkAll("after this build re-saves");

  // ---- the web installer's backup, restored on a wiped board
  std::vector<uint8_t> blob;
  ck(loadExport(fixture("v336.tpsave"), blob), "the v3.36 EXPORT reads");
  nvs().clear(); remove("tamapoke-box.bin"); remove("tamapoke-box.tmp");
  party.begin();
  ck(saveImport(blob.data(), blob.size()), "this build IMPORTs a v3.36 backup (old pcNN box fields)");
  checkAll("restored from the backup");
  // and our own export of it round-trips
  static uint8_t buf[SAVE_TRANSFER_MAX];
  size_t n = saveExport(buf, sizeof(buf));
  std::vector<uint8_t> mine(buf, buf + n);
  nvs().clear(); remove("tamapoke-box.bin"); party.begin();
  ck(n > 0 && saveImport(mine.data(), mine.size()), "and its own re-EXPORT (pmNN fields) imports again");
  checkAll("re-exported and restored");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
