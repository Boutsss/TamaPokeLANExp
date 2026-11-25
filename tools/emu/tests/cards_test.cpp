// Trainer-card exchange through the REAL screens: the player card's TRAINER
// CARDS page -> EXCHANGE opens FRIENDS, two boards pair, each files the
// other's card in its book (and never its own), a forged card off the wire is
// cleaned before it can index anything, the book pages on the rim and
// survives a reload, and down walks back to the player card.
//
// And the guarantee the design rests on: a card is sent UNKEPT, so it never
// takes the gift swap's resend slot. A swap on a lossy link must still finish
// while cards are flying -- negative-checked by sending cards kept instead.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "items.h"
#include "inventory.h"
#include "link.h"
#include "friends.h"
#include "card.h"
#include "casino.h"
#include "casino_art.h"
#include "avatar_names.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=67; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void loop(); void render(); void onTap(int16_t,int16_t); void onSwipeV(int); void onRim(int);
uint8_t uiCurrentScreen(); uint16_t uiRimPages();
extern const char *const SCREEN_NAME[];
extern Pet pet; extern Link lan; extern bool playerOpen, friendsOpen, cardBookOpen, friendsGotCard;
extern uint8_t playerPage, cardBookPage;
extern bool g_emuRadioUp;
int pcBtnCenterY(uint8_t i); int frRowCenterY(uint8_t r);
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}
static bool onScreen(const char *n){ return !strcmp(SCREEN_NAME[uiCurrentScreen()], n); }

static Link P; static Friends PF; static Inventory PB; static Pet PP;
static NvsStore mine, theirs;
static int dropEvery = 0, seen = 0;
static void toPeer(void*, const uint8_t *b, uint8_t n){ if (dropEvery && (++seen % dropEvery)==0) return; uint8_t t[260]; memcpy(t,b,n); P.onPacket(t,n); }
static void toMe(void*, const uint8_t *b, uint8_t n){ if (dropEvery && (++seen % dropEvery)==0) return; uint8_t t[260]; memcpy(t,b,n); lan.onPacket(t,n); }
static TrainerCard peerCard;
static uint32_t peerCardAt = 0;
static void peerPoll() {             // the other board, on its own flash, sending its card like ours does
  mine = nvs(); nvs() = theirs;
  PF.poll(P, PP, PB, pet.currentDay(), millis());
  if (P.ready() && millis() - peerCardAt > 1500) {
    peerCardAt = millis();
    uint8_t w[CARD_WIRE_LEN]; P.sendCard(w, cardToWire(peerCard, w));
  }
  theirs = nvs(); nvs() = mine;
}
static void run(int ms) { for (int t = 0; t < ms; t += 100) { P.tick(millis()); loop(); peerPoll(); render(); } }
static void waitMs(int ms) { unsigned long t0 = millis(); while (millis() - t0 < (unsigned long)ms) { P.tick(millis()); loop(); peerPoll(); } }

