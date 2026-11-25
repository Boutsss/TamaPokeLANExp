// Box filters and favourites through the REAL screens. The box's hub opens the
// FILTER sheet, a chip filters, the ring then shows (and opens) exactly the
// matching creatures, SORT cycles, rim paging follows the filtered count. The
// sheet's star marks a favourite, and a favourite then cannot be released
// (button or YES), replaced to make room, offered in a trade, or -- raised as
// the live pet -- released or retired early. Unstarring makes it releasable
// again. Every coordinate is asked of the firmware.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "dex.h"
#include "box_filter.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=37; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void onSwipeV(int); void onRim(int);
void boxTap(int16_t,int16_t); void partyTap(int16_t,int16_t);
uint8_t uiCurrentScreen(); uint8_t uiRimPages();
extern const char *const SCREEN_NAME[];
extern Pet pet; extern Party party;
extern bool partyOpen, boxOpen, boxFilterOpen, partyPick, releaseConfirm;
extern uint8_t boxPage, boxDetail, partyDetail, boxFilter, boxSort, boxSwapFrom;
int boxIndexAt(uint8_t pos);
void partySlotPos(int i, int *cx, int *cy);
void bfChip(uint8_t i, int *x, int *y, int *w, int *h);
void bfSortBtn(int *x, int *y, int *w, int *h);
void monSheetCentres(int *left, int *star, int *right, int *cy);
void uiConfirmRects(int *c1t, int *c1b, int *c2t, int *c2b);
void partyButtonRects(int *boxTop, int *boxBot, int *closeTop, int *closeBot);
uint8_t tradeOfferableCount();
void focusSwap(uint8_t slot);
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static bool onScreen(const char *n){ return !strcmp(SCREEN_NAME[uiCurrentScreen()], n); }
static PartyMon mon(int16_t dex, uint8_t lvl) {
  Pet p; p.dbgHatchAs(dex, false); p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.relearnFromLevel(); return p.toPartyMon();
}
static void tapSlot(uint8_t pos) { int x, y; partySlotPos(pos, &x, &y); boxTap(x, y); render(); }
static void tapChip(uint8_t f) { int x, y, w, h; bfChip(f, &x, &y, &w, &h); boxTap(x + w / 2, y + h / 2); render(); }
static int L, S, R, BY;

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(25, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  monSheetCentres(&L, &S, &R, &BY);
  for (int i = 0; i < PARTY_SLOTS; i++) party.releaseAt(i);
  for (int i = 0; i < BOX_SLOTS; i++) party.box[i] = PartyMon();
  // eight FIRE creatures spread through the box, and some others
  const int16_t fire[] = { 4, 5, 6, 37, 38, 58, 77, 126 };
  for (int k = 0; k < 8; k++) party.box[k * 2] = mon(fire[k], 10 + k * 5);
  party.box[1] = mon(9, 50); party.box[3] = mon(130, 70);
  party.boxSave();

  // ---- the hub opens FILTER
  partyOpen = true; boxOpen = true; boxPage = 0; render();
  ck(onScreen("box"), "on the box");
  boxTap(233, 224); render();
  ck(boxFilterOpen && onScreen("boxfilter"), "the hub opens the FILTER sheet");
  tapChip(BF_TYPE0 + T_FIRE);
  ck(!boxFilterOpen && boxFilter == BF_TYPE0 + T_FIRE, "a chip filters and closes the sheet");
  ck(uiRimPages() == 2, "eight matches: two pages on the rim");
  bool allFire = true;
  for (uint8_t p = 0; p < 2; p++) {
    boxPage = p;
    for (uint8_t i = 0; i < 6; i++) { int idx = boxIndexAt(i); if (idx >= 0 && DEX_TBL[party.box[idx].dex].type1 != T_FIRE) allFire = false; }
  }
  ck(allFire, "the ring shows only FIRE creatures");
  boxPage = 1;
  int want = boxIndexAt(1);
  tapSlot(1);
  ck(boxDetail == want + 1 && want >= 0, "tapping a filtered slot opens THAT creature, not whatever sits in the box slot of that number");
  boxDetail = 0; render();

  // ---- sort
  boxTap(233, 224); render();
  { int x, y, w, h; bfSortBtn(&x, &y, &w, &h); boxTap(x + w / 2, y + h / 2); boxTap(x + w / 2, y + h / 2); render(); }
  ck(boxFilterOpen && boxSort == BS_IV, "SORT cycles and keeps the sheet open");
  { int x, y, w, h; bfSortBtn(&x, &y, &w, &h); boxTap(x + w / 2, y + h / 2); boxTap(x + w / 2, y + h / 2); boxTap(x + w / 2, y + h / 2); }
  onSwipeV(1); render();
  ck(!boxFilterOpen && boxOpen && boxSort == BS_SPE, "down closes the sheet, not the box");
  boxSort = BS_LEVEL; boxPage = 0; render();
  ck(party.box[boxIndexAt(0)].level >= party.box[boxIndexAt(1)].level, "sorted by LEVEL: best first on the ring");

  // ---- an armed swap needs the empty slots: it shows the plain box
  boxSwapFrom = 1; render();
  ck(uiRimPages() == BOX_SLOTS / 6 && boxIndexAt(1) == 1, "with a party creature waiting to come in, the plain box shows");
  boxSwapFrom = 0;
  boxFilter = BF_ALL; boxSort = BS_SLOT; boxPage = 0; render();

  // ---- the star
  tapSlot(0);                                     // CHARMANDER, box slot 0
  ck(boxDetail == 1, "the sheet opens");
  boxTap(S, BY); render();
  ck(party.box[0].isFav(), "the star marks it a favourite");
  { Party q; q.begin(); ck(q.box[0].isFav(), "and it is saved"); }
  boxTap(R, BY); render();
  ck(!releaseConfirm && !party.box[0].empty(), "RELEASE is refused for a favourite");
  releaseConfirm = true;                          // even if the confirm were somehow up,
  { int c1t, c1b, c2t, c2b; uiConfirmRects(&c1t, &c1b, &c2t, &c2b); boxTap(233, (c1t + c1b) / 2); render(); }
  ck(!party.box[0].empty(), "its YES refuses too");
  boxTap(S, BY); render();
  ck(!party.box[0].isFav(), "unstarred");
  boxTap(R, BY); render();
  ck(releaseConfirm, "and now RELEASE asks");
  releaseConfirm = false;
  boxTap(S, BY); render();                        // star it again for what follows
  boxDetail = 0; boxOpen = false; render();

  // ---- the filter finds favourites
  boxOpen = true; boxTap(233, 224); render(); tapChip(BF_FAV);
  ck(boxIndexAt(0) == 0 && boxIndexAt(1) == -1, "FAVOURITES finds exactly the starred one");
  boxFilter = BF_ALL; boxOpen = false; render();

  // ---- never offered in a trade
  uint8_t before = tradeOfferableCount();
  party.box[0].fav = 0;
  ck(tradeOfferableCount() == before + 1, "a favourite is not offered in a trade (unstarred, it is)");
  party.box[0].fav = PM_FAV;

  // ---- making room never takes a favourite
  for (int i = 0; i < PARTY_SLOTS; i++) party.replaceAt(i, mon(19 + i * 2, 20));
  party.slots[0].fav = PM_FAV; party.save();
  pet.setEnded(mon(1, 30), CER_FAREWELL);
  partyPick = true; partyOpen = true; boxOpen = false; render();
  { int x, y; partySlotPos(0, &x, &y); partyTap(x, y); render(); }
  ck(party.slots[0].dex == 19 && pet.endedKind != CER_NONE, "the make-room chooser will not replace a favourite");
  { int x, y; partySlotPos(1, &x, &y); partyTap(x, y); render(); }
  ck(party.slots[1].dex == 1 && pet.endedKind == CER_NONE, "it replaces a creature that is not one");

  // ---- a favourite waiting for a slot is not simply let go
  PartyMon fv = mon(7, 30); fv.fav = PM_FAV;
  pet.setEnded(fv, CER_FAREWELL);
  partyPick = true; partyOpen = true; render();
  { int bt, bb, ct, cb; partyButtonRects(&bt, &bb, &ct, &cb); partyTap(233, (ct + cb) / 2); render(); }
  ck(pet.endedKind != CER_NONE && partyPick, "a waiting favourite cannot be declined while it could replace someone");
  onSwipeV(1); render();
  ck(pet.endedKind != CER_NONE, "nor by swiping down");
  for (int i = 0; i < PARTY_SLOTS; i++) party.slots[i].fav = PM_FAV;
  { int bt, bb, ct, cb; partyButtonRects(&bt, &bb, &ct, &cb); partyTap(233, (ct + cb) / 2); render(); }
  ck(pet.endedKind == CER_NONE, "but with every party member a favourite it can be, so nobody is stuck");
  for (int i = 0; i < PARTY_SLOTS; i++) party.slots[i].fav = 0;
  partyOpen = false; partyPick = false;

  // ---- raised, it stays a favourite, and cannot be released or retired early
  party.slots[2].fav = PM_FAV; party.save();
  int16_t favDex = party.slots[2].dex;
  focusSwap(2);
  ck(pet.speciesId == favDex && pet.favorite, "raised, it is still a favourite");
  ck(!pet.releaseAllowed(), "and cannot be released");
  pet.release();
  ck(pet.ceremony == CER_NONE, "(release() refuses it outright)");
  ck(!pet.canRetireNow(), "nor retired early, which would give it up for good");
  { Pet q; q.begin(); ck(q.favorite, "the mark survives a reboot while it is live"); }
  focusSwap(2);
  ck(party.slots[2].dex == favDex && party.slots[2].isFav(), "banked again, it is still starred");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
