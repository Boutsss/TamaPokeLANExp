// The Battle Tower and gym rematches through the REAL screens and the REAL
// battle: the chooser's TOWER button, START, the team picker capped at three,
// a fight against exactly the team the tower showed, and what a win and a loss
// do. Then the ladder's REMATCH state: hidden until the champion falls, then a
// stronger team, a reward, and no badge touched. Coordinates are asked of the
// firmware (btlCellCenterX(), twBtnCenterY()...), never restated here.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "items.h"
#include "inventory.h"
#include "battle.h"
#include "tower.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=31; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void onTap(int16_t,int16_t); void onSwipeV(int dir);
void battleTap(int16_t,int16_t);
int uiTileIndex(); void uiTileGo(int i);
extern Pet pet;
extern bool gymOpen, gymPick, towerOpen, pickOpen, battleOpen, gymRematch, gymHard;
extern uint8_t gymRegion, btlKind, btlTeamN, btlFoeAt, btlSquadN, btlMsgCount;
extern TrainerMon btlTeam[];
extern Combatant btlFoe, btlYou;
extern uint32_t btlWinUntil; extern uint8_t btlMenu; extern int8_t btlSwapWho; extern bool btlOver;
int twBtnCenterY(); int btlCellCenterX(int i); int btlCellCenterY(int i);
int uiChooserBtnX(int i); int uiChooserBtnY();
static int bad=0;
static void ck(bool ok,const char *s){printf("%s  %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}

// Wins the fight in front of us through real taps: each turn the foe is left
// on 1 HP and we FIGHT with move 0. Returns once the win screen is up.
static bool winFight() {
  for (int guard = 0; guard < 400 && battleOpen && !btlWinUntil; guard++) {
    if (btlMsgCount) { battleTap(233, 300); render(); continue; }
    btlFoe.hp = 1; btlYou.hp = btlYou.maxHp;
    battleTap(btlCellCenterX(0), btlCellCenterY(0)); render();   // FIGHT (or move 0)
  }
  return btlWinUntil != 0;
}
static bool loseFight() {
  for (int guard = 0; guard < 400 && battleOpen; guard++) {
    if (btlMsgCount) { battleTap(233, 300); render(); continue; }
    btlYou.hp = 1; btlFoe.hp = btlFoe.maxHp;
    battleTap(btlCellCenterX(0), btlCellCenterY(0)); render();
  }
  return !battleOpen;
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(6, false);
  pet.ageMinutes = 60UL * 30;          // level 31: below the tower's 50
  while (pet.hasLearnOffer()) pet.declineLearn();
  for (int i=0;i<PARTY_SLOTS;i++) party.releaseAt(i);
  // Built through a real (unsaved) Pet so each has a real level-40 moveset --
  // a hand-made record with no moves cannot attack, which is not a state the
  // game can reach.
  for (int i=0;i<4;i++){ Pet t; t.dbgHatchAs((int16_t)(9+i*3), false);
    t.ageMinutes = 60UL * 39; t.relearnFromLevel();
    while (t.hasLearnOffer()) t.declineLearn();
    party.replaceAt(i, t.toPartyMon()); }
  towerClearAll();

  // ---- locked: the button opens a screen that says why, and START does nothing
  uiTileGo(6); render();                     // GYM tile -> its chooser
  ck(gymOpen && gymPick, "the GYM tile opens on its chooser");
  onTap(uiChooserBtnX(1), uiChooserBtnY()); render();
  ck(towerOpen, "TOWER opens the tower screen even while locked");
  onTap(233, twBtnCenterY()); render();
  ck(towerOpen && !pickOpen, "but START does nothing until a champion falls");
  onSwipeV(1); render();
  ck(!towerOpen && gymOpen && gymPick, "down goes back to the chooser");

  // ---- unlocked: START -> picker (three) -> a fight against the shown team
  pet.winBadge(0, TRAINER_COUNT - 1, false);
  TowerTeam shown;
  ck(towerTeamFor(towerFloor(), towerSeed(), shown) && shown.count == TOWER_TEAM, "floor 1 has a team of three");
  onTap(uiChooserBtnX(1), uiChooserBtnY()); render();
  onTap(233, twBtnCenterY()); render();
  ck(pickOpen && !towerOpen, "START opens the team picker");
  extern uint8_t pickTrainer;
  uint8_t squadCap(uint8_t idx, bool hard);
  ck(squadCap(pickTrainer, false) == TOWER_TEAM, "capped at three");
  void pickDefault(uint8_t);
  // FIGHT: the picker's GO button, through the real tap path
  extern uint16_t squadMask;
  int pickGoX(); int pickGoY();
  onTap(pickGoX(), pickGoY()); render();
  ck(battleOpen && btlKind == 1 /*BK_TOWER*/, "FIGHT opens a TOWER battle");
  bool same = btlTeamN == shown.count;
  for (uint8_t k = 0; k < btlTeamN && same; k++)
    same = btlTeam[k].dex == shown.mon[k].dex && btlTeam[k].level == TOWER_LEVEL;
  ck(same, "against exactly the team the tower showed, at level 50");
  ck(btlYou.level == TOWER_LEVEL, "and our side is raised to 50 too (pet was level 31)");
  ck(btlSquadN <= TOWER_TEAM, "with at most three of ours");
  ck(winFight(), "winning all three floors' foes reaches the win screen");
  ck(towerStreak() == 1, "a win is one floor: streak 1");
  battleTap(233, 380); render();
  ck(!battleOpen && towerOpen, "and dismissing it returns to the tower, ready to CONTINUE");
  ck(pet.level() < TOWER_LEVEL, "the live pet itself was never raised");

  // ---- a loss ends the run, the best is kept
  // v3.56: the run's team was copied on floor 1, so CONTINUE goes straight to
  // the fight -- no picker, whatever the party has become meanwhile
  onTap(233, twBtnCenterY()); render();
  ck(battleOpen && !pickOpen && towerFloor() == 2, "CONTINUE fights floor 2 with the run's team, no picker");
  ck(loseFight(), "losing ends the fight");
  ck(towerStreak() == 0 && towerBest() == 1, "a loss ends the run; the best is kept");

  // ---- rematches: the pill offers REMATCH only once the region's champion fell
  uiTileGo(6); render();
  gymRegion = 1; gymPick = false; gymHard = false; gymRematch = false; render();
  extern int uiGymPillY();
  onTap(233, uiGymPillY()); onTap(233, uiGymPillY()); render();
  ck(!gymRematch && !gymHard, "JOHTO, champion unbeaten: EASY -> HARD -> back to EASY, no REMATCH");
  gymRegion = 0; render();
  onTap(233, uiGymPillY()); onTap(233, uiGymPillY()); render();
  ck(gymRematch, "KANTO, champion beaten: EASY -> HARD -> REMATCH");
  uint16_t badgesBefore = pet.badges;
  uint8_t oran = bag.count(IT_ORAN_BERRY), ultra = bag.count(IT_ULTRABALL);
  extern int uiGymRowCenterY(int i);
  onTap(233, uiGymRowCenterY(0)); render();
  ck(pickOpen, "tapping BROCK in REMATCH opens the picker");
  onTap(pickGoX(), pickGoY()); render();
  TrainerMon want[TRAINER_TEAM_MAX];
  uint8_t wn = rematchTeam(0, 0, want);
  bool rmSame = battleOpen && btlKind == 2 /*BK_REMATCH*/ && btlTeamN == wn;
  for (uint8_t k = 0; k < wn && rmSame; k++) rmSame = btlTeam[k].dex == want[k].dex && btlTeam[k].level == want[k].level;
  ck(rmSame, "a REMATCH battle against tower.cpp's stronger team, exactly");
  ck(winFight(), "won");
  ck(rematchWon(0, 0) && pet.badges == badgesBefore, "the rematch is recorded and no badge changes");
  ck(bag.count(IT_ORAN_BERRY) == oran + 1 && bag.count(IT_ULTRABALL) == ultra + 2, "first rematch win: ORAN BERRY + 2 ULTRA BALL");
  battleTap(233, 380); render();
  ck(!battleOpen && gymOpen && !gymPick && gymRematch, "dismissing returns to the REMATCH ladder");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
