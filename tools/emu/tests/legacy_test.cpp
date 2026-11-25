// Since v3.50 the save is ONLY the two checkpoint records (petA/petB for the
// creature, plyA/plyB for the player); the ~65 legacy keys that mirrored them
// are no longer written, and any left from an older build are purged once a
// checkpointed save has landed.
//
// That is safe only if every one of those keys was a PURE mirror -- a field
// held only by a legacy key would now be lost on every save. So this test sets
// EVERY field the legacy keys carried to a distinct, non-default value, saves,
// proves no legacy key is on flash, reloads into a fresh Pet and compares them
// all. Then it migrates a pre-checkpoint save (legacy keys only), and checks
// that a backup taken now carries everything and restores it.
#include <cstdio>
#include <cstring>
#include <vector>
#include <map>
#include <string>
#include "Arduino.h"
#include "Preferences.h"
// The comparison has to reach EVERY field, private ones included -- a field the
// test cannot see is exactly the one that could be lost unnoticed.
#define private public
#include "pet.h"
#undef private
#include "party.h"
#include "save.h"
#include "skins.h"
uint32_t g_seed=101; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

// Every field a legacy key used to carry, set to a value no default produces.
static void fill(Pet &p) {
  p.dbgHatchAs(150, false);
  p.fullness = 41; p.joy = 42; p.energy = 43; p.hygiene = 44; p.poops = 2; p.weight = 17;
  p.ivAtk = 21; p.ivDef = 22; p.ivSpe = 23; p.ivHp = 24;
  p.trAtk = 31; p.trDef = 32; p.trSpe = 33;
  p.moves[0] = 300; p.moves[1] = 12; p.moves[2] = 77; p.moves[3] = 401;
  p.avatar = 77; p.region = 3;
  for (int i = 0; i < (int)sizeof(p.badgesX) / (int)sizeof(p.badgesX[0]); i++) { p.badgesX[i] = (uint16_t)(0x11 + i); p.badgesHardX[i] = (uint16_t)(0x21 + i); }
  for (int i = 0; i < REGION_COUNT; i++) p.eggByRegion[i] = (uint16_t)(5 + i);
  p.frozen = true; p.badges = 0x0055; p.badgesHard = 0x0003;
  p.berryKnown = true; p.shiny = true; p.eggShiny = true;
  p.persona = 7; p.skin = SKIN_ARMORED; p.starterPick = false;
  p.evoPen = 2; p.sleepAuto = 1; p.retirePending = true;
  for (size_t i = 0; i < sizeof(p.dexShinyReg); i++) p.dexShinyReg[i] = (uint8_t)(0x5A ^ i);
  for (size_t i = 0; i < sizeof(p.dexReg); i++) p.dexReg[i] = (uint8_t)(0xA5 ^ i);
  p.ageMinutes = 12345; p.eggTarget = 133; p.eggTaps = 1; p.careMistakes = 3;
  p.sleeping = true; p.lastEnd = 2; p.lastSeenEpoch = 1700000000UL;
  p.streak = 12; p.bestStreak = 40; p.lastCareDay = 20000;
  p.bond = 55; p.medals = 6; p.totalMedals = 66; p.lastMilestone = 30;
  p.gameHi = 1111; p.strHi = 2222; p.spdHi = 3333;
  p.wallet = 654321; p.stepsTotal = 9876543; p.coins = 1234567;
  snprintf(p.trainerName, sizeof(p.trainerName), "ASH");
  snprintf(p.nick, sizeof(p.nick), "SHADOW");
  // Two things a LOAD legitimately does, set up front so they are not mistaken
  // for loss: the live creature is registered (shiny too) in the Pokedex, and
  // level-ups missed since lastLearnLevel are caught up.
  p.registerSpecies(p.speciesId);
  p.dexShinyReg[(p.speciesId - 1) >> 3] |= (uint8_t)(1 << ((p.speciesId - 1) & 7));
  p.lastLearnLevel = p.level();
}

static int same(const Pet &a, const Pet &b) {
  int diff = 0;
#define F(x) if (!(a.x == b.x)) { printf("      differs: %s\n", #x); diff++; }
#define A(x) if (memcmp(a.x, b.x, sizeof(a.x))) { printf("      differs: %s\n", #x); diff++; }
  F(fullness) F(joy) F(energy) F(hygiene) F(poops) F(weight)
  F(ivAtk) F(ivDef) F(ivSpe) F(ivHp) F(trAtk) F(trDef) F(trSpe)
  A(moves) F(lastLearnLevel) F(avatar) F(region) A(badgesX) A(badgesHardX) A(eggByRegion)
  F(frozen) F(badges) F(badgesHard) F(berryKnown) F(shiny) F(eggShiny) F(persona) F(skin)
  F(starterPick) F(evoPen) F(sleepAuto) F(retirePending) A(dexShinyReg) A(dexReg)
  F(ageMinutes) F(speciesId) F(eggTarget) F(eggTaps) F(careMistakes) F(sleeping) F(lastEnd)
  F(lastSeenEpoch) F(streak) F(bestStreak) F(lastCareDay) F(bond) F(medals) F(totalMedals)
  F(lastMilestone) F(gameHi) F(strHi) F(spdHi) F(wallet) F(stepsTotal) F(coins)
  A(trainerName) A(nick)
#undef F
#undef A
  return diff;
}

