// Personalities' boon and bane (v3.33): +10% to one battle stat, -10% to
// another, never HP. THE rule is personaStat(); this proves the live pet's
// accessors and a stored creature's both apply it identically, that a battle
// fights with it, and that a creature with no trait is neutral.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "persona.h"
#include "battle.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 77; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static uint16_t statOf(const Pet &p, uint8_t s) {
  switch (s) { case PS_ATK: return p.atkStat(); case PS_DEF: return p.defStat(); case PS_SPA: return p.spaStat();
               case PS_SPD: return p.spdStat(); default: return p.speStat(); }
}
static uint16_t statOf(const PartyMon &m, uint8_t s) {
  switch (s) { case PS_ATK: return party.atkOf(m); case PS_DEF: return party.defOf(m); case PS_SPA: return party.spaOf(m);
               case PS_SPD: return party.spdOf(m); default: return party.speOf(m); }
}

int main() {
  // The games' table, written out rather than derived, so the n/5, n%5 rule in
  // persona.cpp is checked against the real natures and not against itself.
  struct { uint8_t nat, boon, bane; const char *name; } REAL[] = {
    { NAT_LONELY, PS_ATK, PS_DEF, "LONELY" }, { NAT_BRAVE, PS_ATK, PS_SPE, "BRAVE" },
    { NAT_ADAMANT, PS_ATK, PS_SPA, "ADAMANT" }, { NAT_NAUGHTY, PS_ATK, PS_SPD, "NAUGHTY" },
    { NAT_BOLD, PS_DEF, PS_ATK, "BOLD" }, { NAT_RELAXED, PS_DEF, PS_SPE, "RELAXED" },
    { NAT_IMPISH, PS_DEF, PS_SPA, "IMPISH" }, { NAT_LAX, PS_DEF, PS_SPD, "LAX" },
    { NAT_TIMID, PS_SPE, PS_ATK, "TIMID" }, { NAT_HASTY, PS_SPE, PS_DEF, "HASTY" },
    { NAT_JOLLY, PS_SPE, PS_SPA, "JOLLY" }, { NAT_NAIVE, PS_SPE, PS_SPD, "NAIVE" },
    { NAT_MODEST, PS_SPA, PS_ATK, "MODEST" }, { NAT_MILD, PS_SPA, PS_DEF, "MILD" },
    { NAT_QUIET, PS_SPA, PS_SPE, "QUIET" }, { NAT_RASH, PS_SPA, PS_SPD, "RASH" },
    { NAT_CALM, PS_SPD, PS_ATK, "CALM" }, { NAT_GENTLE, PS_SPD, PS_DEF, "GENTLE" },
    { NAT_SASSY, PS_SPD, PS_SPE, "SASSY" }, { NAT_CAREFUL, PS_SPD, PS_SPA, "CAREFUL" },
    { NAT_HARDY, PS_NONE, PS_NONE, "HARDY" }, { NAT_DOCILE, PS_NONE, PS_NONE, "DOCILE" },
    { NAT_SERIOUS, PS_NONE, PS_NONE, "SERIOUS" }, { NAT_BASHFUL, PS_NONE, PS_NONE, "BASHFUL" },
    { NAT_QUIRKY, PS_NONE, PS_NONE, "QUIRKY" },
  };
  bool real = true;
  for (auto &r : REAL) {
    uint8_t c = personaCode(r.nat);
    if (personaBoon(c) != r.boon || personaBane(c) != r.bane || strcmp(personaName(c), r.name)) {
      real = false; printf("      %s: boon %u bane %u name %s\n", r.name, personaBoon(c), personaBane(c), personaName(c));
    }
  }
  ck(real, "all 25 natures: the real names and the real raised/lowered stats");
  // the legacy seven read as the nature with exactly their old boon and bane
  const uint8_t OLD[8][2] = { {0,0}, {PS_DEF,PS_SPE}, {PS_SPD,PS_SPE}, {PS_SPE,PS_DEF}, {PS_SPA,PS_ATK},
                              {PS_DEF,PS_SPA}, {PS_ATK,PS_SPD}, {PS_SPA,PS_DEF} };
  bool legacy = true;
  for (uint8_t t = 1; t < PT_NATURE0; t++)
    if (personaBoon(t) != OLD[t][0] || personaBane(t) != OLD[t][1] || !personaValid(t)) legacy = false;
  ck(legacy, "a creature saved with an old trait keeps exactly its old stats");
  ck(!strcmp(personaName(PT_GLUTTON), "RELAXED") && !strcmp(personaName(PT_ENERGETIC), "NAUGHTY"),
     "and is shown as that nature (GLUTTON -> RELAXED, ENERGETIC -> NAUGHTY)");
  ck(personaLike(PT_GLUTTON) == PL_FOOD && personaLike(PT_CUDDLY) == PL_CUDDLE && personaLike(PT_PICKY) == PL_BERRY,
     "and still loves what it loved");
  ck(!personaValid(PT_END) && !personaValid(255), "codes past the last nature are no nature");
  ck(personaBoon(PT_NONE) == PS_NONE && personaStat(PT_NONE, PS_ATK, 200) == 200, "no personality: neutral");

  Pet p; p.dbgHatchAs(6, false); p.ageMinutes = 49UL * MINUTES_PER_LEVEL;
  p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 20; p.trAtk = p.trDef = p.trSpe = 30;
  p.persona = PT_NONE;
  uint16_t neutral[PS_COUNT], hp0 = p.vitStat();
  for (uint8_t s = 0; s < PS_COUNT; s++) neutral[s] = statOf(p, s);
  bool petOk = true, partyOk = true, hpOk = true;
  for (uint8_t t = 1; t < PT_END; t++) {
    p.persona = t;
    PartyMon m = p.toPartyMon();
    for (uint8_t s = 0; s < PS_COUNT; s++) {
      uint16_t want = s == personaBoon(t) ? neutral[s] * 110 / 100 : s == personaBane(t) ? neutral[s] * 90 / 100 : neutral[s];
      if (statOf(p, s) != want) { petOk = false; printf("      trait %u stat %u: pet %u want %u\n", t, s, statOf(p, s), want); }
      if (statOf(m, s) != want) partyOk = false;
    }
    if (p.vitStat() != hp0 || party.vitOf(m) != hp0) hpOk = false;
  }
  ck(petOk, "the live pet: +10% on the boon, -10% on the bane, the rest untouched");
  ck(partyOk, "a stored creature: exactly the same numbers");
  ck(hpOk, "HP is never touched");
  // a battle fights with it
  p.persona = PT_ENERGETIC;
  Combatant c; combatantFromPet(c, p);
  ck(c.base[SI_ATK] == neutral[PS_ATK] * 110 / 100 && c.base[SI_SPD] == neutral[PS_SPD] * 90 / 100,
     "an ENERGETIC creature fights with +ATK and -SP.D");
  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
