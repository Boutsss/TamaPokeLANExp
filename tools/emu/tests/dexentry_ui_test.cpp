// (v3.42) Pokedex entries from the SD card, through the real gallery: swipe up
// on a registered species opens its entry (category, height, description, in
// the UI language), down comes back, an unregistered one stays a mystery, and
// the one parser refuses a damaged file rather than reading past it.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "i18n.h"
#include "sdmon.h"
#include "dexinfo.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=61; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void onSwipeV(int dir);
extern Pet pet; extern Arduino_Canvas *gfx;
extern bool galleryOpen, galleryPick, galleryEntry; extern int16_t galleryDetail;
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static uint32_t frameSum() { uint32_t h = 0; const uint16_t *b = gfx->buffer(); for (int i = 0; i < 466 * 466; i++) h = h * 31 + b[i]; return h; }
static const uint8_t *mem; static uint32_t memN;
static uint32_t memRead(void *, uint32_t off, void *dst, uint32_t n) {
  if (off >= memN) return 0; uint32_t k = memN - off < n ? memN - off : n; memcpy(dst, mem + off, k); return k;
}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  setLang(LANG_EN);
  DexEntryText e;
  ck(sdDexEntry(1, LANG_EN, e) && !strcmp(e.genus, "Seed Pokemon") && e.heightDm == 7 && strstr(e.text, "seed"),
     "BULBASAUR's entry reads off the card: Seed Pokemon, 0.7 m, its description");
  ck(sdDexEntry(1, LANG_ES, e) && !strcmp(e.genus, "Pokemon Semilla"), "and in Spanish too");
  ck(sdDexEntry(1025, LANG_DE, e) && e.genus[0], "the last species has one");
  ck(!sdDexEntry(0, LANG_EN, e) && !sdDexEntry(1026, LANG_EN, e), "no species 0, nor past the dex");

  // a damaged file: refused, never read past
  static uint8_t junk[64] = { 'T','D','X','1', 0xFF, 0xFF, 6, 0 };
  mem = junk; memN = sizeof(junk);
  ck(!dexInfoRead(memRead, nullptr, 500, LANG_EN, e), "a truncated file is refused for a species it does not reach");
  static uint8_t notdex[16] = { 'N','O','P','E' }; mem = notdex; memN = sizeof(notdex);
  ck(!dexInfoRead(memRead, nullptr, 1, LANG_EN, e), "and a file that is not one at all");

  // through the screen
  pet.dbgRegister(25);
  galleryOpen = true; galleryPick = false; galleryDetail = 25;
  render(); uint32_t detail = frameSum();
  onSwipeV(-1); render();
  ck(galleryEntry, "swipe up on a registered PIKACHU opens its entry");
  ck(frameSum() != detail, "and the panel shows a different page");
  onSwipeV(1); render();
  ck(!galleryEntry && galleryDetail == 25, "swipe down comes back to the creature");
  galleryDetail = 150;
  ck(!pet.isRegistered(150), "MEWTWO is not registered");
  onSwipeV(-1); render();
  ck(!galleryEntry, "so its entry stays a mystery");
  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
