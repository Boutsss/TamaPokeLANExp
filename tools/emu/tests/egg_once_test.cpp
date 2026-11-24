// ONE EGG PER RAISED CREATURE, driven through focusSwap() -- the "RAISE THIS
// ONE" button -- and the real update() that ends a ceremony.
//
// The bug: a creature that had said goodbye went to the party and handed over
// an egg; raising it again brought it back at full age, so the farewell came
// up again ("... wants to tell you") and every round trip was another egg. The
// waiting egg was simply thrown away by the swap.
//
// The rule now: the creature is marked (Pet::eggPaid / PartyMon::eggPaid), the
// farewell is never offered twice, the egg a swap pushes aside is STASHED and
// handed back by the next ending, and with nothing stashed a paid creature
// cannot be retired or released for a fresh egg.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=29; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render();
extern Pet pet; extern Party party;
void focusSwap(uint8_t slot);
void emuSetTimeScale(uint32_t s);
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

// The ceremony runs out; the banked creature takes the first free party slot.
static int finish() {
  // The ceremony runs on the real millis(): fast-forward the emulator clock past it.
  uint32_t t0 = millis();
  emuSetTimeScale(20000);
  while (millis() - t0 < CEREMONY_MS + 500) {}
  emuSetTimeScale(1);
  pet.update(millis());
  int slot = -1;
  if (pet.endedKind != CER_NONE) { slot = party.firstFree(); party.replaceAt(slot, pet.endedMon); pet.clearEnded(); }
  return slot;
}
static void grownUp(int16_t dex) {
  pet.dbgHatchAs(dex, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.ageMinutes = FAREWELL_AGE_MIN + 60;
  pet.fullness = pet.joy = pet.energy = pet.hygiene = 100;
  pet.sleeping = false;
}
static int slotOf(int16_t dex) { for (int i=0;i<PARTY_SLOTS;i++) if (party.slots[i].dex==dex) return i; return -1; }

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  for (int i=0;i<PARTY_SLOTS;i++) party.releaseAt(i);
  for (int i=0;i<BOX_SLOTS;i++) party.box[i] = PartyMon();
  party.boxSave();

  // ---- a creature says goodbye: banked, one egg
  grownUp(6);                                  // CHARIZARD, final form, 3 days
  ck(pet.canFarewellNow() && pet.wantFarewellButton(), "a grown creature is offered its farewell");
  pet.startFarewell();
  int zard = finish();
  ck(zard >= 0 && party.slots[zard].dex == 6 && party.slots[zard].paidEgg(), "it is banked, marked as having given its egg");
  ck(pet.isEgg(), "and the egg arrives");
  int16_t eggT = pet.eggPeek();

  // ---- RAISE THIS ONE: back it comes, and the goodbye does NOT come back
  focusSwap((uint8_t)zard);
  ck(pet.speciesId == 6 && pet.eggPaid, "raised again, it remembers it already said goodbye");
  ck(!pet.canFarewellNow() && !pet.wantFarewellButton(), "and the farewell is never offered a second time");
  ck(pet.eggStashed && pet.stashTarget == eggT, "the waiting egg was set aside, not thrown away");
  {
    Pet q; q.begin();
    ck(q.eggPaid && q.eggStashed && q.stashTarget == eggT, "both survive a reboot");
  }

  // ---- ending it again hands back THAT egg, not a new one
  ck(pet.canRetireNow(), "with its egg waiting it may be retired");
  pet.startRetire();
  finish();
  ck(pet.isEgg() && pet.eggPeek() == eggT && !pet.eggStashed, "and the egg that comes back is the same one");
  ck(pet.evoPenalty() == 0, "with no retire penalty -- it already earned it");
  ck(slotOf(6) >= 0 && party.slots[slotOf(6)].paidEgg(), "and CHARIZARD is banked again, still marked -- never thrown away");

  // ---- the stash spent, and a paid creature raised over a CREATURE (not an egg)
  pet.dbgHatchAs(eggT >= 1 ? eggT : 1, false);   // the egg hatches
  while (pet.hasLearnOffer()) pet.declineLearn();
  zard = slotOf(6);
  focusSwap((uint8_t)zard);                     // CHARIZARD back; the new one goes to the party
  ck(pet.speciesId == 6 && !pet.eggStashed, "raised over a hatched creature: nothing to stash");
  ck(!pet.canRetireNow(), "so it can no longer be retired for a fresh egg");
  pet.release();
  ck(pet.ceremony == CER_NONE && pet.speciesId == 6, "nor released for one");
  ck(!pet.canFarewellNow(), "and the farewell still stays away");
  pet.ageMinutes += 3UL * 24 * 60;              // days later, still nothing
  ck(!pet.wantFarewellButton(), "however long it lives on");

  // ---- the one exception: nobody else left anywhere
  for (int i=0;i<PARTY_SLOTS;i++) party.releaseAt(i);
  ck(pet.canRetireNow(), "with the party and box empty it may end, so no one is ever stuck");

  // ---- a fresh creature is unaffected
  grownUp(9);
  pet.eggPaid = false;
  ck(pet.canFarewellNow(), "a creature that never gave an egg is still offered its farewell");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
