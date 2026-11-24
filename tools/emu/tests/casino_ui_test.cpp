// The Rocket Game Corner through the REAL sketch: the hidden door (a finger
// held on ARCADE, driven through the touch path in real time), buying coins
// with the wallet, a spin, a blackjack hand, a High/Low cash-out, and the prize
// shop -- an avatar that then counts as unlocked on the player card, and a
// card background the player card draws. Every coordinate comes from the
// screen's own geometry functions.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "casino.h"
#include "casino_art.h"
#include "rank.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
uint32_t g_seed=83; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void loop(); void render(); void onTap(int16_t,int16_t); void onSwipeV(int); void onRim(int);
void emuFireInterrupt();
uint8_t uiCurrentScreen(); uint8_t uiRimPages(); uint8_t uiRimPage();
extern const char *const SCREEN_NAME[];
extern Pet pet; extern bool trainOpen, casinoOpen, arcadeOpen, playerOpen;
extern uint8_t casShopKind, casShopPage;
int uiTrainRowCenterY(int i);
void casLobbyBtn(uint8_t i, int *x, int *y, int *w, int *h);
void casBtn(uint8_t i, uint8_t n, int *x, int *y, int *w, int *h);
void casCoinsBtn(uint8_t i, int *x, int *y, int *w, int *h);
void casTab(uint8_t i, int *x, int *y, int *w, int *h);
void casTile(uint8_t i, int *x, int *y, int *w, int *h);
const uint8_t *casSlotStops();
const Blackjack &casBlackjack();
const HighLow &casHighLow();
uint8_t casinoView();
uint16_t cardBgPixel(uint8_t bg, int x, int y, uint32_t t);
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static bool onScreen(const char *n){ return !strcmp(SCREEN_NAME[uiCurrentScreen()], n); }

