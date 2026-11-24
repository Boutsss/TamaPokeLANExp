// Friendship evolution: the bond gate, Eevee's day/night branch, and that every
// other species is untouched. Driven through the real Pet::evolve().
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "dex.h"
#include <cstdio>
uint32_t g_seed=41; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char *s){printf("%s  %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}

// A healthy creature of `dex`, well past its evolution level, at `hour`.
static void ready(Pet &p, int16_t dex, uint8_t bond, int hour) {
  p.dbgHatchAs(dex, false);
  p.ageMinutes = 60UL * 60 * 2;          // level 121 -> capped 100: past every threshold
  p.careMistakes = 0;
  p.bond = bond;
  p.lastSeenEpoch = 20000UL * 86400UL + (uint32_t)hour * 3600UL;
}

int main(){
  gRegionArt = 0xFFFF;
  Pet p; p.begin();

  // ---- a friendship species waits on the bond, whatever its level
  ready(p, 42, 0, 12);                    // GOLBAT
  EvoCheck e = p.evoCheck();
  ck(e.block == EVO_BOND && e.friendship && !p.canEvolveNow(), "GOLBAT at bond 0 is held back by the bond");
  p.evolve();
  ck(p.speciesId == 42, "and evolve() refuses it");
  p.bond = FRIEND_BOND - 1;
  ck(p.evoCheck().block == EVO_BOND, "one point short is still short");
  p.bond = FRIEND_BOND;
  ck(p.canEvolveNow(), "at FRIEND_BOND it can evolve");
  p.evolve();
  ck(p.speciesId == 169, "GOLBAT becomes CROBAT");

  // ---- the level still applies to a friendship species
  ready(p, 172, 100, 12);                 // PICHU
  p.ageMinutes = 0;
  ck(p.evoCheck().block == EVO_LEVEL, "a fully bonded newborn PICHU still waits for its level");

  // ---- nothing else changed: a level species ignores the bond entirely
  ready(p, 4, 0, 12);                     // CHARMANDER
  ck(!p.evoCheck().friendship && p.canEvolveNow(), "CHARMANDER at bond 0 evolves as it always did");

  // ---- EEVEE: bonded by day is ESPEON, bonded at night is UMBREON
  ready(p, DEX_EEVEE, 80, 12);
  p.evolve();
  ck(p.speciesId == DEX_ESPEON, "a bonded EEVEE at noon becomes ESPEON");
  ready(p, DEX_EEVEE, 80, 21);
  p.evolve();
  ck(p.speciesId == DEX_UMBREON, "a bonded EEVEE at 21:00 becomes UMBREON");
  // ...and unbonded it never reaches either, across many tries
  bool leaked = false, evolved = true;
  for (int i = 0; i < 60; i++) {
    ready(p, DEX_EEVEE, 0, i % 2 ? 12 : 21);
    p.evolve();
    if (p.speciesId == DEX_EEVEE) evolved = false;
    if (p.speciesId == DEX_ESPEON || p.speciesId == DEX_UMBREON) leaked = true;
  }
  ck(evolved && !leaked, "an unbonded EEVEE always evolves, and never into ESPEON or UMBREON");

  // ---- a missing pack is EVO_NO_ART, not READY (the card used to say READY)
  ready(p, 95, 0, 12);                    // ONIX -> STEELIX (Johto)
  gRegionArt = 0xFFFF & ~(1u << regionOfDex(208));
  ck(p.evoCheck().block == EVO_NO_ART && !p.canEvolveNow(), "ONIX with Johto's pack missing reports NO ART");
  gRegionArt = 0xFFFF;
  ck(p.canEvolveNow(), "and evolves once the pack is back");

  // ---- care and state still gate, in that order of reporting
  ready(p, 4, 0, 12);
  p.fullness = 10;
  ck(p.evoCheck().block == EVO_CARE, "a hungry creature reports CARE");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
