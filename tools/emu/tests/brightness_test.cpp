// The MAXIMUM brightness setting (settings screen, NVS "bri").
//
// brightnessTarget() is THE rule, asked by the panel and by this test: at the
// top level the panel is exactly what it always was; each level lower is
// dimmer; and sleeping and the idle dims still happen UNDER the cap -- never
// brighter than the awake level the player chose. The settings row is driven
// through clockTap(), and the choice must survive a reboot.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "audio.h"
#include <cstdio>
extern Arduino_Canvas *gfx;
uint32_t g_seed=61; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void clockTap(int16_t,int16_t);
uint8_t brightnessTarget(bool sleeping, bool usb, uint8_t dim, uint8_t level);
uint8_t maxBrightness(); void setMaxBrightness(uint8_t); void loadMaxBrightness();
extern bool clockOpen;
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  ck(maxBrightness() == 10, "a fresh board is at full brightness");
  ck(audioVolume() == 1 && AUDIO_VOL_DEFAULT == 1, "and its sound volume starts at 1 (v3.39)");

  // level 10 is the panel as it always was
  ck(brightnessTarget(false, true, 0, 10) == 180 && brightnessTarget(false, false, 0, 10) == 145,
     "at the top level, awake is unchanged (180 on USB, 145 on battery)");
  ck(brightnessTarget(true, true, 0, 10) == 25 && brightnessTarget(false, true, 1, 10) == 60 &&
     brightnessTarget(true, true, 1, 10) == 10 && brightnessTarget(false, true, 2, 10) == 8,
     "and so are sleeping and both idle dims");

  // every level lower is dimmer, on USB and on battery
  bool mono = true;
  for (uint8_t l = 2; l <= 10; l++)
    for (int usb = 0; usb < 2; usb++)
      if (brightnessTarget(false, usb, 0, l) <= brightnessTarget(false, usb, 0, l - 1)) mono = false;
  ck(mono, "each level is brighter than the one below it");

  // the dims still happen, and never above the cap
  bool under = true, dims = true;
  for (uint8_t l = 1; l <= 10; l++)
    for (int usb = 0; usb < 2; usb++) {
      uint8_t awake = brightnessTarget(false, usb, 0, l);
      for (int sl = 0; sl < 2; sl++) for (uint8_t d = 0; d < 3; d++)
        if (brightnessTarget(sl, usb, d, l) > awake) under = false;
      if (brightnessTarget(true, usb, 0, l) >= awake) dims = false;
      if (brightnessTarget(false, usb, 2, l) > 8) dims = false;
    }
  ck(under, "sleeping and idle are never brighter than the chosen maximum");
  ck(dims, "and sleeping still dims the panel below it, at every level");

  // the settings row
  clockOpen = true; render();
  // the row's - and + : the same columns as the volume row, at its own height
  extern const int BRI_TEST_MINUS_X, BRI_TEST_PLUS_X, BRI_TEST_Y;
  for (int i = 0; i < 4; i++) clockTap(BRI_TEST_MINUS_X, BRI_TEST_Y);
  ck(maxBrightness() == 6, "the - button lowers it");
  for (int i = 0; i < 20; i++) clockTap(BRI_TEST_MINUS_X, BRI_TEST_Y);
  ck(maxBrightness() == 1, "and stops at 1 (never off)");
  clockTap(BRI_TEST_PLUS_X, BRI_TEST_Y); clockTap(BRI_TEST_PLUS_X, BRI_TEST_Y);
  ck(maxBrightness() == 3, "the + button raises it");
  loadMaxBrightness();
  ck(maxBrightness() == 3, "and it survives a reboot");
  for (int i = 0; i < 20; i++) clockTap(BRI_TEST_PLUS_X, BRI_TEST_Y);
  ck(maxBrightness() == 10, "and stops at 10");
  ck(clockOpen, "the settings screen stays open throughout");

  // The firmware version is on this screen so a player can check their build.
  // Measured off the drawn pixels: it must be at least size-2 tall (it was
  // size 1, ~1 mm on the panel, and nobody found it) and stay on the glass.
  {
    extern const int CLK_TEST_VER_Y;
    void renderClock();
    renderClock();   // the settings screen itself: this harness never picks a starter,
                     // so render() would draw the first-boot chooser instead
    const uint16_t *fb = gfx->buffer();
    uint16_t bg = fb[(CLK_TEST_VER_Y - 2) * 466 + 233];
    int top = -1, bottom = -1, left = 466, right = -1;
    for (int y = CLK_TEST_VER_Y - 2; y < CLK_TEST_VER_Y + 22 && y < 466; y++)
      for (int x = 0; x < 466; x++) {
        int dx = x - 233, dy = y - 233;
        if (dx * dx + dy * dy > 228 * 228) continue;          // off the glass: the bezel
        if (fb[y * 466 + x] == bg) continue;
        if (top < 0) top = y;
        bottom = y;
        if (x < left) left = x;
        if (x > right) right = x;
      }
    ck(top >= 0 && bottom - top + 1 >= 12, "the version line is drawn at least size-2 tall");
    int ok = 1;
    for (int y = top; y >= 0 && y <= bottom; y++) {
      int dy = y - 233, half = 0;
      while ((half + 1) * (half + 1) + dy * dy <= 226 * 226) half++;
      if (left < 233 - half || right > 233 + half) ok = 0;
    }
    ck(ok && right > left, "and it stays inside the round glass");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
