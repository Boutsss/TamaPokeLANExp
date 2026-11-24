// Pokedex rewards: the only source of held items and the SHINY CHARM. Driven
// through the real registration path (Pet::registerSpecies -> EV_DEX_REGISTER
// -> dexRewardsCheck), not by calling the check directly, so a missing event
// fails here rather than on a board.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "dex.h"
#include "inventory.h"
#include "dex_rewards.h"
#include "held_items.h"
#include "game_events.h"
#include <cstdio>
uint32_t g_seed=17; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char *s){printf("%s  %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}

int main(){
  gRegionArt = 0xFFFF;
  Pet p; p.begin(); p.dbgHatchAs(4,false);
  bag.begin(); bag.clear();
  dexRewardsReset(); dexRewardsBegin(&p);
  uint8_t g0 = dexGiftsGranted();
  ck(g0==0, "a fresh save starts with no gifts paid");

  // Register species the save does not have yet, one at a time.
  int16_t next = 1;
  auto regNew = [&](int n){
    for (int k = 0; k < n && next <= DEX_COUNT; next++) {
      if (p.isRegistered(next)) continue;
      p.dbgRegister(next); k++;
    }
  };
  while (p.registeredCount() < DEX_GIFT_EVERY - 1) regNew(1);
  ck(dexGiftsGranted()==g0 && !bag.has(IT_ORAN_BERRY), "no gift one short of the milestone");
  regNew(1);
  ck(dexGiftsGranted()==g0+1 && bag.count(IT_ORAN_BERRY)==3, "the 25th registration pays three ORAN BERRIES");

  // Re-registering something already owned is not progress.
  uint8_t before = bag.count(IT_ORAN_BERRY);
  for (int i = 0; i < 40; i++) p.dbgRegister(1);
  ck(bag.count(IT_ORAN_BERRY)==before && dexGiftsGranted()==g0+1, "re-registering an owned species pays nothing");

  // A reload neither re-pays nor forgets.
  dexRewardsReset(); bag.begin(); dexRewardsBegin(&p);
  ck(dexGiftsGranted()==g0+1 && bag.count(IT_ORAN_BERRY)==before, "a reload remembers what was paid, once");

  // A failed bag write leaves the gift OWED, and the next registration pays it.
  while (p.registeredCount() < 2*DEX_GIFT_EVERY - 1) regNew(1);
  nvsFailWritesAfter(0);
  regNew(1);
  nvsResumeWrites();
  ck(dexGiftsGranted()==g0+1 && !bag.has(IT_SITRUS_BERRY), "a failed write grants and records nothing");
  regNew(1);
  ck(dexGiftsGranted()==g0+2 && bag.count(IT_SITRUS_BERRY)==2, "the owed gift is paid on the next registration");

  // SHINY CHARM: completing a region's OBTAINABLE dex, once.
  const uint8_t K = 0;   // Kanto
  uint16_t need = regionObtainableCount(K);
  ck(need > 100 && need <= REGIONS[K].hi - REGIONS[K].lo + 1, "Kanto has a sensible obtainable count");
  int16_t last = -1;
  for (int16_t d = REGIONS[K].lo; d <= (int16_t)REGIONS[K].hi; d++)
    if (dexObtainable(d) && !p.isRegistered(d)) last = d;
  ck(last > 0, "there is still an unregistered obtainable Kanto species");
  for (int16_t d = REGIONS[K].lo; d <= (int16_t)REGIONS[K].hi; d++)
    if (dexObtainable(d) && d != last) p.dbgRegister(d);
  ck(!regionDexComplete(p,K) && !bag.has(IT_SHINY_CHARM), "one short of Kanto: no charm");
  p.dbgRegister(last);
  ck(regionDexComplete(p,K) && bag.has(IT_SHINY_CHARM) && (dexRewardFlags() & DXRW_SHINY),
     "the last Kanto registration grants the SHINY CHARM");
  ck(shinyCharmOdds(48)==24 && shinyCharmOdds(8)==4, "and the charm halves egg shiny odds, floor 4");

  // Every milestone already passed by now is paid, exactly once each.
  ck(dexGiftsGranted() == p.registeredCount()/DEX_GIFT_EVERY,
     "every passed milestone is paid exactly once");

  // A region whose pack is missing has nothing obtainable and is never complete.
  gRegionArt = 0xFFFF & ~(1u << K);
  ck(regionObtainableCount(K)==0 && !regionDexComplete(p,K), "a region with no pack can never count as complete");
  gRegionArt = 0xFFFF;

  // An old save that already passed milestones catches up at boot.
  bag.clear();
  Preferences pr; pr.begin("tamapoke", false); pr.putUChar("dxgf", 0); pr.putUChar("dxrw", 0);
  dexRewardsReset(); dexRewardsBegin(&p);
  ck(dexGiftsGranted()==p.registeredCount()/DEX_GIFT_EVERY && bag.has(IT_SHINY_CHARM),
     "an existing save is paid what it is owed on its first boot");

  printf("%s\n", bad?"FAILURES":"all good"); return bad?1:0;
}
