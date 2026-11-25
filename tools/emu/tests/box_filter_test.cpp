// Box filters and sorting, against the real box_filter.cpp and a real Party:
// which creatures each filter keeps (a dual type matching either type), every
// sort's order with ties kept stable, the PLAIN view still listing empty slots
// (that is where a creature goes), and the favourite mark surviving a save and
// being cleared on a creature that arrives by trade.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "dex.h"
#include "link.h"
#include "box_filter.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=19; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static PartyMon mon(int16_t dex, uint8_t lvl, uint8_t iv) {
  Pet p; p.dbgHatchAs(dex, false); p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = iv; p.relearnFromLevel();
  return p.toPartyMon();
}
static bool seq(const uint16_t *v, uint16_t n, const Party &q, const int16_t *dex, uint8_t m) {
  if (n != m) return false;
  for (uint16_t i = 0; i < n; i++) if (q.box[v[i]].dex != dex[i]) return false;
  return true;
}

int main(){
  Party q; q.begin();
  for (int i = 0; i < BOX_SLOTS; i++) q.box[i] = PartyMon();
  //            slot: 0 CHARIZARD   2 BLASTOISE*  3 PIKACHU(held) 5 CHARMANDER(fav) 7 GYARADOS  9 PIDGEY
  q.box[0] = mon(6, 50, 20);
  q.box[2] = mon(9, 40, 25); q.box[2].shiny = 1;
  q.box[3] = mon(25, 30, 31); q.box[3].held = 1;
  q.box[5] = mon(4, 10, 10); q.box[5].fav = PM_FAV;
  q.box[7] = mon(130, 60, 5);
  q.box[9] = mon(16, 30, 12);
  static uint16_t v[BOX_SLOTS]; uint16_t n;

  n = boxView(q, BF_ALL, BS_SLOT, v);
  ck(n == BOX_SLOTS && v[0] == 0 && v[17] == 17, "the plain view is every slot, empties included, in order");

  { const int16_t d[] = { 6, 4 }; n = boxView(q, BF_TYPE0 + T_FIRE, BS_SLOT, v); ck(seq(v, n, q, d, 2), "FIRE keeps CHARIZARD and CHARMANDER, in slot order"); }
  { const int16_t d[] = { 6, 130, 16 }; n = boxView(q, BF_TYPE0 + T_FLYING, BS_SLOT, v); ck(seq(v, n, q, d, 3), "FLYING matches a SECOND type too (CHARIZARD, GYARADOS, PIDGEY)"); }
  { const int16_t d[] = { 4 }; n = boxView(q, BF_FAV, BS_SLOT, v); ck(seq(v, n, q, d, 1), "FAVOURITES keeps only the starred one"); }
  { const int16_t d[] = { 9 }; n = boxView(q, BF_SHINY, BS_SLOT, v); ck(seq(v, n, q, d, 1), "SHINY keeps only the shiny"); }
  { const int16_t d[] = { 25 }; n = boxView(q, BF_HELD, BS_SLOT, v); ck(seq(v, n, q, d, 1), "HOLDING keeps only the one with an item"); }
  n = boxView(q, BF_TYPE0 + T_GHOST, BS_SLOT, v);
  ck(n == 0, "a type nobody has finds nothing (and no empties)");

  { const int16_t d[] = { 130, 6, 9, 25, 16, 4 }; n = boxView(q, BF_ALL, BS_LEVEL, v);
    ck(seq(v, n, q, d, 6), "LEVEL: highest first, empties left out, a tie (PIKACHU, PIDGEY at 30) in slot order"); }
  { const int16_t d[] = { 25, 9, 6, 16, 4, 130 }; n = boxView(q, BF_ALL, BS_IV, v); ck(seq(v, n, q, d, 6), "IV TOTAL: best first"); }
  { const int16_t d[] = { 4, 6, 9, 16, 25, 130 }; n = boxView(q, BF_ALL, BS_DEX, v); ck(seq(v, n, q, d, 6), "DEX NO.: ascending"); }
  { const int16_t d[] = { 9, 6, 4, 130, 16, 25 }; n = boxView(q, BF_ALL, BS_NAME, v); ck(seq(v, n, q, d, 6), "NAME: A to Z"); }
  strcpy(q.box[7].nick, "AARDVARK");
  { n = boxView(q, BF_ALL, BS_NAME, v); ck(q.box[v[0]].dex == 130, "a nickname is what NAME sorts by"); }
  const uint8_t sorts[] = { BS_ATK, BS_DEF, BS_SPE, BS_HP };
  for (uint8_t s : sorts) {
    n = boxView(q, BF_ALL, s, v);
    bool mono = n == 6;
    for (uint16_t i = 1; i < n; i++) {
      uint16_t a, b;
      switch (s) {
        case BS_ATK: a = q.atkOf(q.box[v[i-1]]); b = q.atkOf(q.box[v[i]]); break;
        case BS_DEF: a = q.defOf(q.box[v[i-1]]); b = q.defOf(q.box[v[i]]); break;
        case BS_SPE: a = q.speOf(q.box[v[i-1]]); b = q.speOf(q.box[v[i]]); break;
        default:     a = q.vitOf(q.box[v[i-1]]); b = q.vitOf(q.box[v[i]]); break;
      }
      if (a < b) mono = false;
    }
    char m[64]; snprintf(m, sizeof m, "sort %u: the real battle stat, highest first", s);
    ck(mono, m);
  }
  { const int16_t d[] = { 130, 6 }; n = boxView(q, BF_TYPE0 + T_FLYING, BS_LEVEL, v); ck(n == 3 && q.box[v[0]].dex == 130 && q.box[v[1]].dex == 6, "a filter and a sort together"); (void)d; }

  // ---- the favourite mark
  q.boxSave();
  { Party r; r.begin(); ck(r.box[5].isFav() && !r.box[0].isFav(), "a favourite survives a save and a reload"); }
  { PartyMon t = q.box[5]; ck(tradeSanitize(t) && !t.isFav(), "a creature that arrives by trade is not yet one of OUR favourites"); }
  { PartyMon g = q.box[0]; g.fav = 1; ck(!g.isFav(), "a stray byte from old padding does not read as a favourite"); }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
