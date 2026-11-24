// The daycare. Rules, inheritance, the cooldown, and -- the part that is this
// branch's own -- the hand-over from the daycare's queue to the live egg,
// including a power cut between the two writes it takes. Driven through the
// real Pet::newEgg()/eggTap()/hatch(), not by calling the hooks by hand.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "dex.h"
#include "inventory.h"
#include "items.h"
#include "breed.h"
#include "save.h"
#include <cstdio>
#include <cstring>
#include <vector>
uint32_t g_seed=29; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char *s){printf("%s  %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}

static PartyMon mon(int16_t dex, uint8_t a, uint8_t d, uint8_t s, uint8_t h, bool shiny=false) {
  PartyMon m; m.dex = dex; m.level = 30; m.ivAtk = a; m.ivDef = d; m.ivSpe = s; m.ivHp = h;
  m.shiny = shiny ? 1 : 0; return m;
}
static void hatchNow(Pet &p) { for (int i = 0; i < 3 && p.isEgg(); i++) p.eggTap(); }

int main() {
  gRegionArt = 0xFFFF;
  Pet p; p.begin(); p.dbgHatchAs(4, false);
  party.begin(); bag.begin(); bag.clear();
  for (int i = 0; i < PARTY_SLOTS; i++) party.releaseAt(i);
  breedReset(); breedBegin(&p);
  const uint32_t day = 20000;

  // ---- the rule
  PartyMon chz = mon(6, 31, 10, 31, 10), chm = mon(5, 10, 31, 10, 31);
  PartyMon sqt = mon(7, 20, 20, 20, 20), dit = mon(BREED_DITTO, 20, 20, 20, 20);
  PartyMon dit2 = mon(BREED_DITTO, 1, 1, 1, 1), mew = mon(151, 31, 31, 31, 31);
  ck(breedCheck(chz, chm) == BV_OK && breedEggSpecies(chz, chm) == 4,
     "CHARIZARD + CHARMELEON breed, and the egg is the base form CHARMANDER");
  ck(breedCheck(chz, sqt) == BV_NO_TYPE, "fire + water share no type: refused");
  ck(breedCheck(dit, sqt) == BV_OK && breedEggSpecies(dit, sqt) == 7,
     "DITTO + SQUIRTLE breed, and the egg is the non-Ditto parent's line");
  ck(breedCheck(dit, dit2) == BV_TWO_DITTO, "two DITTOs never");
  ck(breedCheck(mew, dit) == BV_LEGEND, "a legendary line never");
  ck(breedCheck(chz, chz) == BV_SAME, "the same creature twice is refused");
  ck(breedBaseOf(136) == 133 && breedBaseOf(700) == 133, "EEVEE's branches breed back to EEVEE");

  // ---- inheritance
  {
    bool shape = true;
    for (int t = 0; t < 200 && shape; t++) {
      uint8_t iv[4]; breedInherit(chz, chm, false, iv);
      const uint8_t pa[4] = {31,10,31,10}, pb[4] = {10,31,10,31};
      int rolled = 0;
      for (int i = 0; i < 4; i++) {
        if (iv[i] == BREED_IV_ROLL) rolled++;
        else if (iv[i] != pa[i] && iv[i] != pb[i]) shape = false;
      }
      if (rolled != 4 - BREED_INHERIT) shape = false;
    }
    ck(shape, "without the charm: exactly 3 IVs inherited, each from one parent, 1 left to the roll");
    uint8_t iv[4]; breedInherit(chz, chm, true, iv);
    ck(iv[0]==31 && iv[1]==31 && iv[2]==31 && iv[3]==31, "with the OVAL CHARM: all four, the better parent each");
  }

  // ---- making an egg
  party.replaceAt(0, chz); party.replaceAt(1, chm); party.replaceAt(2, sqt);
  ck(breedCheckIdx(0, 2) == BV_NO_TYPE && breedPossible(), "an incompatible pick is refused while a good pair exists");
  bag.add(IT_OVAL_CHARM, 1);
  ck(breedMake(0, 1, day) == BR_OK && breedHasQueued() && breedQueued().dex == 4,
     "BREED queues a CHARMANDER egg");
  ck(breedMake(0, 1, day) == BR_QUEUE_FULL, "one queued egg at a time");
  ck(breedOnCooldown(day) && !breedOnCooldown(day + 1), "one breed per game day");
  ck(party.slots[0].dex == 6 && party.slots[1].dex == 5, "the parents are not consumed");
  breedReset(); breedBegin(&p);
  ck(breedHasQueued() && breedQueued().dex == 4 && breedQueued().iv[0] == 31,
     "the queued egg and its IVs survive a reboot");

  // ---- the hand-over, all the way to a hatched creature with its parents' IVs
  p.newEgg();
  ck(p.isEgg() && p.eggPeek() == 4, "the next egg is the bred one");
  ck(!breedHasQueued() && breedLive().dex == 4, "and the queue moved to 'live' only after it saved");
  hatchNow(p);
  ck(p.speciesId == 4 && p.ivAtk == 31 && p.ivDef == 31 && p.ivSpe == 31 && p.ivHp == 31,
     "it hatches a CHARMANDER with the inherited IVs");
  ck(breedLive().dex == 0, "and the inheritance is spent");
  p.newEgg(); hatchNow(p);
  ck(breedLive().dex == 0, "an ordinary egg afterwards inherits nothing");

  // ---- the checkpoint write FAILS: the bred egg must stay queued, not vanish
  ck(breedMake(0, 1, day + 1) == BR_OK, "a second breed the next day");
  nvsFailWritesAfter(0);
  p.newEgg();
  nvsResumeWrites();
  ck(breedHasQueued() && breedQueued().dex == 4, "a failed egg save leaves the bred egg QUEUED");

  // ---- power cut AFTER the egg landed but BEFORE the queue was cleared
  {
    std::vector<uint8_t> before = nvs()["dcare"];   // the queue as it stood
    p.newEgg();                                       // lands, then commits
    ck(p.eggPeek() == 4 && !breedHasQueued(), "the retried egg lands and commits");
    nvs()["dcare"] = before;                          // ...as if the commit never happened
    Pet q; q.begin();
    breedReset(); breedBegin(&q);
    ck(!breedHasQueued() && breedLive().dex == 4,
       "boot finishes the interrupted hand-over: no second copy queued, IVs kept");
    // and the negative companion: the recovery must be what did it
    nvs()["dcare"] = before;
    breedReset(); breedBegin(nullptr);
    ck(breedHasQueued(), "(without the pet to compare against, the queue is left alone)");
    breedReset(); breedBegin(&q);
  }

  // ---- SWAP IN NOW
  p.dbgHatchAs(1, false); p.newEgg();
  ck(breedMake(1, 0, day + 5) == BR_OK, "breed again");
  ck(breedCanSwapIn(p) && breedSwapIn(p) && p.eggPeek() == 5 - 1 && !breedHasQueued(),
     "SWAP IN NOW turns the uncracked egg into the bred one");
  p.eggTap();
  ck(!breedCanSwapIn(p), "a cracked egg cannot be swapped");

  // ---- a missing pack: the queued egg waits instead of hatching as a number
  hatchNow(p);
  ck(breedMake(0, 1, day + 9) == BR_OK, "queue one more");
  gRegionArt = 0xFFFF & ~1u;      // Kanto's pack gone
  ck(!breedQueuedHatchable() && breedCheck(chz, chm) == BV_NO_EGG, "with Kanto's pack missing it cannot hatch");
  p.newEgg();
  ck(breedHasQueued() && p.eggPeek() != 4, "so newEgg() leaves it queued and rolls an ordinary egg");
  gRegionArt = 0xFFFF;

  // ---- the backup carries it
  {
    bool listed = false;
    for (uint16_t i = 0; i < SAVE_FIELD_COUNT; i++) if (!strcmp(SAVE_FIELDS[i].key, "dcare")) listed = true;
    ck(listed, "the daycare is in the save backup");
  }
  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
