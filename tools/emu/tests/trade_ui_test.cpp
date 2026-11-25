// Trading through the REAL screens and the REAL party storage: this board's
// global `lan` cross-wired to a simulated peer Link in the same process. The
// peer stores what it receives at once; this board goes through tradePoll(),
// Party::storeDurably() and Party::takeOutExact() exactly as on hardware.
//
// Covered: a clean trade (the offered creature leaves, theirs arrives, its held
// item with it), a trade evolution on arrival, a full party AND box (no DONE is
// sent, so the peer keeps theirs and we keep ours), and a store that FAILS at
// the flash (no DONE until a later store lands -- never a loss).
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "items.h"
#include "link.h"
#include "inventory.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=53; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void onTap(int16_t,int16_t); void tradePoll();
extern Pet pet; extern Link lan;
extern bool lanOpen, tradeOpen; extern uint8_t tradeView;
int trdCardCenterX(uint8_t i); int trdCardCenterY(); int trdBtnCenterY(); int trdRowCenterY(uint8_t r);
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static Link P;                       // the other board
static bool peerStores = true, peerGot = false, peerGave = false;
static uint32_t peerDoneAt = 0;
static void toPeer(void*, const uint8_t *b, uint8_t n){ uint8_t t[260]; memcpy(t,b,n); P.onPacket(t,n); }
static void toMe(void*, const uint8_t *b, uint8_t n){ uint8_t t[260]; memcpy(t,b,n); lan.onPacket(t,n); }
static uint32_t T = 1000;
static void run(int ms) {
  for (int t = 0; t < ms; t += 50) {
    T += 50; lan.tick(T); P.tick(T); tradePoll();
    if (P.tArrived && !P.tStored && peerStores) { peerGot = true; P.tradeStored(); }
    if (P.tradeCanRelease()) { peerGave = true; P.tradeReleased(); peerDoneAt = T; }
    if (P.tPhase == 2) P.tradeReset();   // the peer's app clears a finished deal too, at once
    render();
  }
}
static LinkMon lm(int16_t dex){ Pet p; p.dbgHatchAs(dex,false); p.ageMinutes=600; p.relearnFromLevel();
  Combatant c; combatantFromPet(c,p); LinkMon m; linkMonFrom(m,c); return m; }
static PartyMon pm(int16_t dex, const char *nick){ Pet p; p.dbgHatchAs(dex,false); p.ageMinutes=1200; p.relearnFromLevel();
  PartyMon m = p.toPartyMon(); snprintf(m.nick,sizeof(m.nick),"%s",nick); return m; }
static int countDex(int16_t d){ int n=0; for(int i=0;i<PARTY_SLOTS;i++) if(party.slots[i].dex==d) n++;
  for(int i=0;i<BOX_SLOTS;i++) if(party.box[i].dex==d) n++; return n; }
static void pairUp() {
  lan.begin(true, "ME"); lan.send = toPeer; lan.id = 0x1111; lan.addMon(lm(6));
  P.begin(false, "PEER"); P.send = toMe; P.id = 0x2222; P.addMon(lm(9));
  lan.tick(T); P.tick(T); lan.start();
  peerGot = peerGave = false; peerStores = true;
  run(2000);
}
// The player's side, through the screens: open TRADE, pick party slot `row`, ACCEPT.
static void offerAndAccept(uint8_t row) {
  tradeOpen = false; tradeView = 0; lanOpen = true; render();
  onTap(233, 318);                                 // the LAN screen's TRADE button
  onTap(trdCardCenterX(0), trdCardCenterY());      // my card -> the picker
  onTap(233, trdRowCenterY(row));                  // offer that creature
  run(500);
  onTap(233, trdBtnCenterY());                     // ACCEPT
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(4, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  for (int i=0;i<PARTY_SLOTS;i++) party.releaseAt(i);
  for (int i=0;i<BOX_SLOTS;i++) party.box[i] = PartyMon();
  party.boxSave();
  bag.clear();

  // ---- a clean trade, through the screens
  PartyMon mine = pm(25, "ZAP");
  mine.held = IT_QUICK_CLAW;
  party.replaceAt(0, mine);
  pairUp();
  ck(lan.ready() && P.ready(), "the two boards pair");
  P.tradeOffer(pm(1, "BULBA"));
  offerAndAccept(0);
  ck(tradeOpen && lan.tOffering && lan.tMine.dex == 25, "the trade screen offers the chosen party creature");
  ck(lan.tIAccepted, "ACCEPT accepts");
  P.tradeAccept();
  run(3000);
  ck(countDex(1) == 1 && countDex(25) == 0, "BULBASAUR arrived and PIKACHU left this board");
  ck(peerGot && peerGave, "and the peer stored ours and gave theirs up");
  {
    Party q; q.begin();
    bool onFlash = false;
    for (int i = 0; i < PARTY_SLOTS; i++) if (q.slots[i].dex == 1) onFlash = true;
    for (int i = 0; i < BOX_SLOTS; i++) if (q.box[i].dex == 1) onFlash = true;
    ck(onFlash, "the arrival is on FLASH, not just in RAM");
  }
  ck(!bag.has(IT_QUICK_CLAW), "and its QUICK CLAW left with it (not returned to our bag)");

  // ---- a trade evolution: a KADABRA arrives as ALAKAZAM
  run(4000);                                       // the finished deal clears
  party.replaceAt(2, pm(16, "PIDGE"));   // the picker lists BULBASAUR (slot 1) then PIDGEY: row 1
  P.tradeOffer(pm(64, "KADA"));
  offerAndAccept(1);
  P.tradeAccept();
  run(3000);
  ck(countDex(65) == 1 && countDex(64) == 0, "a traded KADABRA arrives as ALAKAZAM");

  // ---- a full party AND box: no DONE, both keep theirs
  run(4000);
  for (int i=0;i<PARTY_BANKED;i++) party.replaceAt(i, pm(10, "CAT"));
  for (int i=0;i<BOX_SLOTS;i++) party.box[i] = pm(13, "WEED");
  party.boxSave();
  peerGot = peerGave = false;
  P.tradeOffer(pm(7, "SQUIRT"));
  offerAndAccept(2);
  P.tradeAccept();
  run(3000);
  ck(countDex(7) == 0 && !peerGave, "with no room here nothing arrives, and the peer keeps SQUIRTLE");
  ck(countDex(10) == PARTY_BANKED, "and ours is not taken either");
  ck(peerGot, "(the peer did store ours: a duplicate, never a loss)");

  // ---- the flash write FAILS: no DONE until a store actually lands
  pairUp();
  party.releaseAt(PARTY_BANKED - 1);   // one party slot free
  P.tradeOffer(pm(152, "CHIK"));
  offerAndAccept(0);
  nvsFailWritesAfter(0);
  P.tradeAccept();
  run(1500);
  ck(!peerGave && countDex(152) == 0, "a failed write here: no DONE, the peer keeps CHIKORITA");
  nvsResumeWrites();
  run(3000);
  ck(peerGave && countDex(152) == 1, "once a write lands, the trade completes");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
