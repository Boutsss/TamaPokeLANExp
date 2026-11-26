// The avatar picker through the REAL screens: tapping the avatar on the player
// card opens a grid of every UNLOCKED avatar (rankAvatarUnlocked, the card's own
// rule -- never a locked one), a tap wears the one touched and returns to the
// card, down walks back a level at a time, every cell is finger-sized and on the
// glass, and the tower's legendary-costume count is hidden until one is owned.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "casino.h"
#include "rank.h"
#include "avatar_names.h"
#include "species.h"
#include <cstdio>
#include <cstring>
#include <cmath>
uint32_t g_seed=103; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void loop(); void render(); void onTap(int16_t,int16_t); void onSwipeV(int); void onRim(int);
uint8_t uiCurrentScreen(); uint16_t uiRimPages();
extern const char *const SCREEN_NAME[];
extern Pet pet; extern bool playerOpen, avPickOpen; extern uint8_t playerPage, avPickPage;
uint16_t avPickList(uint16_t *out, uint16_t cap);
void avPickCell(uint8_t i, int *x, int *y);
void uiTileGo(int i);
int uiTapFinger();   // asked of the firmware, never retyped
extern Arduino_Canvas *gfx;
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static bool onScreen(const char *n){ return !strcmp(SCREEN_NAME[uiCurrentScreen()], n); }
static bool rowHas(int y0, int y1, uint16_t c) {
  for (int y = y0; y < y1; y++) for (int x = 100; x < 366; x++) if (gfx->buffer()[y * 466 + x] == c) return true;
  return false;
}
#define TILE_PLAYER 0

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(25, false);
  while (pet.hasLearnOffer()) pet.declineLearn();

  // a few shop avatars owned, so the list is more than the free set
  casinoBegin();
  uint32_t c = 1000000;
  static uint32_t wallet; wallet = c;
  auto spend = [](uint32_t n){ return true; };
  casinoBuy(PK_AVATAR, 0, c, spend);
  casinoBuy(PK_AVATAR, 5, c, spend);

  uiTileGo(TILE_PLAYER); render();
  ck(onScreen("player") && playerPage == 0, "on the player card");
  onTap(233, 100); render();                       // the avatar
  ck(onScreen("avpick") && avPickOpen, "tapping the avatar opens the picker");

  static uint16_t list[AVATAR_COUNT];
  uint16_t n = avPickList(list, AVATAR_COUNT);
  int unlocked = 0; bool onlyUnlocked = true;
  for (uint16_t a = 0; a < AVATAR_COUNT; a++) if (rankAvatarUnlocked(a)) unlocked++;
  for (uint16_t k = 0; k < n; k++) if (!rankAvatarUnlocked(list[k])) onlyUnlocked = false;
  ck(n == unlocked && onlyUnlocked && n > AVATAR_BASE_COUNT / 2, "it lists exactly the unlocked avatars, never a locked one");
  bool hasShop = false;
  for (uint16_t k = 0; k < n; k++) if (list[k] == SHOP_AVATAR_FIRST + 5) hasShop = true;
  ck(hasShop, "including one bought at the Game Corner");

  // every cell finger-sized and inside the glass
  bool fits = true;
  for (uint8_t i = 0; i < 9; i++) {
    int x, y; avPickCell(i, &x, &y);
    int cs[4][2] = { { x, y }, { x + uiTapFinger(), y }, { x, y + uiTapFinger() }, { x + uiTapFinger(), y + uiTapFinger() } };
    for (auto &p : cs) if (hypot(p[0] - 233, p[1] - 233) > 230) fits = false;
  }
  { int x0, y0, x1, y1, x3, y3; avPickCell(0, &x0, &y0); avPickCell(1, &x1, &y1); avPickCell(3, &x3, &y3);
    ck(x1 - x0 >= uiTapFinger() && y3 - y0 >= uiTapFinger(), "the cells are laid out at least a finger apart (measured, not assumed)"); }
  ck(fits, "every cell is finger-sized and its corners stay on the glass");

  // a tap wears it
  uint16_t want = list[1 % n];
  int x, y; avPickCell(1, &x, &y);
  avPickPage = 0; render();
  onTap(x + 40, y + 40); render();
  ck(pet.avatar == want && onScreen("player") && !avPickOpen, "a tap wears the avatar touched and returns to the card");
  { Pet q; q.begin(); ck(q.avatar == want, "and it is saved"); }

  // down: picker -> card -> pet
  onTap(233, 100); render();
  onSwipeV(1); render();
  ck(onScreen("player"), "down from the picker is the card");
  onSwipeV(1); render();
  ck(onScreen("main"), "and down again is the pet");

  // the legendary-costume count: absent until one is owned
  const uint16_t PURPLE = 0x895F;
  uiTileGo(TILE_PLAYER); onTap(233, 100); render();
  ck(!rowHas(58, 72, PURPLE), "no legendary-costume count while none is owned");
  casinoGrantAvatar(TOWER_AVATARS[0]);
  render();
  ck(rowHas(58, 72, PURPLE), "and it appears once one is");
  uint16_t n2 = avPickList(nullptr, 0);
  ck(n2 == n + 1, "the costume joins the list");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
