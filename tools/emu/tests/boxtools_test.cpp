// The PC box's tools, through the real screens:
//  - MOVE on a box creature's sheet: the next cell tapped is where it goes --
//    into an empty slot, swapping with another creature, in ANOTHER box after
//    paging the rim, or back to its own slot (nothing moves); down cancels;
//    the plain box shows while moving, even with a filter on;
//  - the overview (tap the title): every box with its count, a tap jumps
//    there, it pages on the rim;
//  - SEARCH: the filter's chip opens the keyboard, and what is typed finds
//    creatures by species name or nickname, case aside.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "box_filter.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=101; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void onTap(int16_t,int16_t); void onSwipeV(int); void onRim(int);
uint8_t uiCurrentScreen(); uint16_t uiRimPages();
extern const char *const SCREEN_NAME[];
extern Pet pet; extern Party party;
extern bool partyOpen, boxOpen, boxFilterOpen, boxListOpen, kbOpen;
extern uint8_t boxPage, boxFilter, boxSort, boxListPage; extern uint16_t boxDetail, boxMoveFrom;
void boxCellPos(uint8_t i, int *cx, int *cy); void boxPillCentre(int *x, int *y);
void monSheetMoveCentre(int *x, int *y);
void boxListTile(uint8_t pos, int *x, int *y, int *w, int *h);
void bfChip(uint8_t i, int *x, int *y, int *w, int *h);
void kbKeyCentre(int i, int *x, int *y);
int boxIndexAt(uint8_t pos);
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static bool on(const char *n){ return !strcmp(SCREEN_NAME[uiCurrentScreen()], n); }
static void tap(int x, int y){ onTap((int16_t)x, (int16_t)y); render(); }
static void tapCell(uint8_t i){ int x, y; boxCellPos(i, &x, &y); tap(x, y); }
static void moveOut(uint8_t cell){ tapCell(cell); int x, y; monSheetMoveCentre(&x, &y); tap(x, y); }
static PartyMon mon(int16_t dex, const char *nick) { PartyMon m; m.dex = dex; m.level = 30; snprintf(m.nick, sizeof(m.nick), "%s", nick); return m; }

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(25, false);
  for (int i = 0; i < PARTY_SLOTS; i++) party.releaseAt(i);   // room in the party: a box tap opens the sheet
  for (uint16_t i = 0; i < BOX_SLOTS; i++) party.box[i] = PartyMon();
  party.box[0] = mon(7, "SHELLY"); party.box[1] = mon(25, ""); party.box[2] = mon(4, "");
  party.boxSave();
  partyOpen = true; boxOpen = true; boxPage = 0; render();
  ck(on("box"), "on the box");

  // ---- MOVE into an empty slot
  moveOut(0);
  ck(boxMoveFrom == 1 && on("box") && !boxDetail, "MOVE on the sheet goes back to the box, carrying the creature");
  tapCell(10);
  ck(party.box[10].dex == 7 && party.box[0].empty() && !boxMoveFrom, "the next cell tapped is where it goes");
  { Party q; q.begin(); ck(q.box[10].dex == 7 && q.box[0].empty(), "and it is saved"); }
  // ---- MOVE onto another creature: they swap
  moveOut(10); tapCell(1);
  ck(party.box[1].dex == 7 && party.box[10].dex == 25, "onto another creature: the two swap places");
  // ---- back onto its own slot: nothing moves
  moveOut(1); tapCell(1);
  ck(party.box[1].dex == 7 && !boxMoveFrom, "back onto its own slot: nothing moves");
  // ---- to ANOTHER box: page on the rim while carrying it
  moveOut(1); onRim(1); onRim(1); render();
  ck(boxPage == 2 && boxMoveFrom == 2, "the rim pages to another box while carrying it");
  tapCell(5);
  ck(party.box[2 * BOX_PER_BOX + 5].dex == 7 && party.box[1].empty(), "and it lands in that box");
  // ---- down puts a move down without moving anything
  moveOut(5); onSwipeV(1); render();
  ck(!boxMoveFrom && boxOpen && party.box[2 * BOX_PER_BOX + 5].dex == 7, "down cancels a move, and the box stays open");
  // ---- a filter on: moving shows the plain box (that is where the room is)
  boxPage = 0; boxFilter = BF_TYPE0 + T_FIRE; render();
  ck(boxIndexAt(0) == 2, "(filtered to FIRE: CHARMANDER first)");
  moveOut(0);
  ck(boxIndexAt(0) == 0 && boxIndexAt(29) == 29, "while moving, the plain box shows, whatever the filter");
  tapCell(29);
  ck(party.box[29].dex == 4 && party.box[2].empty(), "and the move goes to the slot that was tapped");
  boxFilter = BF_ALL; render();

  // ---- the overview: jump to a box
  boxPage = 0; render();
  tap(233, 36);
  ck(on("boxlist") && boxListOpen, "tapping the box title opens every box at a glance");
  ck(uiRimPages() == (BOX_BOXES + 15) / 16, "sixteen a page, paged on the rim");
  { int x, y, w, h; boxListTile(7, &x, &y, &w, &h); tap(x + w / 2, y + h / 2); }
  ck(!boxListOpen && boxPage == 7 && on("box"), "a tap on a box jumps there");
  tap(233, 36); onRim(1); render();
  ck(boxListPage == 1, "the overview pages");
  { int x, y, w, h; boxListTile(4, &x, &y, &w, &h); tap(x + w / 2, y + h / 2); }
  ck(boxPage == 16 + 4, "and a box on its second page is box 21");
  tap(233, 36); onSwipeV(1); render();
  ck(!boxListOpen && boxOpen && on("box"), "down closes the overview, not the box");

  // ---- SEARCH
  boxPage = 0; render();
  party.box[40] = mon(7, ""); party.box[41] = mon(8, "SQUIRT"); party.box[42] = mon(16, "");
  party.boxSave();
  { int x, y; boxPillCentre(&x, &y); tap(x, y); }
  { int x, y, w, h; bfChip(BF_SEARCH, &x, &y, &w, &h); tap(x + w / 2, y + h / 2); }
  ck(kbOpen && on("keyboard"), "the SEARCH chip opens the keyboard");
  for (const char *c = "SQU"; *c; c++) { int x, y; kbKeyCentre(*c - 'A', &x, &y); tap(x, y); }
  { int x, y; kbKeyCentre(29, &x, &y); tap(x, y); }
  ck(!kbOpen && boxFilter == BF_SEARCH && on("box"), "OK filters the box by what was typed");
  int found = 0, squirtle = 0;
  for (uint8_t i = 0; i < 30; i++) { int ix = boxIndexAt(i); if (ix < 0) break; found++; if (party.box[ix].dex == 7) squirtle++; }
  ck(found == 3 && squirtle == 2, "SQU finds SQUIRTLE twice (one nicknamed SHELLY) and SQUIRT the WARTORTLE, not PIDGEY");
  boxSearchSet("shel");
  render();
  ck(boxIndexAt(0) >= 0 && party.box[boxIndexAt(0)].dex == 7 && boxIndexAt(1) < 0, "lower case finds the nickname SHELLY");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
