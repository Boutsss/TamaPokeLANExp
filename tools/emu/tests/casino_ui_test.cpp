// The Game Corner through the REAL sketch: the hidden door (a finger
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
uint8_t uiCurrentScreen(); uint16_t uiRimPages(); uint16_t uiRimPage();
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
bool casinoSpinning(); bool casinoSpinShowsCorner(); uint32_t casinoHeldMs();
extern Arduino_Canvas *gfx;
static uint16_t px(int x, int y) { return gfx->buffer()[y * 466 + x]; }
const Blackjack &casBlackjack();
uint8_t casRouletteResult(); bool casRouletteSpinning(); uint8_t casRouletteBetKind(); uint8_t casRouletteBetArg();
void casRlCell(uint8_t i, int *x, int *y, int *w, int *h); uint8_t casRlCellBet(uint8_t i); void casRlBtn(uint8_t i, int *x, int *y, int *w, int *h);
bool casinoDoorsShowing();
bool casinoWelcomeShowing(); void casinoOpenNow(); void casinoClose(); void casinoDbgSkipDoor(); void uiBack();
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
  hold(233, uiTrainRowCenterY(1), CASINO_HOLD_MS + 600);
  ck(!casinoOpen, "the full hold on another row does not find the door");
  // ---- the reveal: the ARCADE row turns over while it is held
  {
    int ry = uiTrainRowCenterY(3);
    render();
    uint16_t cream = px(233 + 120, ry);            // the ARCADE row's own face
    g_touchX = 233; g_touchY = ry; g_touchDown = true; emuFireInterrupt();
    pump(80);                                      // ~2.4 s
    ck(!casinoSpinning() && casinoHeldMs() > 2000, "for the first 3 s nothing shows: the row stays ARCADE");
    bool sawArcade = false, sawCorner = false, early = false;
    while (casinoHeldMs() < CASINO_REVEAL_MS + 100 && casinoHeldMs() > 0) {
      pump(1);
      if (!casinoSpinning()) continue;
      if (casinoSpinShowsCorner()) sawCorner = true; else sawArcade = true;
      if (casinoOpen) early = true;
    }
    ck(sawArcade && sawCorner, "held on, it turns: both faces come round");
    ck(!early && trainOpen && !casinoOpen, "and nothing opens while it turns");
    render();
    ck(casinoSpinShowsCorner() && px(233 + 120, ry) != cream && px(233 + 120, ry) == 0x0841,
       "it comes to rest on its black Team Rocket GAME CORNER face -- on the panel, not just in a flag");
    g_touchDown = false; emuFireInterrupt(); pump(3);
    ck(!casinoOpen && !casinoSpinning(), "let go before the door: nothing opens");
    render();
    ck(px(233 + 120, ry) == cream, "and the row is back to ARCADE");
  }
  const uint32_t coinsBefore = pet.coins;
  ck(!casino.welcomed, "a fresh save has not had the dealer's welcome");
  g_touchX = 233; g_touchY = uiTrainRowCenterY(3); g_touchDown = true; emuFireInterrupt();
  pump((CASINO_HOLD_MS + 300) / 30);
  ck(casinoOpen && onScreen("casino"), "the full hold on ARCADE opens the GAME CORNER");
  // ---- the dealer's welcome: CASINO_WELCOME_COINS, once per save
  ck(pet.coins == coinsBefore + CASINO_WELCOME_COINS && casino.welcomed && casinoWelcomeShowing(),
     "a first visit: the dealer stakes you 100 coins, and a card says so");
  render();
  ck(casinoDoorsShowing() && px(40, 233) == 0x18C3, "through Team Rocket's doors: shut at first");
  g_touchDown = false; emuFireInterrupt(); pump(3);
  { int x,y,w,h; casLobbyBtn(4,&x,&y,&w,&h); onTap(x+w/2, y+h/2); render(); }
  ck(casinoView() == 0, "and the lobby does not take a tap until they have opened");
  pump(50);
  render();
  ck(!casinoDoorsShowing() && px(40, 233) != 0x18C3, "then they slide apart onto the lobby");
  ck(!arcadeOpen, "and not the arcade itself");
  { int x,y,w,h; casLobbyBtn(0,&x,&y,&w,&h); onTap(x+w/2, y+h/2); render(); }
  ck(!casinoWelcomeShowing() && casinoView() == 0, "any tap puts the welcome card away, and does nothing else");
  {
    uint32_t c = pet.coins;
    casinoClose(); casinoOpenNow(); casinoDbgSkipDoor(); render();
    ck(pet.coins == c && !casinoWelcomeShowing(), "a second visit pays nothing and shows no card");
    casinoBegin();   // the flag is on flash, not just in RAM
    ck(casino.welcomed, "the welcome is remembered across a reload");
    // the negative check: the gift really is gated on the flag, not just lucky
    casino.welcomed = 0; c = pet.coins;
    casinoClose(); casinoOpenNow(); casinoDbgSkipDoor();
    ck(pet.coins == c + CASINO_WELCOME_COINS, "with the flag cleared (a WIPE) the dealer welcomes you again");
    { int x,y,w,h; casLobbyBtn(0,&x,&y,&w,&h); onTap(x+w/2, y+h/2); render(); }
  }

  // ---- coins for money
  pet.wallet = 1000; pet.coins = 0;
  tapGeo(casLobbyBtn, 4);
  tapGeo(casCoinsBtn, 0);
  ck(pet.coins == 50 && pet.wallet == 0, "50 coins for $1000, the GBC price");
  { Pet q; q.begin(); ck(q.coins == 50 && q.wallet == 0, "both sides of the purchase are on flash"); }
  pet.wallet = 999;
  tapGeo(casCoinsBtn, 0);
  ck(pet.coins == 50 && pet.wallet == 999, "a short wallet buys nothing");
  pet.wallet = 10000;
  tapGeo(casCoinsBtn, 1);
  ck(pet.coins == 550 && pet.wallet == 0, "500 coins for $10,000");
  onSwipeV(1); render();
  ck(onScreen("casino") && casinoOpen && casinoView() == 0, "down: back to the lobby");

  // ---- a spin
  tapGeo(casLobbyBtn, 0);
  pet.coins = 50;
  tapBtn(0, 1);                                   // SPIN (bet 1, like the GBC machines)
  ck(pet.coins == 49, "a spin takes a 1-coin bet");
  for (int i = 0; i < 3; i++) tapBtn(0, 1);       // STOP, STOP, STOP
  const uint8_t *st = casSlotStops();
  uint16_t pay = slotPays(SLOT_REELS[0][st[0]], SLOT_REELS[1][st[1]], SLOT_REELS[2][st[2]]);
  ck(pet.coins == 49 + pay, "the reels pay exactly what the paytable says for where they stopped");
  onSwipeV(1); render();
  ck(casinoView() != 0, "a swipe at a table does not leave it: only the held BACK button does");
  uiBack(); render();
  ck(casinoView() == 0, "the held BACK button: back to the lobby");

  // ---- a blackjack hand
  pet.coins = 500;
  tapGeo(casLobbyBtn, 2);
  tapBtn(0, 1);                                   // DEAL
  if (casBlackjack().state == BJ_PLAYER) tapBtn(1, 3);   // STAND
  ck(casBlackjack().state == BJ_DONE, "a dealt hand plays to the end");
  ck(pet.coins == 500 - casBlackjack().bet + casBlackjack().payout(), "and pays its result into the coins");
  uiBack(); render();

  // ---- high / low: cash out at once returns the stake
  pet.coins = 100;
  tapGeo(casLobbyBtn, 1);
  tapBtn(0, 1);                                   // DEAL
  ck(pet.coins == 95 && casHighLow().state == HL_GUESS, "High/Low takes the 5-coin stake");
  tapBtn(2, 3);                                   // CASH OUT
  ck(pet.coins == 100, "and cashing out before guessing gives it back");
  uiBack(); render();

  // ---- roulette
  pet.coins = 100;
  tapGeo(casLobbyBtn, 3);
  ck(onScreen("casino") && casinoView() == 6, "ROULETTE opens the wheel");
  // every bet on the table has its own cell -- GREEN (the zero) included
  {
    bool has[RB_COUNT] = { false };
    for (uint8_t c = 0; c < 9; c++) if (casRlCellBet(c) < RB_COUNT) has[casRlCellBet(c)] = true;
    bool all = true; for (uint8_t b = 0; b < RB_COUNT; b++) if (!has[b]) all = false;
    ck(all, "every kind of bet has a button on the table");
  }
  uint8_t cGreen = 0, cNum = 0;
  for (uint8_t c = 0; c < 9; c++) { if (casRlCellBet(c) == RB_GREEN) cGreen = c; if (casRlCellBet(c) == RB_NUMBER) cNum = c; }
  tapGeo(casRlCell, cGreen);
  ck(casRouletteBetKind() == RB_GREEN, "GREEN can be bet on directly");
  tapGeo(casRlCell, cNum);                        // a straight number: 17 to start
  { int x,y,w,h; casRlCell(cNum,&x,&y,&w,&h); onTap(x+w-6, y+h/2); onTap(x+6, y+h/2); onTap(x+6, y+h/2); render(); }
  ck(casRouletteBetKind() == RB_NUMBER && casRouletteBetArg() == 16, "the number steps up on its right half, down on its left");
  tapGeo(casRlCell, 0);                           // RED
  tapGeo(casRlBtn, 1);                            // SPIN (bet 5)
  ck(pet.coins == 95 && casRouletteSpinning(), "SPIN takes the 5-coin bet and the wheel turns");
  tapGeo(casRlCell, 1);
  ck(casRouletteBetKind() == RB_RED, "no more bets while it turns");
  pump((3600 + 300) / 30);
  ck(!casRouletteSpinning(), "it comes to rest");
  ck(pet.coins == 95 + 5 * roulettePays(RB_RED, 0, casRouletteResult()), "and pays exactly what the table says for where the ball lies");
  {
    render();
    uint8_t r = casRouletteResult();
    uint16_t want = r == 0 ? 0x0480 : rouletteRed(r) ? 0xE8E4 : 0x18C3;
    ck(px(233, 148 - 74 + 5) == want, "the pocket under the pointer is the winning number's colour, on the panel");
  }
  uiBack(); render();

  // ---- the prize shop: an avatar
  pet.coins = CASINO_PRICE;
  tapGeo(casLobbyBtn, 5);
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
  ck(uiRimPages() == (casinoShopCount(PK_AVATAR) + 5) / 6, "the avatar shop pages on the rim, one page per six");
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
