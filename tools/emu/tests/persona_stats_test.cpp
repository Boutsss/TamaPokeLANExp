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
  bool shape = true;
  for (uint8_t t = 1; t < PT_COUNT; t++) {
    uint8_t bo = personaBoon(t), ba = personaBane(t);
    if (bo >= PS_COUNT || ba >= PS_COUNT || bo == ba) shape = false;
  }
  ck(shape, "every personality has a boon and a different bane, both battle stats (never HP)");
  ck(personaBoon(PT_NONE) == PS_NONE && personaStat(PT_NONE, PS_ATK, 200) == 200, "no personality: neutral");

  Pet p; p.dbgHatchAs(6, false); p.ageMinutes = 49UL * MINUTES_PER_LEVEL;
  p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 20; p.trAtk = p.trDef = p.trSpe = 30;
  p.persona = PT_NONE;
  uint16_t neutral[PS_COUNT], hp0 = p.vitStat();
  for (uint8_t s = 0; s < PS_COUNT; s++) neutral[s] = statOf(p, s);
  bool petOk = true, partyOk = true, hpOk = true;
  for (uint8_t t = 1; t < PT_COUNT; t++) {
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
