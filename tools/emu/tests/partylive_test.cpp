// Three things that came in together, through the real screens:
//
//  1. The creature you raise is PARTY SLOT 1. The party is it plus five banked:
//     the ring's first position draws it, a tap there opens its STATS, and
//     only five banked fit. An old save with six banked moves the sixth to a
//     free slot, else to the box -- never dropped.
//  2. A STATS page for ANY creature: the button on a party or box sheet, and
//     slot 1. Down, sideways or a tap goes back; it shows the right creature.
//  3. PC boxes can be RENAMED from the box title, and the name is saved and
//     travels in the backup.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "box_store.h"
#include <cstdio>
#include <cstring>
#include <vector>
uint32_t g_seed=71; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void onSwipeV(int); void onSwipe(int); void onTap(int16_t,int16_t);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
extern Pet pet; extern Party party;
extern bool partyOpen, boxOpen, partyPick, kbOpen;
extern uint8_t boxPage, partyDetail, boxSwapFrom, monStats; extern uint16_t boxDetail;
void partySlotPos(int i, int *cx, int *cy); void partyLivePos(int *cx, int *cy);
int partySlotAt(int16_t x, int16_t y); bool partyLiveAt(int16_t x, int16_t y);
void boxCellPos(uint8_t i, int *cx, int *cy);
void monSheetStatsCentre(int *x, int *y);
void kbKeyCentre(int i, int *x, int *y);
bool monStatsSubject(PartyMon *out);
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static bool on(const char *n){ return !strcmp(SCREEN_NAME[uiCurrentScreen()], n); }
static PartyMon mon(int16_t dex, uint8_t lvl) {
  PartyMon m; m.dex = dex; m.level = lvl; m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20; return m;
}
static void tap(int x, int y) { onTap((int16_t)x, (int16_t)y); render(); }
static void typeName(const char *s) {
  for (int i = 0; i < 12; i++) { int x, y; kbKeyCentre(28, &x, &y); tap(x, y); }   // clear
  for (const char *c = s; *c; c++) { int x, y; kbKeyCentre(*c - 'A', &x, &y); tap(x, y); }
  int x, y; kbKeyCentre(29, &x, &y); tap(x, y);
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(25, false);
  for (int i = 0; i < PARTY_SLOTS; i++) party.releaseAt(i);

  // ---- 1. slot 1 is the live pet
  ck(PARTY_BANKED == 5, "five banked plus the one you raise");
  for (int i = 0; i < PARTY_BANKED; i++) ck(party.add(mon(10 + i, 20)), i == 0 ? "the first five banked fit" : "(and the next)");
  ck(party.isFull() && !party.add(mon(30, 20)), "and a sixth does not: the party is full at five banked");
  { int lx, ly; partyLivePos(&lx, &ly);
    bool clash = false;
    for (int i = 0; i < PARTY_BANKED; i++) { int sx, sy; partySlotPos(i, &sx, &sy); if (sx == lx && sy == ly) clash = true; }
    ck(!clash && partyLiveAt(lx, ly) && partySlotAt(lx, ly) < 0, "slot 1 is the live pet's alone; no banked slot shares it"); }
  partyOpen = true; render();
  ck(on("party"), "on the party");
  { int lx, ly; partyLivePos(&lx, &ly); tap(lx, ly); }
  PartyMon who;
  ck(on("monstats") && monStatsSubject(&who) && who.dex == 25, "tapping slot 1 shows the live pet's STATS");
  onSwipeV(1); render();
  ck(on("party") && !monStats, "down goes back to the party");
  // making room after a farewell never offers the live pet
  pet.setEnded(mon(1, 30), CER_FAREWELL);
  partyPick = true; render();
  { int lx, ly; partyLivePos(&lx, &ly); tap(lx, ly); }
  ck(!monStats && partyPick && party.slots[0].dex == 10, "while choosing who to replace, slot 1 is not a choice");
  { int sx, sy; partySlotPos(1, &sx, &sy); tap(sx, sy); }
  ck(!partyPick && party.slots[1].dex == 1, "a banked slot is");

  // ---- an old save with SIX banked
  for (int i = 0; i < PARTY_SLOTS; i++) party.slots[i] = mon(40 + i, 30);
  party.save();
  for (uint16_t i = 0; i < BOX_SLOTS; i++) party.box[i] = PartyMon();
  party.boxSave();
  { Party q; q.begin();
    ck(q.slots[5].empty() && q.box[0].dex == 45 && q.count() == 5, "an old sixth banked creature moves to the box at boot");
    Party r; r.begin();
    ck(r.box[0].dex == 45 && r.slots[5].empty(), "and stays there (once, not every boot)"); }
  for (int i = 0; i < PARTY_SLOTS; i++) party.slots[i] = PartyMon();
  party.slots[5] = mon(77, 30); party.slots[0] = mon(78, 30);
  party.save();
  { Party q; q.begin(); ck(q.slots[1].dex == 77 && q.slots[5].empty(), "with room in the party, it takes a free party slot instead"); }
  party.begin();

  // ---- 2. STATS from a party sheet and a box sheet
  partyOpen = true; partyPick = false; boxOpen = false; monStats = 0; render();
  { int sx, sy; partySlotPos(1, &sx, &sy); tap(sx, sy); }
  ck(partyDetail == 2 && on("party"), "a party slot opens its sheet");
  { int x, y; monSheetStatsCentre(&x, &y); tap(x, y); }
  ck(on("monstats") && monStatsSubject(&who) && who.dex == 77, "STATS on the sheet shows THAT creature");
  tap(233, 233);
  ck(on("party") && partyDetail == 2 && !monStats, "a tap goes back to its sheet");
  { int x, y; monSheetStatsCentre(&x, &y); tap(x, y); }
  onSwipe(1); render();
  ck(!monStats && partyDetail == 2, "so does a sideways swipe");
  partyDetail = 0; render();
  party.box[3] = mon(143, 60); party.boxSave();
  boxOpen = true; boxPage = 0; boxSwapFrom = 0; render();
  { int x, y; boxCellPos(3, &x, &y); tap(x, y); }
  ck(boxDetail == 4, "a box slot opens its sheet");
  { int x, y; monSheetStatsCentre(&x, &y); tap(x, y); }
  ck(on("monstats") && monStatsSubject(&who) && who.dex == 143, "STATS works from the box too");
  party.box[3] = PartyMon(); render();
  ck(!monStats, "if the creature is gone, the page closes instead of showing nothing");
  boxDetail = 0; render();

  // ---- 3. naming a box
  ck(on("box"), "back on the box");
  { char n[16]; party.boxNameOf(0, n, sizeof(n)); ck(!strcmp(n, "BOX 1"), "a box is BOX n until it is named"); }
  tap(233, 36);
  ck(kbOpen && on("keyboard"), "tapping the box title opens the keyboard on top of it");
  typeName("WATER");
  { char n[16]; party.boxNameOf(0, n, sizeof(n));
    ck(!kbOpen && on("box") && !strcmp(n, "WATER"), "OK names it and returns to the box"); }
  { Party q; q.begin(); char n[16]; q.boxNameOf(0, n, sizeof(n)); ck(!strcmp(n, "WATER"), "the name is saved"); }
  { std::vector<uint8_t> buf(SAVE_TRANSFER_MAX); size_t n = saveExport(buf.data(), buf.size());
    party.setBoxName(0, "");
    ck(n && saveImport(buf.data(), n), "a backup round-trips");
    Party q; q.begin(); char nm[16]; q.boxNameOf(0, nm, sizeof(nm));
    ck(!strcmp(nm, "WATER"), "and carries the box's name"); }
  party.begin();
  boxPage = 1; render();
  tap(233, 36); typeName("");
  { char n[16]; party.boxNameOf(1, n, sizeof(n)); ck(!strcmp(n, "BOX 2"), "an empty name keeps the default"); }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
