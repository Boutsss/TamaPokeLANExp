// Held items must travel with their creature, survive save/restore, and obey
// the bag's durable transfer rules.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "inventory.h"
#include "held_items.h"
#include <cstdio>
#include <cstdlib>

uint32_t g_seed = 71;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

int main() {
  Pet p; p.begin();
  if (p.isEgg()) p.dbgHatchAs(6, false);
  Party pt; pt.begin();
  bag.begin();
  bag.clear();

  ck(bag.add(IT_ORAN_BERRY, 2) == 2, "bag accepts two Oran Berries");
  ck(heldGive(p, pt, HOLDER_PET, IT_ORAN_BERRY), "equip a held item on the live pet");
  ck(p.held == IT_ORAN_BERRY && bag.count(IT_ORAN_BERRY) == 1,
     "equipping removes exactly one from the bag");
  Pet reload; reload.begin();
  ck(reload.held == IT_ORAN_BERRY, "live pet held item survives checkpoint reload");

  PartyMon m; m.dex = 25; m.level = 20;
  ck(pt.add(m), "add a party creature");
  ck(bag.add(IT_LEFTOVERS, 1) == 1, "bag accepts Leftovers");
  ck(heldGive(p, pt, HOLDER_PARTY(0), IT_LEFTOVERS),
     "equip a held item on a party creature");
  Party ptReload; ptReload.begin();
  ck(ptReload.slots[0].held == IT_LEFTOVERS,
     "party held item survives record reload");

  ck(heldTake(p, pt, HOLDER_PET), "take the live pet's item back");
  ck(p.held == ITEM_NONE && bag.count(IT_ORAN_BERRY) == 2,
     "taking an item restores it to the bag");
  pt.releaseAt(0);
  ck(bag.count(IT_LEFTOVERS) == 1, "releasing a party creature returns its held item");

  ck(bag.add(IT_SHINY_CHARM, 2) == 1 && bag.count(IT_SHINY_CHARM) == 1,
     "key items are unique in the bag");
  ck(!bag.consume(IT_SHINY_CHARM, 1), "key items cannot be consumed");
  ck(shinyCharmOdds(48) == 24 && shinyCharmOdds(5) == 4,
     "Shiny Charm halves egg odds with a minimum denominator of four");
  ck(bag.consume(IT_SHINY_CHARM, 1) == false, "Shiny Charm remains owned");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