int main(){
  // ---- every field, through the checkpoints alone
  {
    nvs().clear();
    Pet p; p.begin(); party.begin();
    if (p.awaitingStarter()) p.chooseStarter(4);
    fill(p);
    // Keys that belong to OTHER systems, as a real save has them. A purge that
    // took one of these would delete real data, not a mirror.
    nvs()["rivals"] = { 1, 2, 3 }; nvs()["casino"] = { 1, 4, 0, 0 }; nvs()["lang"] = { 1 };
    nvs()["party"] = std::vector<uint8_t>(8, 9); nvs()["bag"] = { 5, 5 };
    ck(p.saveNow(), "a save with every field set lands");
    ck(nvs().count("rivals") && nvs().count("casino") && nvs().count("lang") && nvs().count("party") && nvs().count("bag"),
       "the purge leaves every key that is not a legacy mirror alone");
    ck(p.legacyKeyCount() == 0, "and no legacy key is on flash");
    bool onlyCkpt = !nvs().count("full") && !nvs().count("dexreg") && !nvs().count("tnam") && !nvs().count("nick");
    ck(onlyCkpt, "(spot check: full, dexreg, tnam, nick are all absent)");
    Pet q; q.begin();
    ck(same(p, q) == 0, "a fresh reload gets back EVERY field the legacy keys used to carry");
  }

  // ---- a save from before the checkpoints migrates, then loses its legacy keys
  {
    nvs().clear();
    nvs()["init"] = { 1 };
    nvs()["dexn"] = { 25, 0 };                 // PIKACHU
    nvs()["age"] = { 0x10, 0x27, 0, 0 };       // 10000 minutes
    nvs()["ivat"] = { 29 };
    nvs()["badg"] = { 0x07, 0 };
    nvs()["coin"] = { 0x39, 0x05 };            // 1337
    const char tn[] = "GARY"; nvs()["tnam"] = std::vector<uint8_t>(tn, tn + sizeof(tn));
    Pet p; p.begin(); party.begin();
    ck(p.speciesId == 25 && p.ageMinutes == 10000 && p.ivAtk == 29 && p.badges == 0x07 &&
       p.coins == 1337 && !strcmp(p.trainerName, "GARY"),
       "a pre-checkpoint save (legacy keys only) still loads");
    ck(p.legacyKeyCount() > 0, "with its legacy keys still there until a save");
    ck(p.saveNow() && p.legacyKeyCount() == 0, "the first save writes the checkpoints and purges them");
    Pet q; q.begin();
    ck(q.speciesId == 25 && q.ageMinutes == 10000 && q.ivAtk == 29 && q.badges == 0x07 &&
       q.coins == 1337 && !strcmp(q.trainerName, "GARY"),
       "and everything they held is in the checkpoints now");
  }

  // ---- a purge never runs ahead of the records it relies on
  {
    nvs().clear();
    nvs()["init"] = { 1 };
    nvs()["dexn"] = { 7, 0 };
    nvs()["ivat"] = { 11 }; nvs()["ivdf"] = { 12 }; nvs()["ivsp"] = { 13 }; nvs()["ivhp"] = { 30 };
    Pet p; p.begin(); party.begin();
    nvsFailWritesAfter(0);               // the checkpoint writes fail
    bool ok = p.saveNow();
    nvsResumeWrites();
    ck(!ok && p.legacyKeyCount() > 0, "a save whose checkpoints did not land leaves the legacy keys alone");
    Pet q; q.begin();
    ck(q.speciesId == 7 && q.ivHp == 30, "so the creature is still there to load");
  }

  // ---- the backup carries it all
  {
    nvs().clear();
    Pet p; p.begin(); party.begin();
    if (p.awaitingStarter()) p.chooseStarter(4);
    fill(p);
    p.saveNow();
    std::vector<uint8_t> buf(SAVE_TRANSFER_MAX);
    size_t n = saveExport(buf.data(), buf.size());
    ck(n > 0, "EXPORT works with no legacy keys");
    nvs().clear();
    ck(saveImport(buf.data(), n), "and IMPORT takes it back");
    Pet q; q.begin();
    ck(same(p, q) == 0, "restoring a backup brings back every field");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
