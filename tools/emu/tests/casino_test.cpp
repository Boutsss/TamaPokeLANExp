// The Game Corner's rules, against the real casino.cpp: blackjack
// totals and payouts, High/Low's tie rule, the slot machine's long-run return
// (computed over EVERY stop, not sampled), and the prize shop's ordering --
// coins are spent BEFORE a prize is recorded, so a failed spend hands out
// nothing.
#include "Arduino.h"
#include "Preferences.h"
#include "casino.h"
#include "casino_art.h"
#include "avatars.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=71; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

// card helpers: rank r (0=A..12=K) in spades
static uint8_t C(uint8_t r){ return r; }
static uint16_t wallet = 0; static bool spendWorks = true; static int spends = 0;
static bool spend(uint16_t n){ spends++; if (!spendWorks || wallet < n) return false; wallet -= n; return true; }

int main(){
  // ---- blackjack totals
  { uint8_t h[]={C(0),C(12)}; bool s; ck(bjTotal(h,2,&s)==21 && s, "A + K is a soft 21"); }
  { uint8_t h[]={C(0),C(0),C(8)}; ck(bjTotal(h,3)==21, "A + A + 9 is 21 (one ace high)"); }
  { uint8_t h[]={C(0),C(0),C(0),C(0)}; ck(bjTotal(h,4)==14, "four aces are 14"); }
  { uint8_t h[]={C(9),C(10),C(11)}; ck(bjTotal(h,3)==30, "10 + J + Q is 30, a bust"); }
  { uint8_t h[]={C(0),C(5),C(9)}; bool s; ck(bjTotal(h,3,&s)==17 && !s, "A + 6 + 10 is a hard 17"); }

  // ---- blackjack payouts, across many seeded hands
  {
    int naturals=0, wins=0, pushes=0, losses=0; long net=0;
    randomSeed(5);
    for (int i=0;i<20000;i++){
      Blackjack b; b.deal(10);
      while (b.state==BJ_PLAYER && bjTotal(b.you,b.youN) < 17) b.hit();   // a simple strategy
      if (b.state==BJ_PLAYER) b.stand();
      if (b.state!=BJ_DONE){ bad++; break; }
      uint16_t p=b.payout();
      if (b.result==BJR_BLACKJACK){ naturals++; if (p!=25) { ck(false,"a natural pays 3:2"); } }
      if (b.result==BJR_WIN){ wins++; if (p!=20) ck(false,"a win pays even money"); }
      if (b.result==BJR_PUSH){ pushes++; if (p!=10) ck(false,"a push returns the bet"); }
      if (b.result==BJR_LOSE||b.result==BJR_BUST){ losses++; if (p!=0) ck(false,"a loss pays nothing"); }
      if (b.dealerN && b.result!=BJR_BLACKJACK && b.result!=BJR_BUST && bjTotal(b.dealer,b.dealerN) < 17)
        ck(false,"the dealer always draws to 17");
      net += (long)p - 10;
    }
    printf("      20000 hands: %d naturals %d wins %d pushes %d losses, return %.3f\n",
           naturals,wins,pushes,losses, 1.0 + net/200000.0);
    ck(naturals>600 && naturals<1300, "naturals come up about 1 hand in 21");
    ck(net < 0 && net > -40000, "the house wins in the long run, but not by a lot");
  }
  {
    Blackjack b; randomSeed(9);
    for (int i=0;i<500;i++){ b.deal(10); if (b.canDouble()) break; }
    ck(b.canDouble(), "a fresh two-card hand may double");
    b.doubleDown();
    ck(b.state==BJ_DONE && b.bet==20 && b.youN==3, "DOUBLE takes exactly one card at twice the bet");
  }

  // ---- high / low
  {
    HighLow h; randomSeed(3); h.start(10);
    h.card = C(12);                         // a KING showing
    ck(!h.guess(true) && h.state==HL_LOST && h.pot==0, "nothing is higher than a KING: HIGHER loses");
    h.start(10); h.card = C(0);             // an ACE
    ck(!h.guess(false), "nothing is lower than an ACE: LOWER loses");
    // a tie: force the next draw to the same rank
    h.start(10); h.card = C(6); h.deck.c[h.deck.top] = 13 + 6;   // the 7 of another suit
    ck(!h.guess(true) && h.state==HL_LOST, "a tie is never right -- the house edge");
    h.start(10); h.card = C(1); h.deck.c[h.deck.top] = C(9);
    ck(h.guess(true) && h.pot==20 && h.streak==1, "a right guess doubles the pot");
    ck(h.cashOut()==20 && h.state==HL_IDLE && h.cashOut()==0, "CASH OUT pays the pot once");
  }

  // ---- slots
  {
    float r = slotReturn();
    printf("      slot return over all %d stops: %.4f\n", SLOT_REEL_LEN*SLOT_REEL_LEN*SLOT_REEL_LEN, r);
    ck(r > 0.85f && r < 0.98f, "the slots keep a house edge between 2% and 15%");
    ck(slotPays(SL_SEVEN,SL_SEVEN,SL_SEVEN)==300 && slotPays(SL_CHERRY,SL_BAR,SL_BALL)==2 &&
       slotPays(SL_BAR,SL_CHERRY,SL_CHERRY)==0, "the paytable reads left to right");
    uint8_t s[3]; randomSeed(1);
    bool inRange = true; for (int i=0;i<1000;i++){ slotSpin(s); for (int k=0;k<3;k++) if (s[k]>=SLOT_REEL_LEN) inRange=false; }
    ck(inRange, "every stop lands on the strip");
  }

  // ---- roulette: every bet returns exactly 36/37 over the 37 pockets
  {
    bool seen[ROULETTE_POCKETS] = { false }, wheelOk = true;
    for (uint8_t i = 0; i < ROULETTE_POCKETS; i++) { if (ROULETTE_WHEEL[i] >= ROULETTE_POCKETS || seen[ROULETTE_WHEEL[i]]) wheelOk = false; else seen[ROULETTE_WHEEL[i]] = true; }
    ck(wheelOk, "the wheel holds every pocket 0..36 exactly once");
    uint8_t reds = 0; for (uint8_t n = 1; n <= 36; n++) reds += rouletteRed(n);
    ck(reds == 18 && !rouletteRed(0), "18 red, 18 black, and 0 is neither");
    bool alt = true;   // on a real wheel the colours alternate pocket to pocket
    for (uint8_t i = 1; i + 1 < ROULETTE_POCKETS; i++) if (rouletteRed(ROULETTE_WHEEL[i]) == rouletteRed(ROULETTE_WHEEL[i + 1])) alt = false;
    ck(alt, "red and black alternate round the wheel");
    bool exact = true;
    for (uint8_t b = 0; b < RB_COUNT; b++) {
      uint8_t args = b == RB_DOZEN ? 3 : b == RB_NUMBER ? 37 : 1;
      for (uint8_t a = 0; a < args; a++) {
        unsigned ret = 0; for (uint8_t n = 0; n < ROULETTE_POCKETS; n++) ret += roulettePays(b, a, n);
        if (ret != 36) { exact = false; printf("      bet %u arg %u returns %u/37\n", b, a, ret); }
      }
    }
    ck(exact, "every bet returns exactly 36 coins in 37 per coin: the zero is the house edge");
    ck(roulettePays(RB_RED, 0, 0) == 0 && roulettePays(RB_NUMBER, 0, 0) == 36, "0 loses red, and pays 35 to 1 on 0 itself");
    ck(roulettePays(RB_GREEN, 0, 0) == 36 && roulettePays(RB_GREEN, 0, 7) == 0, "GREEN pays 35 to 1 on the zero, nothing else");
    ck(roulettePays(RB_DOZEN, 2, 36) == 3 && roulettePays(RB_DOZEN, 0, 13) == 0, "dozens pay 2 to 1");
    randomSeed(4); bool inR = true; for (int i = 0; i < 2000; i++) if (rouletteSpin() >= ROULETTE_POCKETS) inR = false;
    ck(inR, "every spin lands in a pocket");
  }

  // ---- the prize shop
  {
    nvs().clear(); casinoBegin();
    ck(casinoOwns(PK_BACK,0) && casinoOwns(PK_CARDBG,0) && !casinoOwns(PK_AVATAR,0), "only the free defaults are owned at first");
    uint16_t coins = CASINO_PRICE - 1; wallet = coins;
    ck(!casinoBuy(PK_BACK,1,coins,spend) && !casinoOwns(PK_BACK,1), "9998 coins buy nothing");
    coins = wallet = 20000; spendWorks = false; spends = 0;
    ck(!casinoBuy(PK_BACK,1,coins,spend) && spends==1 && !casinoOwns(PK_BACK,1),
       "a spend that does not land records no prize");
    spendWorks = true;
    ck(casinoBuy(PK_BACK,1,coins,spend) && wallet==20000-CASINO_PRICE && casinoOwns(PK_BACK,1), "TEAM ROCKET back bought for 9999");
    coins = wallet;
    ck(!casinoBuy(PK_BACK,1,coins,spend), "and not twice");
    ck(casinoBuy(PK_AVATAR,SHOP_AVATAR_COUNT-1,coins,spend) || wallet < CASINO_PRICE, "the last shop avatar is buyable");
    casinoEquip(PK_CARDBG, 5);
    ck(casino.bg == 0, "an unowned background cannot be equipped");
    casinoEquip(PK_BACK, 1);
    casinoBegin();
    ck(casinoOwns(PK_BACK,1) && casino.back==1, "ownership and the equipped back survive a reload");
    ck(casinoOwns(PK_AVATAR,SHOP_AVATAR_COUNT-1), "and the avatar");
    ck(casinoShopCount(PK_BACK)==CARD_BACK_COUNT && casinoShopCount(PK_CARDBG)==CARD_BG_COUNT &&
       casinoShopCount(PK_AVATAR)==SHOP_AVATAR_COUNT - SHOP_AVATAR_RETIRED_COUNT,
       "the shop lists every design there is, bar the retired ones");
    // RANK AVATARS ARE EARNED, NOT BOUGHT: nothing the shop lists may draw the
    // same as a rank avatar. Checked on the PIXELS, so a new duplicate fails
    // here whatever it is called.
    {
      int dup = 0;
      for (uint8_t p = 0; p < casinoShopCount(PK_AVATAR); p++) {
        int i = casinoShopAt(PK_AVATAR, p);
        for (uint8_t b = 0; b < AVATAR_BASE_COUNT; b++)
          if (!memcmp(AVATARS[SHOP_AVATAR_FIRST + i].idx, AVATARS[b].idx, AVATAR_PX * AVATAR_PX)) {
            printf("      the shop lists %s, which is the rank avatar %s\n", SHOP_AVATAR_NAMES[i], AVATARS[b].name);
            dup++;
          }
      }
      ck(dup == 0, "no listed shop avatar is a rank avatar");
      int champ = -1;
      for (uint8_t i = 0; i < SHOP_AVATAR_COUNT; i++) if (!strcmp(SHOP_AVATAR_NAMES[i], "CHAMP RED")) champ = i;
      bool listed = false;
      for (uint8_t p = 0; p < casinoShopCount(PK_AVATAR); p++) if (casinoShopAt(PK_AVATAR, p) == champ) listed = true;
      uint16_t c2 = 60000;
      ck(champ >= 0 && casinoPrizeRetired(PK_AVATAR, champ) && !listed && !casinoBuy(PK_AVATAR, champ, c2, spend) && c2 == 60000,
         "CHAMP RED (the same sprite as RED) is not listed and cannot be bought");
      ck(casinoShopAt(PK_AVATAR, casinoShopCount(PK_AVATAR)) == -1, "the list ends where it says it does");
    }
    bool ascii = true;
    for (uint8_t k=0;k<PK_KINDS;k++) for (uint8_t i=0;i<casinoPrizeCount(k);i++)
      for (const char *c = casinoPrizeName(k,i); *c; c++) if ((uint8_t)*c > 126) ascii = false;
    ck(ascii, "every prize name is plain ASCII (the bitmap font has nothing else)");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
