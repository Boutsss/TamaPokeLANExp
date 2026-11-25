// The Game Corner's PINBALL through the REAL sketch: the lobby button, PLAY
// charging its coins (and refusing without them), a held finger on each half
// of the panel raising THAT flipper through the touch path itself, a swipe
// that must not leave mid-ball, a finished game paying pinballPayout() and
// keeping the best score across a reload, and leaving between balls paying
// for what was scored.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "casino.h"
#include "pinball.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
uint32_t g_seed=91; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void loop(); void render(); void onTap(int16_t,int16_t); void onSwipeV(int);
void emuFireInterrupt();
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
extern Pet pet; extern bool casinoOpen, trainOpen;
void casinoOpenNow(); void casinoDbgSkipDoor(); void casinoClose();
void casLobbyBtn(uint8_t i, int *x, int *y, int *w, int *h);
void casPinballPlayBtn(int *x, int *y, int *w, int *h);
const Pinball &casPinball(); Pinball &casPinballMut();
uint8_t casinoView(); bool casinoPinballLive();
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static void pump(int n){ for(int i=0;i<n;i++){ std::this_thread::sleep_for(std::chrono::milliseconds(25)); loop(); } }
static void press(int x, int y){ g_touchX=x; g_touchY=y; g_touchDown=true; emuFireInterrupt(); pump(4); }
static void release(){ g_touchDown=false; emuFireInterrupt(); pump(3); }
static void tapPlay(){ int x,y,w,h; casPinballPlayBtn(&x,&y,&w,&h); onTap(x+w/2, y+h/2); render(); }

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(25, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.sleeping = false;

  casinoOpenNow(); casinoDbgSkipDoor(); render();
  { int x,y,w,h; casLobbyBtn(6,&x,&y,&w,&h); onTap(x+w/2, y+h/2); render(); }
  ck(casinoView() == 7 && !strcmp(SCREEN_NAME[uiCurrentScreen()], "casino"), "the lobby's PINBALL button opens the table");

  // ---- no coins, no game
  pet.coins = PB_PRICE - 1;
  tapPlay();
  ck(casPinball().state != PB_ROLL && casPinball().state != PB_SERVE && pet.coins == PB_PRICE - 1, "PLAY is refused without enough coins, and takes none");
  pet.coins = 100;
  tapPlay();
  ck(casPinball().state == PB_SERVE && pet.coins == 100 - PB_PRICE && casPinball().balls == PB_BALLS, "PLAY takes the coins and starts three balls");
  pump(40);
  ck(casPinball().state == PB_ROLL, "and the ball serves itself");

  // ---- the flippers, through the touch path
  press(120, 300);
  ck(casPinball().upL && !casPinball().upR, "a finger held on the LEFT half raises the left flipper only");
  pump(4);
  ck(casPinball().aL < PB_REST_A - 0.3f, "and it swings up");
  release();
  ck(!casPinball().upL, "letting go drops it");
  press(350, 300);
  ck(casPinball().upR && !casPinball().upL, "the RIGHT half raises the right one");
  release();

  // ---- a swipe does not leave while a ball rolls
  casPinballMut().bx = 233; casPinballMut().by = 150; casPinballMut().vx = 0; casPinballMut().vy = 0;
  onSwipeV(1); render();
  ck(casinoOpen && casinoView() == 7 && casPinball().state == PB_ROLL, "a swipe down mid-ball does not leave the table");

  // ---- a game that ends pays for its score, and the best is kept
  Pinball &p = casPinballMut();
  p.score = 9500; p.balls = 1;
  p.bx = PB_CX; p.by = PB_DRAIN_Y + 2; p.vx = 0; p.vy = 200;
  uint16_t before = pet.coins;
  pump(4);
  ck(casPinball().state == PB_OVER, "the last ball drains: the game is over");
  ck(pet.coins == before + pinballPayout(9500) && pinballPayout(9500) == 25, "and it pays for 9500 points: 25 coins");
  pump(4);
  ck(pet.coins == before + 25, "once, not every frame");
  casinoBegin();
  ck(casino.pinBest == 9500, "the best score is saved");

  // ---- leaving between balls pays the score so far
  tapPlay(); pump(2);
  ck(casPinball().state == PB_SERVE, "a new game");
  casPinballMut().score = 5200;
  before = pet.coins;
  onSwipeV(1); render();
  ck(casinoView() == 0 && pet.coins == before + pinballPayout(5200) && pinballPayout(5200) > 0, "a swipe down between balls goes back to the lobby, paying for 5200 points");
  ck(casino.pinBest == 9500, "a lower score does not replace the best");
  // ---- and closing the Game Corner mid-ball does too
  { int x,y,w,h; casLobbyBtn(6,&x,&y,&w,&h); onTap(x+w/2, y+h/2); render(); }
  tapPlay(); pump(40);
  casPinballMut().score = 30000;
  before = pet.coins;
  casinoClose();
  ck(pet.coins == before + pinballPayout(30000) && casino.pinBest == 30000 && !casinoPinballLive(), "closing the Game Corner mid-ball pays for the score and ends it");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
