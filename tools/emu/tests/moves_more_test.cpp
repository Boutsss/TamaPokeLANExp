// The Gen 1-7 moves (v3.33): 159 appended after the hand-picked set, chosen by
// tools/pick_moves.py because the engine can play them as the games do.
//  - the table only grew: every older move kept its index, and it stops at 254
//    so a one-byte move ID is still enough;
//  - each kind of effect does what its row says: a sure stage drop on the foe
//    (ROCK TOMB), a CHANCE of one (ENERGY BALL, about 10%), a drop on the
//    user even when the foe faints (SUPERPOWER), priority (AQUA JET goes
//    first), 2-5 hits, drain, recoil, a heal, a sleep move;
//  - learnsets are real and levelled: CHARMANDER learns METAL CLAW at 13,
//    METAPOD can finally do something (HARDEN), and level gates hold.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "dex.h"
#include <cstdio>
uint32_t g_seed = 999; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static void mk(Combatant &c, int16_t dex, uint8_t lvl) {
  Pet p; p.dbgHatchAs(dex, false); p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 20;
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL; p.relearnFromLevel(); combatantFromPet(c, p);
}
static uint8_t unlockAt(int16_t dex, uint8_t mv) {
  uint8_t best = 255;
  for (uint8_t i = 0; i < learnCount(dex); i++)
    if (learnMove(dex, i) == mv) { uint8_t u = moveUnlockLevel(dex, i); if (u < best) best = u; }
  return best;
}

int main() {
  ck(MV_DARK_PULSE == 89 && MV_CONFUSE_RAY == 95, "every older move kept its index");
  ck(MOVE_COUNT == 255, "the table stops at 254 moves (+ the filler): a byte still holds every ID");
  ck(MV_ROCK_TOMB > MV_CONFUSE_RAY && MV_BULLET_SEED > MV_CONFUSE_RAY, "the new moves come after them");

  // a sure stage drop on the foe
  { Combatant a, d; mk(a, 74, 50); mk(d, 143, 50); TurnLog l;
    int tries = 0; do { battleAct(a, d, MV_ROCK_TOMB, l); } while (l.missed && ++tries < 20);
    ck(d.stage[SI_SPE] == -1, "ROCK TOMB lowers the foe's SPEED"); }
  // a chance of one
  { int n = 0;
    for (int k = 0; k < 2000; k++) { Combatant a, d; mk(a, 1, 50); mk(d, 143, 50); TurnLog l; battleAct(a, d, MV_ENERGY_BALL, l); if (d.stage[SI_SPD] < 0) n++; }
    printf("      ENERGY BALL lowered SP.D in %d of 2000\n", n);
    ck(n > 120 && n < 290, "ENERGY BALL lowers SP.D about 10% of the time"); }
  // a drop on the user, foe fainted or not
  { Combatant a, d; mk(a, 68, 80); mk(d, 19, 5); TurnLog l;
    battleAct(a, d, MV_SUPERPOWER, l);
    ck(d.fainted() && a.stage[SI_ATK] == -1 && a.stage[SI_DEF] == -1, "SUPERPOWER lowers the user's ATK and DEF, even after a KO"); }
  // priority
  { Combatant slow, fast; mk(slow, 7, 50); mk(fast, 101, 50);
    ck(battleMovesFirst(slow, MV_AQUA_JET, fast, MV_TACKLE) && !battleMovesFirst(slow, MV_TACKLE, fast, MV_TACKLE),
       "AQUA JET lets a slow SQUIRTLE move before an ELECTRODE"); }
  // 2-5 hits
  { int lo = 9, hi = 0;
    for (int k = 0; k < 300; k++) { Combatant a, d; mk(a, 1, 50); mk(d, 143, 80); TurnLog l; battleAct(a, d, MV_BULLET_SEED, l);
      if (!l.missed) { if (l.hits < lo) lo = l.hits; if (l.hits > hi) hi = l.hits; } }
    ck(lo == 2 && hi == 5, "BULLET SEED hits 2 to 5 times"); }
  // drain, recoil, heal, sleep
  { Combatant a, d; mk(a, 1, 50); mk(d, 143, 80); a.hp = a.maxHp / 2; uint16_t h0 = a.hp; TurnLog l;
    battleAct(a, d, MV_GIGA_DRAIN, l); ck(a.hp > h0, "GIGA DRAIN heals the user"); }
  { Combatant a, d; mk(a, 19, 50); mk(d, 143, 80); uint16_t h0 = a.hp; TurnLog l;
    for (int k = 0; k < 10 && a.hp == h0; k++) battleAct(a, d, MV_TAKE_DOWN, l);
    ck(a.hp < h0, "TAKE DOWN hurts the user too"); }
  { Combatant a, d; mk(a, 16, 50); mk(d, 19, 50); a.hp = a.maxHp / 4; TurnLog l;
    battleAct(a, d, MV_ROOST, l); ck(a.hp >= a.maxHp / 4 + a.maxHp / 2 - 1, "ROOST heals half"); }
  { int slept = 0;
    for (int k = 0; k < 200; k++) { Combatant a, d; mk(a, 39, 50); mk(d, 19, 50); TurnLog l; battleAct(a, d, MV_SING, l); if (d.ailment == AIL_SLEEP) slept++; }
    ck(slept > 80 && slept < 140, "SING puts the foe to sleep about 55% of the time"); }

  // learnsets
  ck(unlockAt(4, MV_METAL_CLAW) == 13, "CHARMANDER learns METAL CLAW at 13, its real level");
  ck(unlockAt(11, MV_HARDEN) <= 1, "METAPOD finally has a move of its own: HARDEN");
  ck(unlockAt(143, MV_SUPERPOWER) == 65, "SNORLAX learns SUPERPOWER at 65");
  {
    Pet p; p.dbgHatchAs(4, false); p.ageMinutes = 11UL * MINUTES_PER_LEVEL; p.relearnFromLevel();
    bool early = false;
    for (int i = 0; i < MOVE_SLOTS; i++) if (p.moves[i] == MV_METAL_CLAW || p.moves[i] == MV_FLARE_BLITZ) early = true;
    ck(!early, "a level 12 CHARMANDER has neither METAL CLAW (13) nor FLARE BLITZ (40)");
  }
  // every new move has somebody who can learn it
  {
    int nobody = 0;
    for (int mv = MV_CONFUSE_RAY + 1; mv < MOVE_COUNT; mv++) {
      bool any = false;
      for (int16_t d = 1; d <= DEX_COUNT && !any; d++) if (unlockAt(d, (uint8_t)mv) != 255) any = true;
      if (!any) { nobody++; printf("      nobody learns %s\n", MOVE_TBL[mv].name); }
    }
    ck(nobody == 0, "every new move can be learned by somebody");
  }
  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
