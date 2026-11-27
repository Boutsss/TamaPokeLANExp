// (v3.57) The battle screen greys a move the turn would refuse -- a CHOICE item
// locked into another, a status move under TAUNT or an ASSAULT VEST -- and a tap
// on it is refused rather than played. Both ask battleMoveBlocked(), so this
// drives the real screen: a real Choice-locked turn, then a tap on a greyed move.
// Also ARCEUS's plate type: the plate retypes it, and that is what the fight sees.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "items.h"
#include "held_items.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=61; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void battleTap(int16_t,int16_t);
void startTrainerBattle(uint8_t idx, bool hard);
int btlCellCenterX(int i); int btlCellCenterY(int i);
extern Pet pet; extern Party party;
extern bool battleOpen; extern uint16_t squadMask;
extern uint8_t btlMenu, btlMsgCount;
extern Combatant btlYou;
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static int firstStatus(const Combatant &c) {
  for (int i = 0; i < MOVE_SLOTS; i++) if (c.moves[i] && MOVE_TBL[c.moves[i]].cat == MC_STATUS) return i;
  return -1;
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(6, false);
  pet.ageMinutes = 50UL * MINUTES_PER_LEVEL; pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.held = IT_CHOICE_BAND; pet.saveNow();
  for (int i = 0; i < PARTY_SLOTS; i++) party.releaseAt(i);
  squadMask = 0xFFFF;
  startTrainerBattle(0, false);
  ck(battleOpen && btlYou.held == IT_CHOICE_BAND, "CHARIZARD goes in holding a CHOICE BAND");
  int n = 0; for (int i = 0; i < MOVE_SLOTS; i++) if (btlYou.moves[i]) n++;
  ck(n >= 2, "with at least two moves to choose between");
  bool anyBlocked = false;
  for (int i = 0; i < MOVE_SLOTS; i++) if (btlYou.moves[i] && battleMoveBlocked(btlYou, btlYou.moves[i])) anyBlocked = true;
  ck(!anyBlocked, "before it has moved, nothing is locked");

  btlYou.hp = btlYou.maxHp; btlMsgCount = 0; btlMenu = 0; render();
  battleTap(btlCellCenterX(0), btlCellCenterY(0));   // FIGHT
  render();
  uint16_t first = btlYou.moves[0];
  battleTap(btlCellCenterX(0), btlCellCenterY(0));   // the first move
  ck(btlYou.choiceLock == first, "a real turn played, and the band locked it into that move");
  ck(!battleMoveBlocked(btlYou, first), "the locked move stays usable");
  ck(battleMoveBlocked(btlYou, btlYou.moves[1]), "every other move is blocked");

  // back to the grid, then tap the greyed one
  for (int k = 0; k < 40 && btlMsgCount; k++) { battleTap(233, 233); render(); }
  ck(battleOpen && btlMsgCount == 0, "the turn's messages are read and the fight goes on");
  btlMenu = 0; render();
  battleTap(btlCellCenterX(0), btlCellCenterY(0));   // FIGHT
  render();
  uint8_t menuBefore = btlMenu;
  uint16_t hpFoeTurn = btlYou.hp;
  battleTap(btlCellCenterX(1), btlCellCenterY(1));   // the greyed second move
  ck(btlMenu == menuBefore && btlMenu != 0, "a tap on a greyed move leaves the move grid open");
  ck(btlMsgCount >= 1, "and says why");
  ck(btlYou.hp == hpFoeTurn, "no turn was played for it");

  // TAUNT and ASSAULT VEST: the status-move half of the rule
  Combatant c = btlYou; c.choiceLock = 0; c.held = ITEM_NONE; c.tauntTurns = 0;
  int s = firstStatus(c);
  if (s < 0) { c.moves[3] = 0; for (uint16_t m = 1; m < MOVE_COUNT; m++) if (MOVE_TBL[m].cat == MC_STATUS) { c.moves[3] = m; break; } s = 3; }
  ck(!battleMoveBlocked(c, c.moves[s]), "a status move is usable with nothing in the way");
  c.tauntTurns = 2;
  ck(battleMoveBlocked(c, c.moves[s]), "TAUNT blocks it");
  c.tauntTurns = 0; c.held = IT_ASSAULT_VEST;
  ck(battleMoveBlocked(c, c.moves[s]), "so does an ASSAULT VEST");

  // ARCEUS and a plate
  Pet a; a.dbgHatchAs(493, false); a.ageMinutes = 60UL * MINUTES_PER_LEVEL; a.relearnFromLevel();
  a.held = IT_ZAP_PLATE;
  Combatant ac; combatantFromPet(ac, a); battleHeldSetup(ac);
  ck(ac.typeOver == T_ELECTRIC, "a ZAP PLATE makes ARCEUS ELECTRIC for the fight (what its plate chip shows)");
  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
