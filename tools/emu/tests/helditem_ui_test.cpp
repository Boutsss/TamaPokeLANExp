// (v3.38) A held item used up IN battle -- a berry eaten, a FOCUS SASH spent --
// is back on its creature when the fight is over. Played through the real
// screen: a real turn from the move grid, then the stored creature (and the save
// on flash) is checked, not the battle's copy.
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
uint32_t g_seed=53; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
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

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(6, false);
  pet.ageMinutes = 50UL * MINUTES_PER_LEVEL; pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.held = IT_ORAN_BERRY; pet.saveNow();
  for (int i = 0; i < PARTY_SLOTS; i++) party.releaseAt(i);
  squadMask = 0xFFFF;
  startTrainerBattle(0, false);
  ck(battleOpen && btlYou.held == IT_ORAN_BERRY, "the live CHARIZARD goes in holding an ORAN BERRY");

  // at half HP or below, the berry goes the moment the turn checks it
  btlYou.hp = btlYou.maxHp / 2 - 1;
  btlMsgCount = 0; btlMenu = 0; render();
  battleTap(btlCellCenterX(0), btlCellCenterY(0));   // FIGHT
  render();
  battleTap(btlCellCenterX(0), btlCellCenterY(0));   // the first move
  ck(btlYou.itemUsed, "a real turn played, and the berry was eaten in it");
  ck(pet.held == IT_ORAN_BERRY, "the creature still holds its ORAN BERRY afterwards");
  Pet reload; reload.begin();
  ck(reload.held == IT_ORAN_BERRY, "and so does the save on flash");
  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
