// Game Corner coins are 32-bit and capped at COIN_CAP (10,000,000) since v3.46.
// The old 16-bit fields keep a downgrade-safe value and the full count is
// APPENDED -- the player checkpoint's v5 tail and the "coin2" key -- the same
// rule as the avatar index (avatar16_test) and the move IDs.
//
// As there, the read checks FORGE a record whose 16-bit field disagrees with
// the wide one, because a count under 65,536 fits both and would pass
// whichever was read.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "casino.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=93; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static uint16_t rd16(const uint8_t *b){ return (uint16_t)(b[0] | (b[1] << 8)); }
static uint32_t rd32(const uint8_t *b){ return b[0] | (b[1]<<8) | (b[2]<<16) | ((uint32_t)b[3]<<24); }
static void wr16(uint8_t *b, uint16_t v){ b[0]=(uint8_t)v; b[1]=(uint8_t)(v>>8); }
static void wr32(uint8_t *b, uint32_t v){ for(int i=0;i<4;i++) b[i]=(uint8_t)(v>>(8*i)); }
static uint16_t crc16(const uint8_t *d, size_t n){   // CRC-16/CCITT-FALSE
  uint16_t c=0xFFFF;
  for(size_t i=0;i<n;i++){ c^=(uint16_t)d[i]<<8; for(int k=0;k<8;k++) c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1); }
  return c;
}
static void reseal(std::vector<uint8_t> &b){ wr16(&b[6], (uint16_t)b.size()); wr16(&b[b.size()-2], crc16(b.data(), b.size()-2)); }
static const char *newestPly(){
  NvsStore &s = nvs();
  bool a = s.count("plyA"), b = s.count("plyB");
  if (a && b) return (int32_t)(rd32(&s["plyA"][8]) - rd32(&s["plyB"][8])) > 0 ? "plyA" : "plyB";
  return a ? "plyA" : "plyB";
}
// The fixed scalars and variable arrays, then wallet(4) steps(4) coins16(2)
// avatar(2) coins32(4): the offsets below are from the end of the arrays.
static size_t arraysEnd(const std::vector<uint8_t> &b){ return 42 + 2 * rd16(&b[12]) + 4 * b[15] + 2 * b[14] + b[16]; }

int main(){
  static_assert(sizeof(Pet::coins) == 4, "coins are 32-bit");
  static_assert(COIN_CAP == 10000000UL, "the cap the owner asked for");
  Pet pet; pet.begin(); party.begin();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // ---- writing
  const uint32_t BIG = 1234567;
  pet.coins = BIG; ck(pet.saveNow(), "a save with 1,234,567 coins lands");
  {
    auto &b = nvs()[newestPly()];
    size_t e = arraysEnd(b);
    ck(rd16(&b[4]) >= 5 && rd16(&b[e + 8]) == 0xFFFF && rd32(&b[e + 12]) == BIG,
       "the checkpoint keeps 65535 in its 16-bit field (an old build clamps that) and the count in its v5 tail");
    ck(!nvs().count("coin") && !nvs().count("coin2"), "and no legacy coin key is written any more (v3.50)");
  }
  { Pet q; q.begin(); ck(q.coins == BIG, "and 1,234,567 coins come back"); }

  // ---- reading: forge the 16-bit field to disagree with the tail
  {
    std::vector<uint8_t> b = nvs()[newestPly()];
    size_t e = arraysEnd(b);
    wr16(&b[e + 8], 50); wr32(&b[e + 12], 1000000); reseal(b);
    nvs()["plyA"] = b; nvs()["plyB"] = b;
    nvs()["coin"] = { 7, 0 }; nvs()["coin2"] = { 8, 0, 0, 0 };   // stale legacy keys
    Pet p; p.begin();
    ck(p.coins == 1000000, "the v5 tail is what loads");
    std::vector<uint8_t> v4(b.begin(), b.begin() + e + 12);   // a v4 record ends before it
    v4.push_back(0); v4.push_back(0); reseal(v4);
    nvs()["plyA"] = v4; nvs()["plyB"] = v4;
    Pet q; q.begin();
    ck(q.coins == 50, "a v4 record still loads its 16-bit coins");
    wr32(&b[e + 12], COIN_CAP + 5); reseal(b);
    nvs()["plyA"] = b; nvs()["plyB"] = b;
    Pet r; r.begin();
    ck(r.coins == COIN_CAP, "a count past the cap is clamped on load");
  }
  {
    nvs().erase("plyA"); nvs().erase("plyB");
    nvs()["coin"] = { 50, 0 }; nvs()["coin2"] = { 0x80, 0x84, 0x1E, 0x00 };   // 2,000,000: a pre-checkpoint save
    Pet p; p.begin();
    ck(p.coins == 2000000, "with no checkpoint, coin2 wins over coin");
    nvs().erase("plyA"); nvs().erase("plyB"); nvs().erase("coin2");
    Pet q; q.begin();
    ck(q.coins == 50, "and a save from before coin2 reads coin");
  }

  // ---- the cap, on every way coins arrive
  pet.coins = COIN_CAP - 3; pet.addCoins(10);
  ck(pet.coins == COIN_CAP, "winnings stop at 10,000,000");
  pet.addCoins(0xFFFFFFFFu);
  ck(pet.coins == COIN_CAP, "and a huge payout cannot wrap past it");
  pet.wallet = 999999; pet.coins = COIN_CAP - 10;
  ck(pet.buyCoins(500, COIN_DOLLARS) && pet.coins == COIN_CAP && pet.wallet == 999999 - 10 * COIN_DOLLARS,
     "buying coins near the cap fills the case and charges only for what fits");
  ck(!pet.buyCoins(50, COIN_DOLLARS) && pet.wallet == 999999 - 10 * COIN_DOLLARS, "a full case buys nothing");
  pet.coins = 70000;
  ck(pet.spendCoins(9999) && pet.coins == 60001, "spending works above 65,535");

  // ---- High/Low's pot doubles past 65,535 without wrapping, and stops at the cap
  {
    HighLow hl; bool passed = false;
    for (int tries = 0; tries < 2000 && !passed; tries++) {
      hl.start(9000);
      while (hl.state == HL_GUESS && hl.pot <= 65535) hl.guess(cardRank(hl.card) < 6);
      if (hl.state == HL_GUESS) passed = hl.pot > 65535;
    }
    ck(passed && hl.pot == 9000u * 8, "a pot of 9,000 doubles to 72,000 (not 6,464)");
    bool capped = false;
    for (int tries = 0; tries < 2000 && !capped; tries++) {
      hl.start(1); hl.pot = 6000000;
      if (hl.guess(cardRank(hl.card) < 6)) capped = hl.pot == COIN_CAP;
    }
    ck(capped && hl.cashOut() == COIN_CAP, "and a double past 10,000,000 pays exactly the cap");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
