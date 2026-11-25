// The daycare driven the way a player drives it: real swipes along the tile axis,
// real taps on the coordinates the FIRMWARE says its targets are at
// (dcCardCenterX() and friends -- never a copy of the layout here), down to back
// out. breed_test proves the rules; this proves the screen actually asks them.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "items.h"
#include "inventory.h"
#include "breed.h"
#include <cstdio>
uint32_t g_seed=5; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void onSwipe(int dir); void onSwipeV(int dir);
void onTap(int16_t x, int16_t y); void onRim(int dir);
int uiTileIndex();
extern Pet pet;
extern bool daycareOpen, partyOpen; extern uint8_t dcView; extern uint16_t dcPage;
int dcCardCenterX(uint8_t i); int dcCardCenterY();
int dcBtnCenterX(uint8_t i); int dcBtnCenterY();
int dcRowCenterY(uint8_t i);
uint16_t dcPickedA(); uint16_t dcPickedB();
void dcDebugPick(uint16_t a, uint16_t b);

static int bad=0;
static void ck(bool ok,const char *s){printf("%s  %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}
static void tap(int x, int y){ onTap((int16_t)x,(int16_t)y); render(); }
static void tapCard(uint8_t i){ tap(dcCardCenterX(i), dcCardCenterY()); }
static void tapBtn(uint8_t i){ tap(dcBtnCenterX(i), dcBtnCenterY()); }
static void tapRow(uint8_t r){ tap(233, dcRowCenterY(r)); }

int main(){
  setup();
  for (int i=0;i<4;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(4,false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  for (int i=0;i<PARTY_SLOTS;i++) party.releaseAt(i);
  // party 0 CHARIZARD, 1 CHARMELEON (compatible), 2 SQUIRTLE (not with either)
  const int16_t dex[3] = { 6, 5, 7 };
  for (int i=0;i<3;i++){ PartyMon m; m.dex=dex[i]; m.level=30;
    m.ivAtk=m.ivDef=m.ivSpe=m.ivHp=(uint8_t)(10+i); party.replaceAt(i,m); }
  pet.lastSeenEpoch = 20000UL*86400UL + 3600;

  // ---- reached from the pet along the tile axis: PARTY, then DAYCARE
  int home = uiTileIndex();
  onSwipe(1); render();
  ck(partyOpen, "one swipe right from the pet is the PARTY");
  onSwipe(1); render();
  ck(daycareOpen && dcView == 0 && uiTileIndex() == home - 2, "two swipes right is the DAYCARE");

  // ---- picking parent A, then B, by tapping
  tapCard(0);
  ck(dcView == 1, "tapping card A opens the picker");
  tapRow(0);
  ck(dcView == 0 && dcPickedA() == 0, "tapping the first row sets A to party slot 0 and returns");
  tapCard(1);
  tapRow(0);
  ck(dcView == 1 && dcPickedB() == 0xFFFF, "the row A already holds is locked for B, not taken");
  onSwipeV(1); render();
  ck(daycareOpen && dcView == 0, "swiping down backs out of the picker to the daycare, not further");
  tapCard(1); tapRow(2);
  ck(dcPickedB() == 2, "B set to SQUIRTLE");

  // ---- an incompatible pair: BREED is dead to the touch, not just drawn grey
  tapBtn(0);
  ck(!breedHasQueued(), "BREED on CHARIZARD + SQUIRTLE queues nothing");
  tapCard(1); tapRow(1);
  ck(dcPickedB() == 1, "B changed to CHARMELEON");
  tapBtn(0);
  ck(breedHasQueued() && breedQueued().dex == 4, "BREED on a compatible pair queues a CHARMANDER egg");
  tapBtn(0);
  ck(breedHasQueued(), "a second BREED tap does not replace or add an egg");

  // ---- SWAP IN takes two taps, because it replaces the egg on screen
  pet.newEgg();                 // takes the queued egg; breed another for the swap
  ck(pet.isEgg() && pet.eggPeek() == 4, "the next egg was the bred one");
  pet.lastSeenEpoch += 86400UL;  // next day: the cooldown has passed
  tapBtn(0);
  ck(breedHasQueued(), "next day, breed again");
  dcDebugPick(1, 0);            // swap the parents round: CHARMELEON first
  int16_t before = pet.eggPeek();
  tapBtn(1);
  ck(breedHasQueued() && pet.eggPeek() == before, "ONE tap on SWAP IN only arms it");
  tapBtn(1);
  ck(!breedHasQueued() && pet.eggPeek() == 4, "the second tap swaps the bred egg in");

  // ---- down from the daycare's home goes back to the pet
  onSwipeV(1); render();
  ck(!daycareOpen && uiTileIndex() == home, "swiping down from the daycare returns to the pet");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