static void pump(int n){ for(int i=0;i<n;i++){ std::this_thread::sleep_for(std::chrono::milliseconds(30)); loop(); } }
static void hold(int x,int y,int ms){
  g_touchX=x; g_touchY=y; g_touchDown=true; emuFireInterrupt();
  pump(ms/30);
  g_touchDown=false; emuFireInterrupt(); pump(3);
}
typedef void (*Geo)(uint8_t, int*, int*, int*, int*);
static void tapGeo(Geo g, uint8_t i){ int x,y,w,h; g(i,&x,&y,&w,&h); onTap(x+w/2, y+h/2); render(); }
static void tapBtn(uint8_t i, uint8_t n){ int x,y,w,h; casBtn(i,n,&x,&y,&w,&h); onTap(x+w/2, y+h/2); render(); }

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(25, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.sleeping = false;

  // ---- the hidden door
  trainOpen = true; render();
  hold(233, uiTrainRowCenterY(3), 2000);
  ck(!casinoOpen && !arcadeOpen && trainOpen, "two seconds on ARCADE opens nothing: not the arcade, not the casino");
  hold(233, uiTrainRowCenterY(1), 5600);
  ck(!casinoOpen, "five seconds on another row does not find the door");
  hold(233, uiTrainRowCenterY(3), 5600);
  ck(casinoOpen && onScreen("casino"), "five seconds on ARCADE opens the ROCKET GAME CORNER");
  ck(!arcadeOpen, "and not the arcade itself");

  // ---- coins for money
  pet.wallet = 1000; pet.coins = 0;
  tapGeo(casLobbyBtn, 3);
  tapGeo(casCoinsBtn, 0);
  ck(pet.coins == 50 && pet.wallet == 950, "50 coins for $50");
  { Pet q; q.begin(); ck(q.coins == 50 && q.wallet == 950, "both sides of the purchase are on flash"); }
  pet.wallet = 10;
  tapGeo(casCoinsBtn, 0);
  ck(pet.coins == 50 && pet.wallet == 10, "a short wallet buys nothing");
  onSwipeV(1); render();
  ck(onScreen("casino") && casinoOpen && casinoView() == 0, "down: back to the lobby");

  // ---- a spin
  tapGeo(casLobbyBtn, 0);
  tapBtn(0, 1);                                   // SPIN (bet 10)
  ck(pet.coins == 40, "a spin takes the 10-coin bet");
  for (int i = 0; i < 3; i++) tapBtn(0, 1);       // STOP, STOP, STOP
  const uint8_t *st = casSlotStops();
  uint16_t pay = slotPays(SLOT_REELS[0][st[0]], SLOT_REELS[1][st[1]], SLOT_REELS[2][st[2]]) * 10;
  ck(pet.coins == 40 + pay, "the reels pay exactly what the paytable says for where they stopped");
  onSwipeV(1); render();

  // ---- a blackjack hand
  pet.coins = 500;
  tapGeo(casLobbyBtn, 2);
  tapBtn(0, 1);                                   // DEAL
  if (casBlackjack().state == BJ_PLAYER) tapBtn(1, 3);   // STAND
  ck(casBlackjack().state == BJ_DONE, "a dealt hand plays to the end");
  ck(pet.coins == 500 - 10 + casBlackjack().payout(), "and pays its result into the coins");
  onSwipeV(1); render();

  // ---- high / low: cash out at once returns the stake
  pet.coins = 100;
  tapGeo(casLobbyBtn, 1);
  tapBtn(0, 1);                                   // DEAL
  ck(pet.coins == 90 && casHighLow().state == HL_GUESS, "High/Low takes the stake");
  tapBtn(2, 3);                                   // CASH OUT
  ck(pet.coins == 100, "and cashing out before guessing gives it back");
  onSwipeV(1); render();

  // ---- the prize shop: an avatar
  pet.coins = CASINO_PRICE;
  tapGeo(casLobbyBtn, 4);
  ck(onScreen("casshop"), "PRIZES opens the shop");
  tapGeo(casTab, 1);                              // AVATARS
  tapGeo(casTile, 0);
  ck(pet.coins == CASINO_PRICE && !casinoOwns(PK_AVATAR, 0), "one tap on a 9999 prize only arms it");
  tapGeo(casTile, 0);
  ck(pet.coins == 0 && casinoOwns(PK_AVATAR, 0), "the second tap buys it");
  ck(pet.avatar == SHOP_AVATAR_FIRST, "and puts it on");
  ck(rankAvatarUnlocked(SHOP_AVATAR_FIRST), "the player card counts it as unlocked");
  ck(!rankAvatarUnlocked(SHOP_AVATAR_FIRST + 1), "but not the one beside it");
  tapGeo(casTile, 1); tapGeo(casTile, 1);
  ck(!casinoOwns(PK_AVATAR, 1) && pet.coins == 0, "with no coins left nothing more is bought");

  // ---- paging, on the rim
  ck(uiRimPages() == (SHOP_AVATAR_COUNT + 5) / 6, "the avatar shop pages on the rim, one page per six");
  onRim(1); render();
  ck(casShopPage == 1 && onScreen("casshop"), "a rim drag turns the page and stays in the shop");

  // ---- a card background, worn by the player card
  pet.coins = CASINO_PRICE;
  tapGeo(casTab, 2);                              // CARD BGS
  tapGeo(casTile, 2); tapGeo(casTile, 2);         // FIRE
  ck(casinoOwns(PK_CARDBG, 2) && casino.bg == 2, "a card background is bought and equipped");
  {
    casinoBegin();
    ck(casinoOwns(PK_CARDBG, 2) && casino.bg == 2 && casinoOwns(PK_AVATAR, 0), "everything bought survives a reload");
  }
  uint8_t live = 0;
  for (uint8_t b = 0; b < CARD_BG_COUNT; b++) if (cardBgAnimated(CARD_BGS[b].pattern)) {
    bool moved = false;
    for (int y = 20; y < 460 && !moved; y += 7) for (int x = 20; x < 460 && !moved; x += 7)
      if (cardBgPixel(b, x, y, 0) != cardBgPixel(b, x, y, 6000)) moved = true;
    if (moved) live++;
    else printf("      %s does not move\n", CARD_BGS[b].name);
  }
  ck(live == 6, "every LIVE background actually moves");
  bool still = true;
  for (int y = 20; y < 460; y += 7) for (int x = 20; x < 460; x += 7)
    if (cardBgPixel(2, x, y, 0) != cardBgPixel(2, x, y, 6000)) still = false;
  ck(still, "and a static one does not");

  // ---- out
  onSwipeV(1); render();
  ck(onScreen("casino"), "down from the shop: the lobby");
  onSwipeV(1); render();
  ck(!casinoOpen && trainOpen, "down from the lobby: back on the training menu");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
