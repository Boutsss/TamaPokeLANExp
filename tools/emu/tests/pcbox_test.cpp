// The 1200-slot PC box, against the real Party, box_store and save.cpp:
//
//  - the old 18-slot NVS box moves into the file at boot, each creature to its
//    own slot, and the old key goes only once the file is confirmed -- also
//    from a blob written before PartyMon grew (a shorter stride);
//  - a FULL box (1200) survives a reload, last slot included;
//  - a power cut mid-write: a cut-short tmp file is ignored, a whole one wins;
//  - a trade stores into box slot 1000 and reports a 16-bit index;
//  - EXPORT carries the box as pcNN fields, IMPORT restores it exactly and
//    REPLACES what was there, a corrupt field is refused before anything is
//    touched, and an OLD backup (the "box" key) is migrated at the next boot;
//  - WIPE empties it, file and RAM.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "box_store.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
uint32_t g_seed=59; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static PartyMon mon(int16_t dex, uint8_t lvl, const char *nick) {
  PartyMon m; m.dex = dex; m.level = lvl; m.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  m.ivAtk = 20; m.ivDef = 21; m.ivSpe = 22; m.ivHp = 23; m.stateVersion = 1;
  snprintf(m.nick, sizeof(m.nick), "%s", nick);
  return m;
}
// CRC-16/CCITT-FALSE, pinned to its published check value below
static uint16_t crc16ccitt(const uint8_t *p, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) { c ^= (uint16_t)p[i] << 8; for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1); }
  return c;
}
static bool same(const PartyMon &a, const PartyMon &b) { return !memcmp(&a, &b, sizeof(PartyMon)); }
static void fresh() { nvs().clear(); remove("tamapoke-box.bin"); remove("tamapoke-box.tmp"); }

