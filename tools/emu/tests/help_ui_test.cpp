// The how-to-play card through the REAL screens: a first game opens it straight
// after the starter is TAPPED (the caller, not just the rule), it pages with its
// own button AND on the rim, horizontal does nothing, a miss does nothing, GOT IT
// closes it for good on this save (a fresh boot does not repeat it, WIPE does),
// the menu's last row reopens it, down closes it -- and every line of every page,
// in all six languages, fits the round panel at the row it is drawn on.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "i18n.h"
#include <cstdio>
#include <cstring>
#include <cmath>
uint32_t g_seed=107; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void loop(); void render(); void onTap(int16_t,int16_t); void onSwipe(int); void onSwipeV(int); void onRim(int);
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
extern Pet pet; extern bool helpOpen, menuOpen; extern uint8_t helpPage;
uint8_t helpPages();
void helpBtn(int *x, int *y, int *w, int *h);
void helpOfferFirstTime();
int uiMenuRowCenterY(int i);
int uiSafeHalfWidthFor(int y0, int y1);
int uiTapFinger();
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static bool onScreen(const char *n){ return !strcmp(SCREEN_NAME[uiCurrentScreen()], n); }
static void tapBtn(){ int x,y,w,h; helpBtn(&x,&y,&w,&h); onTap(x+w/2, y+h/2); render(); }
// where renderRegionPick / renderStarterSelect draw row i (as starter_test)
static int regionRowY(int i){ return 108 + i*72 + 30; }
static int starterRowY(int i){ return 110 + i*(70+8) + 35; }
static uint8_t helpKey(){ auto it = nvs().find("help"); return it == nvs().end() || it->second.empty() ? 0 : it->second[0]; }

int main(){
  setup();
  render();
  ck(pet.awaitingStarter(), "a brand new save");
  onTap(233, regionRowY(0)); render();
  onTap(233, starterRowY(0)); render();
  ck(!pet.awaitingStarter() && onScreen("help") && helpPage == 0,
     "tapping the first starter opens the how-to-play card on page 1");

  // the button: finger-sized, corners on the glass
  { int x,y,w,h; helpBtn(&x,&y,&w,&h);
    bool on = true;
    int cs[4][2] = { {x,y}, {x+w,y}, {x,y+h}, {x+w,y+h} };
    for (auto &p : cs) if (hypot(p[0]-233, p[1]-233) > 230) on = false;
    ck(h >= uiTapFinger() && w >= uiTapFinger() && on, "NEXT is finger-sized and on the glass"); }

  tapBtn();
  ck(onScreen("help") && helpPage == 1, "NEXT turns the page");
  onRim(1); render();
  ck(onScreen("help") && helpPage == 2, "and so does the rim");
  onSwipe(-1); render(); onSwipe(1); render();
  ck(onScreen("help") && helpPage == 2, "horizontal does nothing on the card");
  onSwipeV(-1); render();
  ck(onScreen("help") && helpPage == 2, "nor does up");
  onTap(233, 120); render();
  ck(onScreen("help") && helpPage == 2, "a tap off the button does nothing");
  ck(helpKey() == 0, "nothing is marked seen before the end");
  tapBtn();
  ck(helpPage == helpPages() - 1, "to the last page");
  tapBtn();
  ck(onScreen("main") && !helpOpen, "GOT IT closes it, onto the pet");
  ck(helpKey() == 1, "and it is remembered");
  helpOfferFirstTime();
  ck(!helpOpen, "so a later first-game hook does not show it again");

  // the menu's last row reopens it; down closes it (a hatched creature: an
  // egg's screen has no menu)
  pet.dbgHatchAs(25, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  render();
  onTap(233, 60); render();
  ck(onScreen("menu"), "the menu");
  onTap(233, uiMenuRowCenterY(6)); render();
  ck(onScreen("help") && helpPage == 0, "its last row is HOW TO PLAY, from page 1");
  onRim(1); render();
  onSwipeV(1); render();
  ck(onScreen("main"), "down closes the card");

  // every line of every page fits the chord at the row it is drawn on
  int worst = 999; char where[64] = "";
  for (int l = 0; l < LANG_COUNT; l++) {
    setLang((Lang)l);
    for (int pg = 0; pg < helpPages(); pg++) {
      const char *b = T((StrId)(S_HELP_B1 + pg));
      int y = 140, lines = 0;
      while (*b) {
        const char *e = strchr(b, '\n');
        int len = e ? (int)(e - b) : (int)strlen(b);
        int slack = uiSafeHalfWidthFor(y, y + 16) - len * 6;
        if (slack < worst) { worst = slack; snprintf(where, sizeof(where), "lang %d page %d line %d", l, pg, lines); }
        lines++; y += 34;
        if (!e) break;
        b = e + 1;
      }
      int bx, by, bw, bh; helpBtn(&bx, &by, &bw, &bh);
      if (140 + (lines - 1) * 34 + 16 > by) { worst = -1; snprintf(where, sizeof(where), "lang %d page %d runs into the button", l, pg); }
      const char *t = T((StrId)(S_HELP_T1 + pg));
      int ts = uiSafeHalfWidthFor(84, 108) - (int)strlen(t) * 9;
      if (ts < worst) { worst = ts; snprintf(where, sizeof(where), "lang %d page %d title", l, pg); }
    }
  }
  setLang(LANG_EN);
  printf("      tightest fit: %d px (%s)\n", worst, where);
  ck(worst >= 8, "every page, in every language, fits the panel with room to spare");

  // WIPE brings it back for the next first game
  pet.factoryReset();
  ck(helpKey() == 0, "WIPE forgets it");
  helpOfferFirstTime();
  ck(helpOpen, "so the next first game shows it again");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