int main(){
  setup();
  for (int i=0;i<3;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.dbgHatchAs(133, false);
  while (pet.hasLearnOffer()) pet.declineLearn();
  snprintf(pet.trainerName, sizeof(pet.trainerName), "ALI");
  pet.avatar = SHOP_AVATAR_FIRST + 3;               // LANCE, bought at the casino
  casino.bg = 2;                                     // FIRE
  bag.clear(); bag.add(IT_ORAN_BERRY, 2);
  PP.dbgHatchAs(25, true);
  peerCard.fid = 0x5151; snprintf(peerCard.name, sizeof(peerCard.name), "BEA");
  peerCard.avatar = SHOP_AVATAR_FIRST + 60; peerCard.bg = 46; peerCard.rank = 4;
  peerCard.badges = 11; peerCard.dex = 87; peerCard.medals = 9; peerCard.petDex = 25; peerCard.petShiny = 1;
  mine = nvs(); nvs().clear(); PB.clear(); PB.add(IT_POKEBALL, 1); theirs = nvs(); nvs() = mine;

  // ---- the player card's last page
  playerOpen = true; render();
  uint8_t last = uiRimPages() - 1;
  for (int i = 0; i < 20; i++) onRim(1);
  ck(playerPage == last, "the TRAINER CARDS page is the player card's last page");
  render();
  onTap(233, pcBtnCenterY(0)); render();
  ck(friendsOpen && onScreen("friends"), "EXCHANGE opens the FRIENDS screen");

  // ---- pair, and the cards cross
  g_emuRadioUp = true;
  lan.send = toPeer; lan.id = 0x1111;
  P.begin(false, "BEA"); P.send = toMe; P.id = 0x2222;
  PF.sessionStart(P, PP); P.tick(millis());
  onTap(233, frRowCenterY(0)); render();             // HOST
  P.start();
  waitMs(2200);
  ck(lan.ready() && friendsGotCard, "paired: the friend's card arrives");
  ck(cardBookCount() == 1 && !strcmp(cardBookAt(0).name, "BEA") && cardBookAt(0).avatar == SHOP_AVATAR_FIRST + 60 &&
     cardBookAt(0).bg == 46 && cardBookAt(0).badges == 11 && cardBookAt(0).petShiny,
     "and is filed in the book, exactly");
  {
    TrainerCard got;
    bool ok = P.cxN && cardFromWire(P.cx, P.cxN, got);
    ck(ok && !strcmp(got.name, "ALI") && got.avatar == SHOP_AVATAR_FIRST + 3 && got.bg == 2 && got.petDex == 133,
       "and ours reached them: our name, casino avatar and card background");
  }
  int w0 = nvsBlobWrites()["cards"];
  waitMs(4000);
  ck(cardBookCount() == 1, "the same card arriving again is not filed twice");
  ck(nvsBlobWrites()["cards"] == w0, "nor re-written to flash on every repeat (it arrives every 1.5 s)");

  // ---- the gift swap still works with cards flying, on a lossy link
  dropEvery = 3;
  onTap(233, frRowCenterY(0)); render();             // GIFT -> chooser
  onTap(233, frRowCenterY(1)); render();             // OFFER (ORAN BERRY)
  mine = nvs(); nvs() = theirs; PF.offerGift(P, PB, IT_POKEBALL, pet.currentDay()); theirs = nvs(); nvs() = mine;
  waitMs(6000);
  ck(bag.count(IT_POKEBALL) == 1 && bag.count(IT_ORAN_BERRY) == 1,
     "a gift swap on a lossy link still completes while cards are being sent");
  dropEvery = 0;

  // ---- down walks back to the player card
  onSwipeV(1); render();
  ck(!friendsOpen && playerOpen && playerPage == last, "down: back to the TRAINER CARDS page, not the LAN screen");

  // ---- a forged card off the wire is cleaned
  {
    uint8_t w[CARD_WIRE_LEN]; TrainerCard f; f.fid = 0x7777; snprintf(f.name, sizeof f.name, "EVIL");
    cardToWire(f, w);
    memset(w, 'A', CARD_NAME_LEN); w[3] = (uint8_t)0xFF;   // no terminator anywhere, and a non-ASCII byte
    w[12] = 250; w[13] = 250; w[14] = 99; w[20] = 0xFF; w[21] = 0x7F;   // avatar, bg, rank, dex out of range
    TrainerCard c;
    ck(cardFromWire(w, CARD_WIRE_LEN, c) && c.avatar < AVATAR_COUNT && c.bg < CARD_BG_COUNT && c.rank <= 10 &&
       c.petDex == 0 && strlen(c.name) == CARD_NAME_LEN - 1 && c.name[3] == '?',
       "a forged card is cleaned: name terminated and ASCII, every index in range");
    ck(!cardFromWire(w, CARD_WIRE_LEN - 1, c), "and a short one is refused outright");
    ck(!cardBookAdd(cardMine(pet, 0, 0, 1, 0x1234), 0x1234), "our own card is never filed");
    cardBookAdd(c, 0x1234);
  }

  // ---- the book
  onTap(233, pcBtnCenterY(1)); render();
  ck(cardBookOpen && onScreen("cardbook"), "CARD BOOK opens the book");
  ck(uiRimPages() == 2 && cardBookPage == 0, "one page per card");
  onRim(1); render();
  ck(cardBookPage == 1 && cardBookOpen, "it pages on the rim");
  onSwipeV(1); render();
  ck(!cardBookOpen && playerOpen && playerPage == last, "down: back to the player card");
  cardBookBegin();
  ck(cardBookCount() == 2 && !strcmp(cardBookAt(1).name, "BEA"), "the book survives a reload");

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
