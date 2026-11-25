// (v3.40) The moves that switch: the engine asks, the battle screen performs.
// Played through the real screen: U-TURN pauses the turn for the player to pick
// who comes in, ROAR drags a random teammate out (and ends a wild fight), the
// dragged-out trainer creature keeps its damage for later, BATON PASS hands the
// boosts on, and a faint is replaced by ANY healthy teammate, not only a later one.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=59; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void battleTap(int16_t,int16_t);
void startTrainerBattle(uint8_t idx, bool hard);
bool startWildBattle(uint8_t region, bool hard);
int btlCellCenterX(int i); int btlCellCenterY(int i);
uint8_t btlSwitchList(uint8_t *out);
void btlSwitchCellPos(uint8_t pos, int *x, int *y);
int btlNextHealthy(uint8_t side);
void btlBackCenter(int *x, int *y);
extern Pet pet; extern Party party;
extern bool battleOpen, btlOver, btlWon, btlPickOut; extern uint16_t squadMask;
extern uint8_t btlMenu, btlMsgCount, btlSquadN, btlSquadAt, btlFoeAt, btlFoeSquadN;
extern Combatant btlYou, btlFoe, btlSquad[], btlFoeSquad[];
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static void useMove(uint16_t mv) {               // FIGHT, then the first cell, holding `mv`
  btlYou.moves[0] = mv;
  btlMsgCount = 0; btlMenu = 0; render();
  battleTap(btlCellCenterX(0), btlCellCenterY(0));
  render();
  battleTap(btlCellCenterX(0), btlCellCenterY(0));
}
static bool pickMember(uint8_t member) {
  btlMsgCount = 0; render();
  uint8_t list[TRAINER_TEAM_MAX]; uint8_t n = btlSwitchList(list);
  for (uint8_t k = 0; k < n; k++) if (list[k] == member) {
    int x, y; btlSwitchCellPos(k, &x, &y); battleTap(x + 30, y + 30); return true;
  }
  return false;
}
static void freshTrainerFight() {
  pet.dbgHatchAs(6, false);
  pet.ageMinutes = 70UL * MINUTES_PER_LEVEL; pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  const int16_t dex[2] = { 9, 143 };
  for (int i = 0; i < PARTY_SLOTS; i++) party.releaseAt(i);
  for (int i = 0; i < 2; i++) {
    PartyMon m; m.dex = dex[i]; m.level = 50; m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 25;
    party.replaceAt(i, m);
  }
  squadMask = 0xFFFF;
  startTrainerBattle(0, false);
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // ---- U-TURN: the turn waits for the player's pick
  freshTrainerFight();
  ck(battleOpen && btlSquadN == 3 && btlFoeSquadN >= 2, "a trainer fight, both teams held as squads");
  useMove(MV_U_TURN);
  ck(btlPickOut && btlMenu == 2, "U-TURN hits, then the switch screen opens and waits");
  { btlMsgCount = 0; render(); int bx, by; btlBackCenter(&bx, &by); battleTap(bx, by); }   // the real BACK button
  ck(btlPickOut && btlMenu == 2, "and BACK cannot back out of it");
  battleTap(10, 233);                                   // nor can a miss anywhere else
  ck(btlPickOut && btlMenu == 2, "nor a tap off the cells");
  ck(pickMember(2) && btlSquadAt == 2 && !btlPickOut && btlMenu == 0, "the picked teammate comes in and the turn goes on");

  // ---- BATON PASS: the boosts go with it
  freshTrainerFight();
  btlYou.stage[SI_ATK] = 2; btlYou.stage[SI_SPE] = 1;
  useMove(MV_BATON_PASS);
  ck(btlPickOut, "BATON PASS asks who to pass to");
  ck(pickMember(1) && btlSquadAt == 1 && btlYou.stage[SI_ATK] == 2 && btlYou.stage[SI_SPE] == 1,
     "and the one sent in inherits ATK +2 and SPD +1");

  // ---- ROAR on a trainer: a random teammate, and the dragged one keeps its damage
  freshTrainerFight();
  btlFoe.hp = btlFoe.maxHp / 2;
  uint8_t was = btlFoeAt; uint16_t hurtHp = btlFoe.hp;
  useMove(MV_ROAR);
  ck(btlFoeAt != was, "ROAR drags the trainer's creature out for another");
  ck(btlFoeSquad[was].hp == hurtHp, "and the one dragged out keeps its damage for later");

  // ---- ROAR in the wild ends the fight
  pet.dbgHatchAs(6, false); pet.ageMinutes = 70UL * MINUTES_PER_LEVEL; pet.relearnFromLevel();
  if (startWildBattle(1, false)) {
    useMove(MV_ROAR);
    ck(btlOver && !btlWon, "ROAR in the wild blows the creature away: the fight is over, no win");
  } else ck(false, "a wild battle starts");

  // ---- a faint is replaced by ANY healthy teammate
  freshTrainerFight();
  btlSquadAt = 2; btlYou = btlSquad[2];
  ck(btlNextHealthy(0) >= 0 && btlNextHealthy(0) != 2,
     "the LAST teammate out still has the earlier ones to fall back on");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
