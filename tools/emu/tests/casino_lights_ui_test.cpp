// The Game Corner's marquee, read off the PANEL: a ring of bulbs that chases
// slowly (one in four lit, moving on), that a player WIN lights all together for
// a moment, that a loss leaves alone, and that settles back to the chase. Every
// position comes from casBulbPos() and every lit/dim answer from casBulbLit(),
// the same functions the renderer draws with.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "casino.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
uint32_t g_seed=79; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void loop(); void render(); void onTap(int16_t,int16_t);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
extern Pet pet; extern bool casinoOpen;
void casLobbyBtn(uint8_t i, int *x, int *y, int *w, int *h);
void casTab(uint8_t i, int *x, int *y, int *w, int *h);
void casTile(uint8_t i, int *x, int *y, int *w, int *h);
void casBulbPos(uint8_t i, int *x, int *y);
uint8_t casBulbLit(uint8_t i, uint32_t now);
bool casLightsFlashing();
extern Arduino_Canvas *gfx;
static uint16_t px(int x, int y) { return gfx->buffer()[y * 466 + x]; }
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static void pump(int n){ for(int i=0;i<n;i++){ std::this_thread::sleep_for(std::chrono::milliseconds(30)); loop(); } }
typedef void (*Geo)(uint8_t, int*, int*, int*, int*);
static void tapGeo(Geo g, uint8_t i){ int x,y,w,h; g(i,&x,&y,&w,&h); onTap(x+w/2, y+h/2); }

#define NB 40
static const uint16_t HOT = 0xFFF6;   // a lit bulb's centre (casMarquee)
// Render, then read which bulbs the PANEL shows lit. Retried until the render
// fell inside one animation step, so the frame and `when` agree.
static int panelLit(bool lit[NB], uint32_t *when) {
  for (int tries = 0; tries < 50; tries++) {
    uint32_t a = millis(); render(); uint32_t b = millis();
    if (a / 120 != b / 120 || a / 140 != b / 140) continue;
    int n = 0;
    for (uint8_t i = 0; i < NB; i++) { int x, y; casBulbPos(i, &x, &y); lit[i] = px(x - 1, y - 1) == HOT; n += lit[i]; }
    *when = a;
    return n;
  }
  return -1;
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(25, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.sleeping = false;
  casinoOpen = true; render(); pump(60);
  ck(!strcmp(SCREEN_NAME[uiCurrentScreen()], "casino"), "in the Game Corner lobby");

  // ---- the chase
  bool lit[NB]; uint32_t t0;
  int n = panelLit(lit, &t0);
  bool agrees = n >= 0;
  for (uint8_t i = 0; i < NB && agrees; i++) if (lit[i] != (bool)casBulbLit(i, t0)) agrees = false;
  ck(agrees, "every bulb on the panel is lit exactly when casBulbLit says");
  ck(n == NB / 4, "at rest one bulb in four is lit -- a slow chase, not a strobe");
  ck(!casLightsFlashing(), "and nothing is flashing before anyone has won");
  bool moved = false;
  for (int k = 0; k < 10 && !moved; k++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    bool lit2[NB]; uint32_t t1;
    if (panelLit(lit2, &t1) == NB / 4 && t1 / 140 != t0 / 140) moved = memcmp(lit, lit2, sizeof(lit)) != 0;
  }
  ck(moved, "and the lit bulbs move round the ring");

  // ---- a loss does not flash
  pet.coins = 0;
  tapGeo(casLobbyBtn, 0);                    // SLOTS
  { int x, y, w, h; extern void casBtn(uint8_t, uint8_t, int*, int*, int*, int*); casBtn(0, 1, &x, &y, &w, &h); onTap(x + w / 2, y + h / 2); }
  ck(!casLightsFlashing(), "a refused bet (no coins) does not light the marquee");

  // ---- a win: buy a prize, which the dealer grumbles at
  extern void onSwipeV(int); onSwipeV(1); pump(2);   // back to the lobby
  tapGeo(casLobbyBtn, 5); tapGeo(casTab, PK_MON);
  pet.coins = 1500000;
  tapGeo(casTile, 0); tapGeo(casTile, 0);
  ck(casLightsFlashing(), "a win lights the marquee");
  onSwipeV(1);   // back to the lobby, where every bulb is in view (proved above)
  int allOn = 0, allOff = 0, mixed = 0;
  for (int k = 0; k < 12; k++) {
    uint32_t t; bool l[NB];
    int m = panelLit(l, &t);
    if (!casLightsFlashing()) break;
    if (m == NB) allOn++; else if (m == 0) allOff++; else mixed++;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ck(allOn > 0 && allOff > 0 && mixed == 0, "while it flashes every bulb blinks together, on and off");
  std::this_thread::sleep_for(std::chrono::milliseconds(1600));
  { uint32_t t; bool l[NB]; int m = panelLit(l, &t);
    ck(!casLightsFlashing() && m == NB / 4, "and after a moment it settles back to the chase"); }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
