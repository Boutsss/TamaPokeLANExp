// The bag's item descriptions (v3.57) are GENERATED from each item's effect
// code (held_items.cpp itemDescribe), so a new item gets one for free -- and
// an item whose effect the generator does not know would get NONE. This walks
// the whole catalogue: every item says something, in ASCII (the panel font has
// nothing else), short enough to sit under its name in a bag row.
#include "Arduino.h"
#include "Preferences.h"
#include "items.h"
#include "held_items.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=5; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

int main(){
  int empty = 0, long_ = 0, nonAscii = 0, longest = 0;
  char worst[64] = "";
  for (uint16_t k = 1; k < ITEM_COUNT; k++) {
    char d[64];
    itemDescribe((ItemKey)k, d, sizeof d);   // a roomy buffer: truncation must not hide a long one
    int n = (int)strlen(d);
    if (!n) { empty++; printf("      no description: %s\n", itemEntry((ItemKey)k).name); }
    if (n > ITEM_DESC_MAX) { long_++; printf("      too long (%d): %s -> %s\n", n, itemEntry((ItemKey)k).name, d); }
    for (const char *p = d; *p; p++) if ((unsigned char)*p < 32 || (unsigned char)*p > 126) { nonAscii++; break; }
    if (n > longest) { longest = n; snprintf(worst, sizeof worst, "%s", d); }
  }
  printf("      %d items, longest description %d chars: \"%s\"\n", ITEM_COUNT - 1, longest, worst);
  ck(empty == 0, "every item in the catalogue has a description");
  ck(long_ == 0, "each fits under its name in a bag row (ITEM_DESC_MAX)");
  ck(nonAscii == 0, "and each is plain ASCII, all the panel font can draw");
  char d[64];
  itemDescribe(IT_CHARCOAL, d, sizeof d);
  ck(strstr(d, "FIRE") != nullptr, "CHARCOAL names the type it boosts, read from its own row");
  itemDescribe(IT_ZAP_PLATE, d, sizeof d);
  ck(strstr(d, "ELECTRIC") && strstr(d, "ARCEUS"), "a plate says what it does to ARCEUS");
  itemDescribe(ITEM_NONE, d, sizeof d);
  ck(d[0] == 0, "nothing for no item");
  printf(bad ? "itemdesc_test: %d FAILED\n" : "itemdesc_test: all passed\n", bad);
  return bad ? 1 : 0;
}
