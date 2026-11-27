// The trainer card's stars as LOGIC (v3.57): the rainbow star, counted from
// shiny catches, and a friend's card telling this board which of their stars
// are NEW since their previous card -- which then flash until acknowledged.
//
// Everything here goes through the real stars.cpp / card.cpp and their own
// NVS records, and every rule is checked from both sides: what must happen,
// and what must not (a plain catch is not a shiny one; a FIRST card has no
// "since last time", so nothing on it is new).
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "card.h"
#include "stars.h"
#include "game_events.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=37; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static TrainerCard friendCard(uint8_t stars) {
  TrainerCard c; c.fid = 0xBEEF; snprintf(c.name, sizeof c.name, "MISTY"); c.stars = stars;
  return c;
}

int main(){
  Pet p; p.begin();
  starsClearAll();
  starsBegin(&p);

  // ---- the rainbow star: 10 SHINY catches, counted for good
  ck(STAR_COUNT == 7 && STAR_SHINY == 6, "seven stars; the rainbow one appended after the six that shipped");
  for (int i = 0; i < 20; i++) gameEvent(EV_WILD_CATCH, 16);
  ck(starsShinyCatches() == 0 && !starHas(STAR_SHINY), "ordinary catches do not count");
  for (int i = 0; i < STAR_SHINY_CATCHES - 1; i++) gameEvent(EV_SHINY_CATCH, 16);
  ck(starsShinyCatches() == STAR_SHINY_CATCHES - 1 && !starHas(STAR_SHINY), "nine shinies: not yet");
  gameEvent(EV_SHINY_CATCH, 16);
  ck(starHas(STAR_SHINY), "the tenth earns the rainbow star");
  starsBegin(&p);   // a reload: count and star are on flash
  ck(starsShinyCatches() == STAR_SHINY_CATCHES && starHas(STAR_SHINY), "both survive a reload");
  {
    // a v3.56 record (4 bytes, no shiny count) loads with the count at 0
    nvs()["stars"] = { 1, 0x01, 5, 0 };
    starsBegin(&p);
    ck(starsShinyCatches() == 0 && starsLanWins() == 5 && starHas(0), "an older record keeps its stars and LAN wins, counting shinies from 0");
  }

  // ---- stars on the wire
  {
    TrainerCard c = friendCard(0x45), got;
    uint8_t w[CARD_WIRE_LEN];
    ck(cardToWire(c, w) == 33 && w[32] == 0x45, "a card carries its stars at byte 32");
    ck(cardFromWire(w, 33, got) && got.stars == 0x45, "and they read back");
    ck(cardFromWire(w, 32, got) && got.stars == 0, "a 32-byte card from an older board has none");
    w[32] = 0xFF;
    ck(cardFromWire(w, 33, got) && got.stars == STARS_MASK, "bits past the last star are dropped, never trusted");
  }

  // ---- the card book: NEW since their last card, until acknowledged
  {
    nvs().erase("cards");
    cardBookBegin();
    cardBookAdd(friendCard(0x03), 1);
    ck(cardBookAt(0).stars == 0x03 && cardBookAt(0).starsNew == 0, "a FIRST card: nothing is new (there is no last time)");
    cardBookAdd(friendCard(0x03 | 0x10), 1);
    ck(cardBookAt(0).starsNew == 0x10, "their next card: only the star earned since is new");
    cardBookAdd(friendCard(0x03 | 0x10), 1);
    ck(cardBookAt(0).starsNew == 0x10, "the same card again: still new, until the player has seen it");
    cardBookBegin();
    ck(cardBookCount() == 1 && cardBookAt(0).starsNew == 0x10, "an unacknowledged star survives a reload");
    ck(cardBookAck(0) && cardBookAt(0).starsNew == 0, "a tap acknowledges it");
    ck(!cardBookAck(0), "and there is nothing left to acknowledge");
    cardBookBegin();
    ck(cardBookAt(0).starsNew == 0 && cardBookAt(0).stars == 0x13, "acknowledged stays acknowledged across a reload");
    cardBookAdd(friendCard(0x13 | 0x40), 1);
    ck(cardBookAt(0).starsNew == 0x40, "a later star is new again");
    TrainerCard other = friendCard(0x7F); other.fid = 0xCAFE; snprintf(other.name, sizeof other.name, "BROCK");
    cardBookAdd(other, 1);
    ck(cardBookAt(0).fid == 0xCAFE && cardBookAt(0).starsNew == 0 && cardBookAt(1).starsNew == 0x40,
       "another friend's first card is not new, and MISTY's new star is still waiting");
  }

  // ---- a caught or traded shiny shows in the Pokedex, without being "raised"
  {
    Pet q;
    uint16_t before = q.registeredCount();
    ck(!q.isShinyRegistered(150) && !q.isRegistered(150), "MEWTWO: neither raised nor its shiny seen");
    ck(q.noteShiny(150), "a shiny MEWTWO caught: noted");
    ck(q.isShinyRegistered(150), "its shiny now shows in the Pokedex");
    ck(!q.isRegistered(150) && q.registeredCount() == before,
       "but it is NOT registered: that still means raised, and the count is unchanged");
    ck(!q.noteShiny(150), "a second one is not new");
    ck(!q.noteShiny(0) && !q.noteShiny(DEX_COUNT + 1), "and nothing outside the dex is marked");
  }

  printf(bad ? "stars_test: %d FAILED\n" : "stars_test: all passed\n", bad);
  return bad ? 1 : 0;
}
