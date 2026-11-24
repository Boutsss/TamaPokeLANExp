// Gifts and visits through the REAL screens: this board's global `lan`,
// `friends`, `bag` and `pet`, cross-wired to a simulated peer board (its own
// Link, Friends, Inventory and Pet, and its own NVS swapped in around every
// call that may write). friends_test proves the protocol; this proves the
// screen drives it -- the LAN screen's FRIENDS button, HOST, the gift chooser,
// OFFER, VISIT -- and asks the screen for every coordinate it taps.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "items.h"
#include "inventory.h"
#include "link.h"
#include "friends.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=61; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void loop(); void render(); void onTap(int16_t,int16_t); void onSwipeV(int);
void friendsLoop(uint32_t now);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
static bool onScreen(const char *n) { return !strcmp(SCREEN_NAME[uiCurrentScreen()], n); }
extern Pet pet; extern Link lan; extern bool lanOpen, friendsOpen;
extern bool g_emuRadioUp;
int frRowCenterY(uint8_t r); int frRowAt(int16_t x, int16_t y);
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static Link P; static Friends PF; static Inventory PB; static Pet PP;
static NvsStore mine, theirs;
static void toPeer(void*, const uint8_t *b, uint8_t n){ uint8_t t[260]; memcpy(t,b,n); P.onPacket(t,n); }
static void toMe(void*, const uint8_t *b, uint8_t n){ uint8_t t[260]; memcpy(t,b,n); lan.onPacket(t,n); }
static uint32_t T = 1000;
static void peerPoll() {             // the other board, on its own flash
  mine = nvs(); nvs() = theirs;
  PF.poll(P, PP, PB, pet.currentDay(), millis());
  theirs = nvs(); nvs() = mine;
}
static void run(int ms) {
  for (int t = 0; t < ms; t += 100) {
    T += 100; P.tick(millis());
    loop();                          // the firmware's own loop: pumps lan, runs friendsLoop()
    peerPoll();
    render();
  }
}
static void tapRow(uint8_t r) { onTap(233, frRowCenterY(r)); render(); }
static void peerUp() {
  P.begin(false, "BEA"); P.send = toMe; P.id = 0x2222;
  PF.sessionStart(P, PP);
  P.tick(millis());
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(133, false);           // EEVEE
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.joy = 40;
  bag.clear(); bag.add(IT_ORAN_BERRY, 2); bag.add(IT_SHINY_CHARM, 1);
  PP.dbgHatchAs(25, true);              // the friend's shiny PIKACHU
  mine = nvs(); nvs().clear(); PB.clear(); PB.add(IT_POKEBALL, 1); theirs = nvs(); nvs() = mine;

  // ---- in from the LAN screen
  lanOpen = true; render();
  onTap(233, 120 + 2 * 70 + 28);         // the LAN screen's third row: FRIENDS
  render();
  ck(friendsOpen && !lanOpen && onScreen("friends"), "FRIENDS on the LAN screen opens the friends screen");
  ck(frRowAt(233, frRowCenterY(0)) == 0 && frRowAt(233, frRowCenterY(1)) == 1, "its rows answer at their own centres");
  ck(frRowAt(233, frRowCenterY(0) - 50) != 0 && frRowAt(4, frRowCenterY(0)) == -1, "and not outside them");

  // ---- no radio: said, not hung
  tapRow(0);
  ck(lan.state == LINK_REFUSED && lan.refusedWhy == LINK_WHY_RADIO && friendsOpen,
     "with no radio HOST says so and stays on the screen");

  // ---- pair through HOST
  g_emuRadioUp = true;
  lan.send = toPeer; lan.id = 0x1111;
  peerUp();
  tapRow(0);
  P.start();
  run(3000);
  ck(lan.ready() && lan.mode == LINK_MODE_FRIENDS && friends.havePeer, "HOST pairs in friends mode");
  ck(friends.peerMon.valid && friends.peerMon.dex == 25 && friends.peerMon.shiny, "and shows the friend's creature");

  // ---- the gift swap, through the chooser
  tapRow(0);                              // GIFT -> the chooser
  ck(onScreen("friends"), "GIFT opens the chooser on the same screen");
  tapRow(1);                              // OFFER the first giftable: ORAN BERRY (the key item is not listed)
  ck(friends.gph == FG_OFFER && friends.gmine == IT_ORAN_BERRY, "OFFER offers the ORAN BERRY, never the key item");
  run(1000);
  ck(bag.count(IT_ORAN_BERRY) == 2 && bag.count(IT_POKEBALL) == 0, "nothing moves on one offer");
  mine = nvs(); nvs() = theirs; PF.offerGift(P, PB, IT_POKEBALL, pet.currentDay()); theirs = nvs(); nvs() = mine;
  run(3000);
  ck(bag.count(IT_ORAN_BERRY) == 1 && bag.count(IT_POKEBALL) == 1 && bag.count(IT_SHINY_CHARM) == 1,
     "both offer: one ORAN BERRY left, the POKE BALL arrived, the charm untouched");
  ck(PB.count(IT_ORAN_BERRY) == 1 && PB.count(IT_POKEBALL) == 0, "and the friend's bag is the mirror image");
  {
    Inventory q; q.begin();
    ck(q.count(IT_POKEBALL) == 1 && q.count(IT_ORAN_BERRY) == 1, "the swap is on FLASH, not just in RAM");
    Friends f; f.begin();
    ck(f.giftDay == pet.currentDay() && f.rec.state == FR_NONE, "with the day recorded and the journal closed");
  }
  tapRow(0);
  ck(onScreen("friends") && friends.gph == FG_DONE, "GIFT again the same day does not reopen the chooser");

  // ---- the visit
  uint8_t joy0 = pet.joy;
  tapRow(1);                              // VISIT
  run(600);
  ck(friends.vnum >= 1 && friends.grantedVnum == friends.vnum && friends.visitBonus,
     "VISIT: the pets meet and the bonus is paid");
  ck(pet.joy >= joy0 + FRIEND_VISIT_JOY || pet.joy == 100, "joy went up by the visit's amount");
  ck(friends.visiting(millis()), "and the two pets are on screen together");
  onSwipeV(1); render();                  // down = back, one level: the tableau first
  ck(!friends.visiting(millis()) && friendsOpen, "down ends the visit tableau, not the screen");
  uint8_t joy1 = pet.joy;
  tapRow(1); run(600);
  ck(pet.joy == joy1 && !friends.visitBonus, "a second visit the same day pays nothing");
  onSwipeV(1); render();

  // ---- leaving
  onSwipeV(1); render();
  ck(!friendsOpen && lanOpen && lan.state == LINK_OFF && !friends.active,
     "down again leaves: back on the LAN screen, the session over");
  run(500);
  ck(P.state == LINK_LOST, "and the friend is told at once");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
