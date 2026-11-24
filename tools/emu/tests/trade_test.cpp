// Trading over the link, protocol side. Two Links cross-wired in one process,
// with each board's STORAGE simulated so it can be made to fail on purpose.
//
// The property, under loss, duplication, a failed store and a link that dies
// at every stage: EACH CREATURE ENDS UP ON AT LEAST ONE BOARD. A duplicate is
// acceptable (the two generals cannot be beaten); a loss is not.
//
// The storage-first fix is what makes "a failed store" safe: the original
// (Tubiana's) sent DONE from tradeCommit(), before the app stored anything, so
// one side failing its store still told the other side to let go. The
// negative check of this test is exactly that: put the old DONE back and the
// failed-store scenario loses a creature.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "link.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=47; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static Link A, B;
static int dropEvery = 0, dupEvery = 0, seen = 0;
static bool cut = false;               // the link is gone: nothing gets through
static void wire(Link &to, const uint8_t *b, uint8_t n) {
  seen++;
  if (cut) return;
  if (dropEvery && (seen % dropEvery) == 0) return;
  uint8_t tmp[260]; if (n > sizeof(tmp)) return;
  memcpy(tmp, b, n);
  to.onPacket(tmp, n);
  if (dupEvery && (seen % dupEvery) == 0) to.onPacket(tmp, n);
}
static void toB(void*, const uint8_t*b, uint8_t n){ wire(B, b, n); }
static void toA(void*, const uint8_t*b, uint8_t n){ wire(A, b, n); }

static LinkMon lmon(int16_t dex){
  Pet p; p.dbgHatchAs(dex,false); p.ageMinutes = 20UL * 60; p.relearnFromLevel();
  Combatant c; combatantFromPet(c,p); LinkMon m; linkMonFrom(m,c); return m;
}
static PartyMon pmon(int16_t dex, const char *nick){
  Pet p; p.dbgHatchAs(dex,false); p.ageMinutes = 20UL * 60; p.relearnFromLevel();
  PartyMon m = p.toPartyMon(); snprintf(m.nick, sizeof(m.nick), "%s", nick); return m;
}

// ---- one simulated board's storage and the app loop that drives the trade
struct Board {
  Link *L;
  PartyMon own;           // the creature it offers (in its "box")
  bool ownHere = true;    // still on this board
  bool gotTheirs = false; // the peer's creature is stored here
  bool storeFails = false;
  void poll() {
    if (L->tArrived && !gotTheirs && !storeFails) { gotTheirs = true; L->tradeStored(); }
    if (L->tradeCanRelease() && ownHere) { ownHere = false; L->tradeReleased(); }
  }
};
static Board BA{ &A }, BB{ &B };
static uint32_t T = 0;
static void run(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 50) { T += 50; A.tick(T); B.tick(T); BA.poll(); BB.poll(); }
}
static void fresh() {
  cut = false; dropEvery = dupEvery = 0;
  A.send = toB; B.send = toA;
  A.begin(true, "A"); B.begin(false, "B");
  A.id = 0x1111; B.id = 0x2222;
  A.addMon(lmon(6)); B.addMon(lmon(9));
  A.tick(T); B.tick(T); A.start();
  run(3000);
  BA = Board{ &A, pmon(64, "KADA"), true, false, false };
  BB = Board{ &B, pmon(1, "BULBA"), true, false, false };
}
// each creature on at least one board
static bool noneLost() {
  bool aMon = BA.ownHere || BB.gotTheirs;   // A's creature: still on A, or stored on B
  bool bMon = BB.ownHere || BA.gotTheirs;
  return aMon && bMon;
}

int main(){
  // ---- a clean trade
  fresh();
  ck(A.ready() && B.ready(), "the two boards pair");
  A.tradeOffer(BA.own); B.tradeOffer(BB.own);
  run(1000);
  ck(A.tradeHasTheirs() && B.tradeHasTheirs() && A.tTheirs.dex == 1 && B.tTheirs.dex == 64,
     "each side sees the other's offer");
  A.tradeAccept(); run(500);
  ck(BA.ownHere && BB.ownHere && !BA.gotTheirs, "one accept alone moves nothing");
  B.tradeAccept(); run(2000);
  ck(BA.gotTheirs && BB.gotTheirs && !BA.ownHere && !BB.ownHere, "both accept: each stores theirs, then gives up its own");

  // ---- a changed offer voids an earlier accept
  fresh();
  A.tradeOffer(BA.own); B.tradeOffer(BB.own); run(800);
  A.tradeAccept(); run(300);
  B.tradeOffer(pmon(7, "SQUIRT")); run(800);
  B.tradeAccept(); run(1500);
  ck(BA.ownHere && BB.ownHere && !BA.gotTheirs && !BB.gotTheirs,
     "B changing its offer after A accepted voids A's accept: nothing moves");

  // ---- under loss and duplication
  fresh();
  dropEvery = 3; dupEvery = 4;
  A.tradeOffer(BA.own); B.tradeOffer(BB.own); run(1500);
  A.tradeAccept(); B.tradeAccept(); run(6000);
  ck(BA.gotTheirs && BB.gotTheirs && !BA.ownHere && !BB.ownHere,
     "with every 3rd frame dropped and every 4th doubled, the trade still completes exactly");

  // ---- THE FIX: B's store FAILS -> B never says DONE -> A never lets go
  fresh();
  BB.storeFails = true;
  A.tradeOffer(BA.own); B.tradeOffer(BB.own); run(800);
  A.tradeAccept(); B.tradeAccept(); run(4000);
  ck(BA.ownHere, "B failed to store A's creature, so A keeps it (no DONE was ever sent)");
  ck(noneLost(), "and neither creature is lost");
  // a later successful store finishes it
  BB.storeFails = false; run(3000);
  ck(BB.gotTheirs && !BA.ownHere && noneLost(), "once B's store succeeds the trade completes, nothing lost");

  // ---- the link dies at every stage: never a loss
  const char *stage[] = { "after the offers", "after one accept", "after both accept", "just after the first DONE" };
  for (int s = 0; s < 4; s++) {
    fresh();
    A.tradeOffer(BA.own); B.tradeOffer(BB.own); run(800);
    if (s >= 1) { A.tradeAccept(); run(200); }
    if (s >= 2) { B.tradeAccept(); }
    if (s == 3) { run(60); }
    cut = true;
    run(20000);              // long past every timeout
    char m[96];
    snprintf(m, sizeof(m), "link cut %s: each creature is on at least one board", stage[s]);
    ck(noneLost(), m);
  }

  // ---- untrusted bytes are clamped, a bad dex refused
  {
    PartyMon m = pmon(25, "PIKA");
    m.level = 250; m.ivAtk = 99; m.moves[0] = 250; m.held = 250; m.persona = 99; m.joy = 200;
    m.nick[sizeof(m.nick) - 1] = 'X';
    ck(tradeSanitize(m) && m.level == MAX_LEVEL && m.ivAtk == 31 && m.moves[0] == 0 &&
       m.held == 0 && m.persona == 0 && m.joy == 100 && m.nick[sizeof(m.nick) - 1] == 0,
       "a received creature's every field is clamped");
    PartyMon z = pmon(25, "Z"); z.dex = DEX_COUNT + 5;
    ck(!tradeSanitize(z), "and a dex past the table is refused outright");
  }
  ck(tradeEvolution(64) == 65 && tradeEvolution(1) == 0, "KADABRA evolves on trade; BULBASAUR does not");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
