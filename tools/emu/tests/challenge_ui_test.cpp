// The weekly challenge through the real screens: the chooser's WEEKLY, START,
// the picker enforcing the week's RULE (refusing a creature, refusing an
// illegal squad at FIGHT), the fight, a win paying ONCE a week, and NO ITEMS
// really switching held items off. The draw is deterministic per week, so the
// test walks forward week by week until it reaches the rule it wants.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "items.h"
#include "inventory.h"
#include "battle.h"
#include "challenge.h"
#include <cstdio>
uint32_t g_seed=37; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void onTap(int16_t,int16_t); void battleTap(int16_t,int16_t);
void uiTileGo(int i);
extern Pet pet;
extern bool gymOpen, gymPick, chalOpen, pickOpen, battleOpen;
extern uint8_t btlKind, btlTeamN, btlSquadN, btlMsgCount;
extern uint16_t squadMask;
extern Combatant btlFoe, btlYou, btlSquad[];
extern uint32_t btlWinUntil;
int chBtnCenterY(); int btlCellCenterX(int i); int btlCellCenterY(int i); int pickGoX(); int pickGoY();
bool pickAllows(uint8_t n);
int uiChooserBtnX(int i); int uiChooserBtnY();
static int bad=0;
static void ck(bool ok,const char *s){printf("%s  %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}

static bool winFight() {
  for (int g = 0; g < 400 && battleOpen && !btlWinUntil; g++) {
    if (btlMsgCount) { battleTap(233, 300); render(); continue; }
    btlFoe.hp = 1; btlYou.hp = btlYou.maxHp;
    battleTap(btlCellCenterX(0), btlCellCenterY(0)); render();
  }
  return btlWinUntil != 0;
}
// Walks the clock forward a week at a time until the challenge has `rule`.
static bool weekWith(uint8_t rule) {
  for (int w = 0; w < 400; w++) {
    pet.lastSeenEpoch += 7UL * 86400UL;
    if (challengeRefreshFor(pet, party, pet.currentDay()) && challengeCurrent().rule == rule &&
        challengeTeamDrawable(challengeCurrent())) return true;
  }
  return false;
}
static void openWeekly() {
  uiTileGo(6); render();
  onTap(uiChooserBtnX(2), uiChooserBtnY()); render();
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(6, false);
  pet.ageMinutes = 60UL * 40;
  while (pet.hasLearnOffer()) pet.declineLearn();
  for (int i=0;i<PARTY_SLOTS;i++) party.releaseAt(i);
  const int16_t mons[3] = { 7, 25, 1 };   // water, electric, grass/poison
  for (int i=0;i<3;i++){ Pet t; t.dbgHatchAs(mons[i], false); t.ageMinutes = 60UL * 39; t.relearnFromLevel();
    while (t.hasLearnOffer()) t.declineLearn(); party.replaceAt(i, t.toPartyMon()); }
  challengeClearAll();
  pet.lastSeenEpoch = 20000UL * 86400UL;

  openWeekly();
  ck(chalOpen, "WEEKLY opens the challenge screen");
  onTap(233, chBtnCenterY()); render();
  ck(chalOpen && !pickOpen, "locked without a badge: START does nothing");
  pet.winBadge(0, 0, false);

  // ---- SOLO: the picker allows exactly one, FIGHT refuses two
  ck(weekWith(CR_SOLO), "a SOLO week comes round");
  openWeekly();
  onTap(233, chBtnCenterY()); render();
  ck(pickOpen, "START opens the picker");
  squadMask = 0b11;      // the pet AND the first party member
  onTap(pickGoX(), pickGoY()); render();
  ck(pickOpen && !battleOpen, "FIGHT with two on a SOLO week is refused");
  squadMask = 0b1;
  onTap(pickGoX(), pickGoY()); render();
  ck(battleOpen && btlKind == 3 /*BK_CHALLENGE*/ && btlSquadN == 1 && btlTeamN == 1,
     "with one it opens a 1 vs 1 challenge against the leader's ace");
  uint8_t ub = bag.count(IT_ULTRABALL);
  ck(winFight(), "won");
  ck(challengeDone() && bag.count(IT_ULTRABALL) == ub + CHAL_BALLS, "the week is cleared and pays 3 ULTRA BALL");
  battleTap(233, 380); render();
  ck(!battleOpen && chalOpen, "dismissing returns to the challenge screen");
  onTap(233, chBtnCenterY()); render();
  squadMask = 0b1;
  onTap(pickGoX(), pickGoY()); render();
  ck(battleOpen, "a cleared week can be fought again");
  winFight();
  ck(bag.count(IT_ULTRABALL) == ub + CHAL_BALLS, "but pays nothing the second time");
  battleTap(233, 380); render();

  // ---- TYPE: a creature of the wrong type cannot even be selected
  ck(weekWith(CR_TYPE), "a TYPE week comes round");
  openWeekly();
  onTap(233, chBtnCenterY()); render();
  bool someRefused = false, someAllowed = false;
  for (uint8_t n = 0; n <= 3; n++) { if (pickAllows(n)) someAllowed = true; else someRefused = true; }
  ck(someAllowed && someRefused, "the rule lets some of ours in and keeps others out");
  bool defaultClean = true;
  for (uint8_t n = 0; n <= 3; n++) if ((squadMask & (1 << n)) && !pickAllows(n)) defaultClean = false;
  ck(defaultClean, "the default selection holds no one the rule refuses");
  uint8_t refused = 0; for (uint8_t n = 0; n <= 3; n++) if (!pickAllows(n)) { refused = n; break; }
  squadMask |= (uint16_t)(1 << refused);
  onTap(pickGoX(), pickGoY()); render();
  ck(pickOpen && !battleOpen, "FIGHT with a wrong-type creature forced in is refused");
  pickOpen = false; render();

  // ---- NO ITEMS: held items do nothing, and are never spent
  ck(weekWith(CR_NOITEMS), "a NO ITEMS week comes round");
  bag.add(IT_FOCUS_SASH, 1);
  extern bool heldGive(Pet &, Party &, uint16_t, ItemKey);
  ck(heldGive(pet, party, 0, IT_FOCUS_SASH) && pet.held == IT_FOCUS_SASH, "the pet holds a FOCUS SASH");
  openWeekly();
  onTap(233, chBtnCenterY()); render();
  squadMask = 0b1;
  onTap(pickGoX(), pickGoY()); render();
  ck(battleOpen && btlYou.held == ITEM_NONE, "in a NO ITEMS fight our side holds nothing");
  winFight();
  battleTap(233, 380); render();
  ck(pet.held == IT_FOCUS_SASH, "and the sash is still the pet's afterwards");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