int main(){
  ck(crc16ccitt((const uint8_t *)"123456789", 9) == 0x29B1, "the re-signing CRC is CCITT-FALSE");
  // ---- a fresh board
  fresh();
  { Party q; q.begin(); ck(q.box && q.boxCount() == 0, "a fresh board has an empty PC box"); }

  // ---- the old 18-slot box migrates
  fresh();
  {
    PartyMon old[BOX_LEGACY_SLOTS];
    old[0] = mon(1, 10, "BULBA"); old[3] = mon(4, 12, "CHAR"); old[17] = mon(7, 14, "SQUIRT");
    Preferences p; p.begin("tamapoke", false); p.putBytes("box", old, sizeof(old)); p.end();
    Party q; q.begin();
    ck(same(q.box[0], old[0]) && same(q.box[3], old[3]) && same(q.box[17], old[17]) && q.boxCount() == 3,
       "the old box's creatures land in their OWN slots");
    Preferences r; r.begin("tamapoke", true);
    ck(!r.isKey("box"), "and the old key is gone once the file holds them");
    Party again; again.begin();
    ck(same(again.box[17], old[17]) && again.boxCount() == 3, "they come back from the FILE after a reboot");
  }
  // ...from a blob written before PartyMon grew (48-byte records)
  fresh();
  {
    const size_t oldStride = 48;
    std::vector<uint8_t> raw(oldStride * BOX_LEGACY_SLOTS, 0);
    PartyMon m = mon(25, 30, "PIKA");
    memcpy(&raw[5 * oldStride], &m, oldStride);
    Preferences p; p.begin("tamapoke", false); p.putBytes("box", raw.data(), raw.size()); p.end();
    Party q; q.begin();
    ck(q.box[5].dex == 25 && !strcmp(q.box[5].nick, "PIKA") && q.box[5].held == 0, "an older, shorter record migrates by its front, the rest at defaults");
  }
  // ...and a failed file write keeps the old key for next time
  fresh();
  {
    PartyMon old[BOX_LEGACY_SLOTS]; old[2] = mon(150, 70, "MEW2");
    Preferences p; p.begin("tamapoke", false); p.putBytes("box", old, sizeof(old)); p.end();
    // a directory where the tmp file should go makes the write fail
    mkdir("tamapoke-box.tmp", 0755);
    Party q; q.begin();
    Preferences r; r.begin("tamapoke", true);
    ck(r.isKey("box"), "if the file cannot be written, the old box is NOT removed (never in neither place)");
    rmdir("tamapoke-box.tmp");
    Party q2; q2.begin();
    Preferences r2; r2.begin("tamapoke", true);
    ck(q2.box[2].dex == 150 && !r2.isKey("box"), "and the next boot finishes the move");
  }

  // ---- a FULL box
  fresh();
  {
    Party q; q.begin();
    for (uint16_t i = 0; i < BOX_SLOTS; i++) q.box[i] = mon(1 + i % 151, 1 + i % 100, "X");
    ck(q.boxSave(), "a full box of 1200 writes");
    Party r; r.begin();
    ck(r.boxCount() == BOX_SLOTS && same(r.box[BOX_SLOTS - 1], q.box[BOX_SLOTS - 1]) && same(r.box[600], q.box[600]),
       "and all 1200 read back, the last slot included");
    ck(r.boxFirstFree() == -1 && !r.boxAdd(mon(1, 1, "Y")), "a full box refuses another");
  }

  // ---- a power cut mid-write
  fresh();
  {
    Party q; q.begin();
    q.box[10] = mon(9, 40, "OLD"); q.boxSave();
    // the newer copy, cut short: half a tmp file
    FILE *src = fopen("tamapoke-box.bin", "rb"); std::vector<uint8_t> all(64 * 1024);
    size_t n = fread(all.data(), 1, all.size(), src); fclose(src);
    FILE *t = fopen("tamapoke-box.tmp", "wb"); fwrite(all.data(), 1, n / 2, t); fclose(t);
    Party r; r.begin();
    ck(r.box[10].dex == 9 && !strcmp(r.box[10].nick, "OLD"), "a cut-short newer copy is ignored: the old box stands");
    // the newer copy WHOLE, the rename never happened: it wins
    Party w; w.begin(); w.box[11] = mon(3, 50, "NEW"); w.boxSave();
    FILE *nf = fopen("tamapoke-box.bin", "rb"); std::vector<uint8_t> newer(64 * 1024);
    size_t nn = fread(newer.data(), 1, newer.size(), nf); fclose(nf);
    w.box[11] = PartyMon(); boxStoreSave(w.box);      // the older box under the real name
    FILE *tf = fopen("tamapoke-box.tmp", "wb"); fwrite(newer.data(), 1, nn, tf); fclose(tf);
    Party z; z.begin();
    ck(z.box[11].dex == 3, "a whole newer copy left by a cut before the rename wins");
  }

  // ---- a trade into box slot 1000: a 16-bit index
  fresh();
  {
    Party q; q.begin();
    for (int i = 0; i < PARTY_SLOTS; i++) q.replaceAt(i, mon(10, 5, "P"));
    for (uint16_t i = 0; i < 1000; i++) q.box[i] = mon(11, 5, "B");
    q.boxSave();
    int16_t where = -1;
    PartyMon t = mon(65, 60, "ZAM");
    ck(q.storeDurably(t, &where) && where == PARTY_SLOTS + 1000, "a trade stores into box slot 1000 and says so in 16 bits");
    ck(q.takeOutExact(where, t) && q.box[1000].empty(), "and takes it out of exactly that slot");
  }

  // ---- the backup carries the box, and restores it
  fresh();
  {
    Pet pt; pt.begin();
    party.begin();
    party.box[0] = mon(1, 10, "A"); party.box[35] = mon(4, 20, "B"); party.box[1199] = mon(7, 30, "C");
    party.box[1199].fav = PM_FAV;
    party.boxSave();
    static uint8_t buf[SAVE_TRANSFER_MAX];
    size_t n = saveExport(buf, sizeof(buf));
    ck(n > 0, "EXPORT works with a PC box");
    std::vector<uint8_t> keep(buf, buf + n);
    auto hasKey = [&](const char *k) { return std::search(keep.begin(), keep.end(), k, k + strlen(k)) != keep.end(); };
    ck(hasKey("pc00") && hasKey("pc01") && hasKey("pc39") && !hasKey("pc02"), "one pcNN field per box that holds anything");
    party.box[0] = PartyMon(); party.box[500] = mon(99, 99, "JUNK"); party.boxSave();
    ck(saveImport(keep.data(), keep.size()), "IMPORT takes it");
    Party r; r.begin();
    ck(r.box[0].dex == 1 && r.box[35].dex == 4 && r.box[1199].dex == 7 && r.box[1199].isFav(), "the box comes back exactly, favourites included");
    ck(r.box[500].empty() && r.boxCount() == 3, "and REPLACES what was there");
    std::vector<uint8_t> bad1 = keep;
    // corrupt the slot byte of the first pc00 record
    auto it = std::search(bad1.begin(), bad1.end(), "pc00", "pc00" + 4);
    size_t at = (it - bad1.begin()) + 4 + 1 + 2;        // key, kind, length
    bad1[at] = 31;
    // re-sign it, so it is the SLOT check that refuses it, not the checksum
    uint16_t c = crc16ccitt(bad1.data(), bad1.size() - 2);
    bad1[bad1.size() - 2] = (uint8_t)(c & 0xFF); bad1[bad1.size() - 1] = (uint8_t)(c >> 8);
    ck(!saveImport(bad1.data(), bad1.size()), "a box record naming slot 31 of a box of 30 is refused (checksum valid)");
    Party r2; r2.begin();
    ck(r2.boxCount() == 3, "and the box is untouched by the refusal");
  }
  // ---- an OLD backup (the "box" key) migrates at the next boot
  fresh();
  {
    PartyMon old[BOX_LEGACY_SLOTS]; old[4] = mon(133, 25, "EEV");
    Preferences p; p.begin("tamapoke", false); p.putBytes("box", old, sizeof(old)); p.end();
    static uint8_t buf[SAVE_TRANSFER_MAX];
    size_t n = saveExport(buf, sizeof(buf));
    fresh();
    ck(saveImport(buf, n), "an old-format backup imports");
    Party r; r.begin();
    ck(r.box[4].dex == 133, "and its box arrives in the PC box at the next boot");
  }

  // ---- WIPE
  {
    Party q; q.begin(); party.begin();
    Pet pt; pt.begin();
    pt.factoryReset();
    Party r; r.begin();
    ck(r.boxCount() == 0 && party.boxCount() == 0, "WIPE empties the PC box, on flash and in RAM");
  }

  // ---- the size
  fresh();
  {
    party.begin();
    for (uint16_t i = 0; i < BOX_SLOTS; i++) party.box[i] = mon(1 + i % 151, 50, "FULL");
    party.boxSave();
    static uint8_t buf[SAVE_TRANSFER_MAX];
    size_t n = saveExport(buf, sizeof(buf));
    printf("      a backup with a FULL box: %u of %u bytes\n", (unsigned)n, (unsigned)SAVE_TRANSFER_MAX);
    ck(n > 0 && n < SAVE_TRANSFER_MAX * 3 / 4, "a full box fits the backup with a quarter to spare");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
