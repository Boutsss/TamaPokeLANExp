// The v3.37 battle field, through the real screen rather than the engine alone:
// a fight started from the UI resets the field and seats each side on its own
// half of it (or the player's REFLECT would shield the foe), a trapped creature
// cannot be switched out from the menu, and a creature switched in takes the
// hazards waiting on its side.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "trainers.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=47; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void battleTap(int16_t,int16_t);
void startTrainerBattle(uint8_t idx, bool hard);
extern Pet pet; extern Party party;
extern bool battleOpen; extern uint16_t squadMask;
extern uint8_t btlSquadN, btlSquadAt, btlMenu, btlMsgCount;
extern Combatant btlSquad[], btlYou, btlFoe;
uint8_t btlSwitchList(uint8_t *out);
void btlSwitchCellPos(uint8_t pos, int *x, int *y);
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static bool tapSwitchTo(uint8_t member) {
  btlMsgCount = 0; btlMenu = 2; render();
  uint8_t list[TRAINER_TEAM_MAX]; uint8_t n = btlSwitchList(list);
  for (uint8_t k = 0; k < n; k++) if (list[k] == member) {
    int x, y; btlSwitchCellPos(k, &x, &y); battleTap(x + 30, y + 30); return true;
  }
  return false;
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(6, false);
  pet.ageMinutes = 60UL * MINUTES_PER_LEVEL; pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  const int16_t dex[2] = { 9, 143 };
  for (int i = 0; i < 2; i++) {
    PartyMon m; m.dex = dex[i]; m.level = 50; m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 25;
    party.replaceAt(i, m);
  }
  for (int i = 2; i < PARTY_SLOTS; i++) party.releaseAt(i);
  squadMask = 0xFFFF;

  gField.weather = W_RAIN; gField.side[0].rocks = 1;          // left over from "an earlier fight"
  startTrainerBattle(0, false);
  ck(battleOpen && btlSquadN == 3, "a fight with a squad of three");
  ck(gField.weather == W_NONE && !gField.side[0].rocks, "every fight starts on an empty field");
  ck(btlYou.side == 0 && btlFoe.side == 1, "the player on side 0, the opponent on side 1");

  btlYou.trapped = true;
  uint8_t before = btlSquadAt;
  ck(tapSwitchTo(1) && btlSquadAt == before, "a trapped creature cannot be switched out from the menu");
  ck(btlMsgCount > 0, "and the player is told why");

  btlYou.trapped = false;
  gField.side[0].rocks = 1;
  ck(tapSwitchTo(1) && btlSquadAt == 1, "free again, the switch goes through");
  ck(btlYou.dex == 9 && btlYou.maxHp - btlYou.hp >= btlYou.maxHp / 8 - 1,
     "and the creature sent in takes STEALTH ROCK on arrival (an eighth, neutral to Rock)");
  ck(btlYou.side == 0, "still seated on the player's side");
  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
