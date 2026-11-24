// Personalities through the real Pet: the roll, each trait's favourite care, the
// awake nudges, that NO trait makes a night harder, and storage -- the checkpoint
// byte, banking and coming back, and a real v3.28 save gaining a trait once.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "persona.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
uint32_t g_seed=23; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char *s){printf("%s  %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}

static void fresh(Pet &p, uint8_t trait) {
  p.dbgHatchAs(4, false);
  p.persona = trait;
  p.fullness = p.joy = p.energy = 50; p.hygiene = 50; p.poops = 0; p.bond = 0;
  p.sleeping = false;
}

static bool loadNvs(const std::string &path) {
  FILE *f = fopen(path.c_str(), "rb"); if (!f) return false;
  nvs().clear(); uint32_t n = 0; bool ok = fread(&n, 4, 1, f) == 1;
  for (uint32_t i = 0; ok && i < n; i++) {
    uint32_t kl = 0, vl = 0; ok = fread(&kl, 4, 1, f) == 1 && kl <= 64;
    std::string k(kl, 0); ok = ok && fread(&k[0], 1, kl, f) == kl && fread(&vl, 4, 1, f) == 1;
    std::vector<uint8_t> v(vl); ok = ok && (!vl || fread(v.data(), 1, vl, f) == vl);
    if (ok) nvs()[k] = v;
  }
  fclose(f); return ok && n > 0;
}

int main() {
  Pet p; p.begin();

  // ---- the roll
  {
    bool all[PT_COUNT] = {};
    for (int i = 0; i < 200; i++) { p.dbgHatchAs(4, false); if (personaValid(p.persona)) all[p.persona] = true; }
    bool every = true; for (int t = 1; t < PT_COUNT; t++) every = every && all[t];
    ck(every, "hatching rolls every one of the seven traits, and only valid ones");
    Pet scratch; scratch.dbgHatchAs(4, false);
    ck(scratch.persona == PT_NONE, "a scratch Pet (a battle foe) never spends a roll");
  }

  // ---- favourite care: each trait's own, against a creature with no trait
  {
    Pet q; q.begin();
    fresh(q, PT_NONE); q.caress(); uint8_t plain = q.joy;
    fresh(q, PT_CUDDLY); q.caress();
    ck(q.joy == plain + 5 && q.bond == 2, "CUDDLY: a caress is worth +5 joy and +1 bond more");
    fresh(q, PT_NONE); q.feedBerry(q.lovesBerry(0) ? 1 : 0); uint8_t full0 = q.fullness;
    fresh(q, PT_PICKY); q.feedBerry(q.lovesBerry(0) ? 1 : 0);
    ck(q.fullness == full0 - 5, "PICKY: a berry it does not love fills 5 less");
    fresh(q, PT_NONE); q.clean(); uint8_t j0 = q.joy;
    fresh(q, PT_TIDY); q.clean();
    ck(q.joy == j0 + 10, "TIDY: a bath is worth +10 joy");
    fresh(q, PT_NONE); q.toggleLight(); uint8_t j1 = q.joy;
    fresh(q, PT_SLEEPY); q.toggleLight();
    ck(q.joy == j1 + 8, "SLEEPY: being put to bed is worth +8 joy");
    fresh(q, PT_GLUTTON); q.toggleLight();
    ck(q.joy == j1, "and nobody else cares about bedtime");
  }

  // ---- awake nudges: a little, never a lot
  {
    Pet q; q.begin();
    fresh(q, PT_NONE); q.fullness = 100; for (int i = 0; i < 20; i++) q.dbgTick(); uint8_t f0 = q.fullness;
    fresh(q, PT_GLUTTON); q.fullness = 100; for (int i = 0; i < 20; i++) q.dbgTick();
    ck(q.fullness == f0 - 5, "GLUTTON: 20 awake minutes cost 5 more food (1/4 a minute)");
  }

  // ---- THE NIGHT IS NEVER HARDER: ten hours asleep, every trait vs none
  {
    Pet q; q.begin();
    auto night = [&](uint8_t t, uint8_t out[4]) {
      fresh(q, t); q.fullness = q.joy = q.hygiene = 60; q.energy = 20; q.sleeping = true;
      for (int i = 0; i < 600; i++) q.dbgTick();
      out[0] = q.fullness; out[1] = q.joy; out[2] = q.hygiene; out[3] = q.energy;
    };
    uint8_t base[4]; night(PT_NONE, base);
    bool same = true, sleepyOk = true;
    for (uint8_t t = 1; t < PT_COUNT; t++) {
      uint8_t r[4]; night(t, r);
      if (r[0] != base[0] || r[1] != base[1] || r[2] != base[2]) same = false;
      if (r[3] < base[3]) sleepyOk = false;
    }
    ck(same, "asleep, no trait changes food, joy or hygiene at all");
    (void)sleepyOk;   // saturates at 100 over ten hours -- measured below instead
    // Energy over a SHORT sleep, before anyone reaches the cap: no trait may
    // refill slower than none, and SLEEPY must refill strictly faster (the
    // positive companion -- without it "never slower" passes vacuously).
    auto nap = [&](uint8_t t) {
      fresh(q, t); q.energy = 10; q.sleeping = true;
      for (int i = 0; i < 8; i++) q.dbgTick();
      return q.energy;
    };
    uint8_t e0 = nap(PT_NONE);
    bool slower = false;
    for (uint8_t t = 1; t < PT_COUNT; t++) if (nap(t) < e0) slower = true;
    ck(!slower, "asleep, no trait refills energy slower than none");
    ck(nap(PT_SLEEPY) > e0, "and SLEEPY really does refill faster");
  }

  // ---- storage: the checkpoint byte, banking and back
  {
    Pet q; q.begin(); q.dbgHatchAs(7, false); q.persona = PT_TIDY; q.saveNow();
    Pet r; r.begin();
    ck(r.persona == PT_TIDY, "the trait survives a reload through the checkpoint");
    PartyMon m = r.toPartyMon();
    ck(m.persona == PT_TIDY, "banking keeps it");
    Pet s; s.begin(); s.switchTo(m);
    ck(s.persona == PT_TIDY, "and raising it again brings it back");
  }

  // ---- a REAL v3.28 save (its checkpoint byte was always 0) gains one, once
  {
    std::string f = __FILE__;
    f = f.substr(0, f.rfind("/tools/emu/tests/")) + "/tools/fixtures/v328.nvs";
    ck(loadNvs(f), "the v3.28 fixture loads");
    Pet q; q.begin();
    ck(q.speciesId == 6 && !strcmp(q.nick, "BLAZE"), "its live creature is the v3.28 CHARIZARD 'BLAZE'");
    uint8_t rolled = q.persona;
    ck(personaValid(rolled), "which gains a trait on its first load");
    q.saveNow();
    Pet s; s.begin();
    ck(s.persona == rolled, "and keeps that same one afterwards");
    party.begin();
    bool clean = true;
    for (int i = 0; i < PARTY_SLOTS; i++) if (party.slots[i].persona != PT_NONE) clean = false;
    for (int i = 0; i < BOX_SLOTS; i++) if (party.box[i].persona != PT_NONE) clean = false;
    ck(clean, "v3.28's banked creatures read as NO trait after migration, not stray bytes");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
