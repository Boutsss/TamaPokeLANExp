// The six STATUS moves (v3.33), against the real battle engine and the real
// learnsets:
//  - each inflicts its ailment when it hits, and the type rules hold: a Ground
//    type cannot be paralysed by THUNDER WAVE, a Steel type cannot be
//    poisoned, a Fire type cannot be burned, nothing gets a second status,
//    and CONFUSE RAY stacks on top of one;
//  - asleep, a creature loses its turns, then wakes;
//  - accuracy is the games' (HYPNOSIS lands about 60% of the time);
//  - the hard AI uses one when it can take hold, and never on a foe that
//    already has a status;
//  - who can learn them comes from PokeAPI, gated at TM_LEVEL like every TM;
//  - and the move table only GREW: every older move kept its index.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "dex.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 4242; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
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
// Use mv until it lands (status moves can miss): the outcome of a hit.
static TurnLog landed(int16_t atkDex, int16_t defDex, uint16_t mv, Combatant *out) {
  TurnLog log;
  for (int k = 0; k < 50; k++) {
    Combatant a, d; mk(a, atkDex, 50); mk(d, defDex, 50);
    battleAct(a, d, mv, log);
    if (out) *out = d;
    if (!log.missed || log.immune) return log;
  }
  return log;   // never took hold: *out is a creature it was tried on
}
// When can this species use mv? 255 = never. moveUnlockLevel() is THE answer
// the picker (learnableFor) and relearnFromLevel() both ask.
static uint8_t unlockAt(int16_t dex, uint16_t mv) {
  uint8_t best = 255;
  for (uint16_t i = 0; i < learnCount(dex); i++)
    if (learnMove(dex, i) == mv) { uint8_t u = moveUnlockLevel(dex, i); if (u < best) best = u; }
  return best;
}

int main() {
  // ---- the table only grew
  ck(MV_DARK_PULSE == 89 && MV_STRUGGLE < MV_DARK_PULSE, "every older move kept its index (DARK PULSE is still 89)");
  ck(MV_THUNDER_WAVE == 90 && MV_CONFUSE_RAY == 95 && MOVE_COUNT >= 96, "the six status moves are 90..95, after every older move");

  Combatant d;
  TurnLog l = landed(25, 19, MV_THUNDER_WAVE, &d);
  ck(d.ailment == AIL_PARA && l.inflicted == AIL_PARA, "THUNDER WAVE paralyses (RATTATA)");
  l = landed(25, 50, MV_THUNDER_WAVE, &d);
  ck(l.immune && d.ailment == AIL_NONE, "but not a Ground type (DIGLETT): immune");
  l = landed(25, 135, MV_THUNDER_WAVE, &d);
  ck(d.ailment == AIL_NONE, "nor an Electric type (JOLTEON)");
  l = landed(1, 19, MV_SLEEP_POWDER, &d);
  ck(d.ailment == AIL_SLEEP && d.ailTurns >= 2 && d.ailTurns <= 4, "SLEEP POWDER sends it to sleep for 2-4 turns");
  l = landed(110, 19, MV_TOXIC, &d);
  ck(d.ailment == AIL_POISON, "TOXIC poisons");
  l = landed(110, 81, MV_TOXIC, &d);
  ck(d.ailment == AIL_NONE, "but not a Steel type (MAGNEMITE)");
  l = landed(92, 19, MV_CONFUSE_RAY, &d);
  ck(d.confuseTurns > 0, "CONFUSE RAY works on a Normal type: a status move ignores the type chart");
  l = landed(6, 19, MV_WILL_O_WISP, &d);
  ck(d.ailment == AIL_BURN, "WILL-O-WISP burns");
  l = landed(6, 4, MV_WILL_O_WISP, &d);
  ck(d.ailment == AIL_NONE, "but not a Fire type (CHARMANDER)");
  // no second status; confusion stacks
  {
    Combatant a, t; mk(a, 92, 50); mk(t, 19, 50);
    t.ailment = AIL_BURN;
    TurnLog lg;
    int tries = 0;
    do { battleAct(a, t, MV_HYPNOSIS, lg); } while (lg.missed && ++tries < 40 && t.ailment == AIL_BURN && lg.inflicted == AIL_NONE);
    ck(t.ailment == AIL_BURN, "a burned creature cannot also be put to sleep");
    for (int k = 0; k < 10 && !t.confuseTurns; k++) battleAct(a, t, MV_CONFUSE_RAY, lg);
    ck(t.confuseTurns > 0 && t.ailment == AIL_BURN, "CONFUSE RAY confuses on top of a burn");
  }
  // asleep: turns lost, then awake
  {
    Combatant a, t; mk(a, 19, 50); mk(t, 19, 50);
    a.ailment = AIL_SLEEP; a.ailTurns = 3;   // wakes, and acts, on the turn the count runs out
    uint16_t hp0 = t.hp;
    TurnLog lg;
    battleAct(a, t, MV_TACKLE, lg); battleAct(a, t, MV_TACKLE, lg);
    ck(t.hp == hp0 && a.ailment == AIL_SLEEP, "asleep, it loses its turns");
    for (int k = 0; k < 6 && t.hp == hp0; k++) battleAct(a, t, MV_TACKLE, lg);
    ck(t.hp < hp0 && a.ailment == AIL_NONE, "then wakes and acts");
  }
  // accuracy
  {
    int hits = 0;
    for (int k = 0; k < 1000; k++) {
      Combatant a, t; mk(a, 92, 50); mk(t, 19, 50);
      TurnLog lg; battleAct(a, t, MV_HYPNOSIS, lg);
      if (t.ailment == AIL_SLEEP) hits++;
    }
    printf("      HYPNOSIS landed %d of 1000\n", hits);
    ck(hits > 540 && hits < 660, "HYPNOSIS lands about 60% of the time");
  }
  // the hard AI
  {
    Combatant a, t; mk(a, 25, 50); mk(t, 143, 50);   // PIKACHU vs SNORLAX: plenty of HP
    a.moves[3] = MV_THUNDER_WAVE;
    int used = 0;
    for (int k = 0; k < 50; k++) if (aiChooseMove(a, t, true) == MV_THUNDER_WAVE) used++;
    ck(used > 0, "the hard AI uses THUNDER WAVE on a fresh, bulky foe");
    t.ailment = AIL_PARA;
    used = 0;
    for (int k = 0; k < 50; k++) if (aiChooseMove(a, t, true) == MV_THUNDER_WAVE) used++;
    ck(used == 0, "and never on a foe that already has a status");
    Combatant g; mk(g, 76, 50);                       // GOLEM: Ground, immune
    used = 0;
    for (int k = 0; k < 50; k++) if (aiChooseMove(a, g, true) == MV_THUNDER_WAVE) used++;
    ck(used == 0, "nor on a Ground type it cannot touch");
  }
  // who can learn them, and when
  ck(unlockAt(25, MV_THUNDER_WAVE) == 40, "PIKACHU can learn THUNDER WAVE, from level 40 (a TM, gated like every TM)");
  ck(unlockAt(1, MV_SLEEP_POWDER) != 255, "BULBASAUR can learn SLEEP POWDER");
  ck(unlockAt(94, MV_HYPNOSIS) != 255 && unlockAt(94, MV_CONFUSE_RAY) != 255 && unlockAt(94, MV_WILL_O_WISP) != 255,
     "GENGAR: HYPNOSIS, CONFUSE RAY, WILL-O-WISP");
  ck(unlockAt(129, MV_TOXIC) == 255 && unlockAt(129, MV_THUNDER_WAVE) == 255, "MAGIKARP learns none of them");
  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
