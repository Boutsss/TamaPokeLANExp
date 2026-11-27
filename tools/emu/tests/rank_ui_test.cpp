// Trainer rank on the real player card: tapping the avatar cycles ONLY through
// unlocked ones (the rule rankAvatarUnlocked() states), opening the card
// re-scores progress that arrived without an event, and an avatar worn from
// before ranks existed is never taken away.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "rank.h"
#include "avatars.h"
#include <cstdio>
uint32_t g_seed=43; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void onTap(int16_t,int16_t); void uiTileGo(int i);
extern Pet pet; extern bool playerOpen; extern uint8_t playerPage;
static int bad=0;
static void ck(bool ok,const char *s){printf("%s  %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(4, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  rankClearAll(); rankBegin();
  pet.avatar = 0;

  // ---- the avatar tap (page 0, the sprite under the name) skips locked ones
  uiTileGo(0); render();
  ck(playerOpen && playerPage == 0, "the PLAYER tile opens the card on page 0");
  // Since v3.51 the tap opens the avatar PICKER (what it offers, locks
  // included, is avpick_ui_test's); it no longer cycles in place.
  extern bool avPickOpen;
  onTap(233, 110); render();
  ck(avPickOpen && pet.avatar == 0, "tapping the avatar opens the picker, changing nothing yet");
  avPickOpen = false; playerOpen = true; playerPage = 0;

  // ---- opening the card re-scores progress that came without an event
  uint8_t r0 = rankShown();
  for (int i = 0; i < 9; i++) pet.badges |= (uint16_t)(1u << i);   // no EV_GYM_WIN
  playerOpen = false;
  uiTileGo(0); render();
  ck(rankShown() > r0, "re-opening the card raises the rank to what the badges are worth");
  ck(rankAvatarUnlocked(7), "which unlocks the rank-2 avatar (YOUNGSTER)");

  // ---- grandfathered: wearing a locked avatar from before ranks keeps it
  rankClearAll(); pet.badges = 0;
  pet.avatar = 6;                    // SAILOR needs rank 8
  rankBegin();
  ck(rankAvatarUnlocked(6), "an avatar already worn before ranks stays selectable");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
