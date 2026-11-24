// Shiny chains: the rule at its limits, linking through the REAL event path the
// battle uses (EV_WILD_WIN / EV_WILD_CATCH), the break, persistence, and -- the
// caller -- that startWildBattle() really does serve the chained species half
// the time.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "dex.h"
#include "wild.h"
#include "chain.h"
#include "game_events.h"
#include <cstdio>
uint32_t g_seed=19; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render();
bool startWildBattle(uint8_t region, bool hard);
extern Pet pet; extern bool battleOpen; extern int16_t wildDex;
static int bad=0;
static void ck(bool ok,const char *s){printf("%s  %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}

int main(){
  // ---- the rule at its limits
  ck(chainShinyMult(0) == 1 && chainShinyMult(1) == 2 && chainShinyMult(15) == 16 &&
     chainShinyMult(16) == 16 && chainShinyMult(99) == 16, "multiplier is 1 + length, capped at 16");
  ck(chainAlive(100, 100) && chainAlive(100, 101) && !chainAlive(100, 102), "alive through the next day, broken after a whole day");
  ck(wildShinyThresholdFor(0, 1, false) * 4096 == WILD_SHINY_SCALE, "no chain is the flat 1/4096");
  ck(wildShinyThresholdFor(0, 16, false) * 256 == WILD_SHINY_SCALE, "the longest chain is 1/256");
  ck(wildShinyThresholdFor(0, 16, true) * 128 == WILD_SHINY_SCALE, "and 1/128 with the SHINY CHARM");
  ck(!chainEligible(150) && chainEligible(16), "a legendary (MEWTWO) cannot chain; PIDGEY can");

  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(6, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.lastSeenEpoch = 20000UL * 86400UL + 12 * 3600;
  const uint32_t day = pet.currentDay();

  // ---- linking through the battle's own events
  gameEvent(EV_WILD_WIN, 16);
  gameEvent(EV_WILD_CATCH, 16);
  gameEvent(EV_WILD_WIN, 16);
  ck(chainDex(day) == 16 && chainLen(day) == 3, "win, catch, win on PIDGEY: a chain of 3");
  gameEvent(EV_WILD_WIN, 150);
  ck(chainDex(day) == 16 && chainLen(day) == 3, "beating a legendary neither breaks nor extends it");
  gameEvent(EV_WILD_WIN, 19);
  ck(chainDex(day) == 19 && chainLen(day) == 1 && chainBest() == 3, "another species restarts it; the best is kept");
  for (int i = 0; i < 120; i++) chainLink(19, day);
  ck(chainLen(day) == CHAIN_MAX, "it stops counting at CHAIN_MAX rather than wrapping");

  // ---- persistence and the break
  chainReset(); chainBegin(&pet);
  ck(chainDex(day) == 19 && chainLen(day) == CHAIN_MAX, "the chain survives a reboot");
  ck(chainLen(day + 1) == CHAIN_MAX && chainLen(day + 2) == 0, "and breaks after a whole day without a link");
  chainLink(19, day + 2);
  ck(chainLen(day + 2) == 1, "a link after the break starts over at 1");

  // ---- chainPick: one coin, the region, the pack
  chainLink(16, day + 2);
  ck(chainPick(0, day + 2, true) == 16 && chainPick(0, day + 2, false) == 0, "the coin decides, both ways");
  ck(chainPick(1, day + 2, true) == 0, "PIDGEY does not appear in a Johto roll");
  gRegionArt = 0xFFFF & ~1u;
  ck(chainPick(0, day + 2, true) == 0, "nor when Kanto's pack is missing");
  gRegionArt = 0xFFFF;

  // ---- the CALLER: startWildBattle() serves the chained species about half the time
  pet.lastSeenEpoch = (day + 2) * 86400UL + 12 * 3600;
  int hits = 0, n = 300;
  for (int i = 0; i < n; i++) {
    battleOpen = false;
    if (startWildBattle(0, false) && wildDex == 16) hits++;
  }
  battleOpen = false;
  ck(hits > n * 40 / 100 && hits < n * 65 / 100, "with a live chain, about half of Kanto encounters are the chained PIDGEY");
  chainReset(); chainBegin(&pet);
  pet.lastSeenEpoch = (day + 9) * 86400UL;    // long broken
  hits = 0;
  for (int i = 0; i < n; i++) { battleOpen = false; if (startWildBattle(0, false) && wildDex == 16) hits++; }
  battleOpen = false;
  ck(hits < n * 10 / 100, "with the chain broken, PIDGEY is back to an ordinary share");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
