// Switching in battle with a FULL team of six, through the real screen.
//
// The bug: the switch list reused the move grid's four cells and drew squad
// slots 0..3 only -- the active one among them -- so with six in the squad at
// most three choices showed and slots 4 and 5 could never be sent in. This
// asserts every other member is offered, each in its own cell ON THE PANEL
// (pixels, not just a list), and that the last one can actually be chosen.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "trainers.h"
#include "species.h"   // UI_BG_DAY
#include <cstdio>
#include <cstring>
uint32_t g_seed=43; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void battleTap(int16_t,int16_t);
void startTrainerBattle(uint8_t idx, bool hard);
extern Pet pet; extern Party party; extern Arduino_Canvas *gfx;
extern bool battleOpen; extern uint16_t squadMask;
extern uint8_t btlSquadN, btlSquadAt, btlMenu;
extern Combatant btlSquad[];
uint8_t btlSwitchList(uint8_t *out);
void btlSwitchCellPos(uint8_t pos, int *x, int *y);
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static uint16_t px(int x, int y) { return gfx->buffer()[y * 466 + x]; }

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(6, false);
  pet.ageMinutes = 60UL * MINUTES_PER_LEVEL; pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  // the live pet plus five banked: a squad of six
  const int16_t dex[5] = { 9, 3, 25, 143, 131 };
  for (int i = 0; i < 5; i++) {
    PartyMon m; m.dex = dex[i]; m.level = 50; m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 25;
    party.replaceAt(i, m);
  }
  party.releaseAt(5);
  squadMask = 0xFFFF;
  startTrainerBattle(0, false);
  ck(battleOpen && btlSquadN == 6, "a fight with a squad of six");

  btlMenu = 2; render();
  uint8_t list[TRAINER_TEAM_MAX];
  uint8_t n = btlSwitchList(list);
  ck(n == 5, "the switch list offers all FIVE others");
  bool hasActive = false;
  for (uint8_t k = 0; k < n; k++) if (list[k] == btlSquadAt) hasActive = true;
  ck(!hasActive, "and not the one already fighting");
  bool allDrawn = true;
  for (uint8_t k = 0; k < n; k++) {
    int x, y; btlSwitchCellPos(k, &x, &y);
    if (x < 30 || x > 436 || y < 100 || y > 380) allDrawn = false;
    if (px(x + 3, y + 30) != UI_BG_DAY) { allDrawn = false; printf("      cell %u not drawn\n", k); }
  }
  ck(allDrawn, "each one has its own cell on the panel");
  bool distinct = true;
  for (uint8_t a = 0; a < n; a++) for (uint8_t b = a + 1; b < n; b++) {
    int xa, ya, xb, yb; btlSwitchCellPos(a, &xa, &ya); btlSwitchCellPos(b, &xb, &yb);
    if (xa == xb && ya == yb) distinct = false;
  }
  ck(distinct, "no two share a cell");

  // send in the LAST one -- the slot the old list could never reach
  int x, y; btlSwitchCellPos(n - 1, &x, &y);
  battleTap(x + 30, y + 30);
  ck(btlSquadAt == list[n - 1] && btlSquadAt == 5, "the sixth member can be sent in");

  // a fainted member is listed but refused
  btlSquad[1].hp = 0;
  btlMenu = 2; render();
  n = btlSwitchList(list);
  int pos = -1;
  for (uint8_t k = 0; k < n; k++) if (list[k] == 1) pos = k;
  uint8_t before = btlSquadAt;
  btlSwitchCellPos(pos, &x, &y);
  battleTap(x + 30, y + 30);
  ck(pos >= 0 && btlSquadAt == before, "a fainted one is shown but cannot be sent in");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
