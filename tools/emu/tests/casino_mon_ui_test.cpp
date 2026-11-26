// The armored MEWTWO through the REAL screens: the Game Corner's POKEMON tab
// lists it at 1,000,000 coins, one tap arms it and a second buys it, it lands in
// the PC box with the purchase banner up, too few coins buy nothing -- and once
// it is the live pet the main screen and a battle stream ITS sprite (pa150.bin),
// judged on the loaded art rather than on what was asked for.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "casino.h"
#include "sdmon.h"
#include "battle.h"
#include "species.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
uint32_t g_seed=89; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void loop(); void render(); void onTap(int16_t,int16_t);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
extern Pet pet; extern bool casinoOpen;
extern uint8_t casShopKind, casShopPage;
extern PmdMon pmd, btlPmd[2];
extern Combatant btlYou;
void casLobbyBtn(uint8_t i, int *x, int *y, int *w, int *h);
void casTab(uint8_t i, int *x, int *y, int *w, int *h);
void casTile(uint8_t i, int *x, int *y, int *w, int *h);
uint8_t casinoView();
void startTrainerBattle(uint8_t idx, bool hard);
extern Arduino_Canvas *gfx;
static uint16_t px(int x, int y) { return gfx->buffer()[y * 466 + x]; }
uint16_t uiSkinMark();   // asked of the firmware, never retyped
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static bool onScreen(const char *n){ return !strcmp(SCREEN_NAME[uiCurrentScreen()], n); }
static void pump(int n){ for(int i=0;i<n;i++){ std::this_thread::sleep_for(std::chrono::milliseconds(30)); loop(); } }
typedef void (*Geo)(uint8_t, int*, int*, int*, int*);
static void tapGeo(Geo g, uint8_t i){ int x,y,w,h; g(i,&x,&y,&w,&h); onTap(x+w/2, y+h/2); render(); }
static int boxArmored(int *at = nullptr) {
  int n = 0;
  for (int i = 0; i < BOX_SLOTS; i++)
    if (party.box[i].dex == 150 && party.box[i].skin() == SKIN_ARMORED) { n++; if (at) *at = i; }
  return n;
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(25, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.sleeping = false;

  casinoOpen = true; render(); pump(60);
  ck(onScreen("casino"), "in the Game Corner");
  tapGeo(casLobbyBtn, 5);                       // PRIZES
  tapGeo(casTab, PK_MON);
  ck(casShopKind == PK_MON && casinoShopAt(PK_MON, 0) == 0, "the POKEMON tab lists the armored MEWTWO");
  {
    int x, y, w, h; casTab(PK_MON, &x, &y, &w, &h);
    int x0, y0, w0, h0; casTab(0, &x0, &y0, &w0, &h0);
    ck(x + w <= 233 + 182 && x0 >= 233 - 182 && w >= 80, "four tabs fit the felt, none narrower than 80 px");
  }

  // too few coins: two taps buy nothing
  pet.coins = 999999;
  tapGeo(casTile, 0); tapGeo(casTile, 0);
  ck(pet.coins == 999999 && boxArmored() == 0, "999,999 coins buy nothing");

  // enough: the first tap only arms it, the second buys it
  pet.coins = 1234567;
  pump(100);                                    // let the refusal's moment pass
  render();
  const uint16_t inkBanner = UI_INK;            // casBanner's plate
  ck(px(233 - 124, 242) != inkBanner, "no banner before a purchase (the check below can fail)");
  tapGeo(casTile, 0);
  ck(pet.coins == 1234567 && boxArmored() == 0, "the first tap only arms the purchase");
  tapGeo(casTile, 0);
  int at = -1;
  ck(pet.coins == 234567 && boxArmored(&at) == 1, "the second spends 1,000,000 and puts it in the PC box");
  render();
  ck(px(233 - 124, 242) == inkBanner, "and the banner says so");
  { Pet q; q.begin(); ck(q.coins == 234567, "the coins spent are on flash"); }

  // the box marks it: a purple rim inside its cell, read off the panel
  { extern bool partyOpen, boxOpen; extern uint8_t boxPage; void boxCellPos(uint8_t, int*, int*);
    casinoOpen = false; partyOpen = true; boxOpen = true; boxPage = (uint8_t)(at / 30); render();
    int cx, cy; boxCellPos((uint8_t)(at % 30), &cx, &cy);
    ck(px(cx - 27 + 1, cy) == uiSkinMark(), "the PC box marks it with a purple rim");
    party.box[at].skinTag = 0; render();
    ck(px(cx - 27 + 1, cy) != uiSkinMark(), "and an ordinary MEWTWO without one");
    party.box[at].setSkin(SKIN_ARMORED);
    partyOpen = boxOpen = false; }

  // as the live pet: its own sprite on the main screen. An ORDINARY MEWTWO
  // first, so the sprite cache already holds species 150 -- the armored one
  // must still replace it (a cache keyed on the species alone would not).
  casinoOpen = false;
  PmdMon plain; plain.load(150, false);
  pet.dbgHatchAs(150, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  for (int i = 0; i < 5; i++) { loop(); render(); }
  ck(pmd.loaded && pmd.acts[PMD_IDLE].w == plain.acts[PMD_IDLE].w, "an ordinary MEWTWO draws the ordinary sprite");
  ck(at >= 0 && pet.switchTo(party.box[at]), "the armored one can be raised");
  for (int i = 0; i < 5; i++) { loop(); render(); }
  ck(pmd.loaded && pmd.acts[PMD_IDLE].w != plain.acts[PMD_IDLE].w && pmd.has(PMD_WALKL),
     "the main screen streams the armored sprite, not the ordinary MEWTWO");

  startTrainerBattle(0, false);
  render();
  ck(btlYou.dex == 150 && btlYou.skin == SKIN_ARMORED && btlPmd[0].loaded &&
     btlPmd[0].acts[PMD_IDLE].w != plain.acts[PMD_IDLE].w,
     "and so does a battle");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
