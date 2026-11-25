// The battle rules the second batch of moves (v3.37) needs, each driven through
// the real engine: battleAct / battleTurnStart / battleOnEntry / battleEndTurn /
// battleEndRound, never a re-typed formula. Where a rule is "X never happens"
// there is a companion proving the mechanism engaged at all.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "held_items.h"
#include "dex.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=41; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static uint16_t mv(const char *name) {
  for (uint16_t i = 1; i < MOVE_COUNT; i++) if (!strcmp(MOVE_TBL[i].name, name)) return i;
  printf("      no move %s\n", name); bad++; return 0;
}
static void mk(Combatant &c, int16_t dex, uint8_t lvl, uint8_t side) {
  Pet p; p.dbgHatchAs(dex, false);
  p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 20;
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.relearnFromLevel();
  combatantFromPet(c, p);
  c.side = side;
}
static bool noted(const TurnLog &l, uint8_t bn) { for (uint8_t i = 0; i < l.noteN; i++) if (l.note[i] == bn) return true; return false; }
// one full turn the way btlResolve plays it: start, both act in order, end
static void turn(Combatant &a, uint16_t ma, Combatant &b, uint16_t mb, TurnLog *la = nullptr, TurnLog *lb = nullptr) {
  battleTurnStart(a, ma, b, mb);
  bool aFirst = battleMovesFirst(a, ma, b, mb);
  Combatant &x = aFirst ? a : b, &y = aFirst ? b : a;
  uint16_t mx = aFirst ? ma : mb, my = aFirst ? mb : ma;
  TurnLog l1, l2;
  battleAct(x, y, mx, l1);
  if (!y.fainted()) battleAct(y, x, my, l2);
  if (la) *la = aFirst ? l1 : l2;
  if (lb) *lb = aFirst ? l2 : l1;
  TurnLog e;
  if (!a.fainted() && !b.fainted()) { battleEndTurn(a, &b, e); battleEndTurn(b, &a, e); }
  battleEndRound(e);
}

int main() {
  Combatant A, B;

  // ---- PROTECT, and its falling odds
  {
    battleFieldReset(); mk(A, 25, 50, 0); mk(B, 6, 50, 1);
    uint16_t hp0 = A.hp; TurnLog la, lb;
    turn(A, mv("PROTECT"), B, mv("FLAMETHROWER"), &la, &lb);
    ck(A.hp == hp0 && noted(lb, BN_PROTECTED), "PROTECT: the attack that turn does nothing");
    int ok = 0, n = 400;
    for (int i = 0; i < n; i++) {
      battleFieldReset(); mk(A, 25, 50, 0); mk(B, 6, 50, 1);
      turn(A, mv("PROTECT"), B, mv("GROWL"));
      uint16_t h = A.hp; turn(A, mv("PROTECT"), B, mv("FLAMETHROWER"));
      if (A.hp == h) ok++;
    }
    printf("      a second PROTECT in a row held %d/%d\n", ok, n);
    ck(ok > n / 6 && ok < n / 2, "a second PROTECT in a row works about a third of the time");
  }

  // ---- flinching: FAKE OUT, first turn out only, and only from the faster side
  {
    battleFieldReset(); mk(A, 25, 50, 0); mk(B, 143, 50, 1);   // PIKACHU outspeeds SNORLAX
    TurnLog la, lb;
    turn(A, mv("FAKE OUT"), B, mv("BODY SLAM"), &la, &lb);
    ck(noted(lb, BN_FLINCHED) && A.hp == A.maxHp, "FAKE OUT flinches the slower foe: it does not act");
    uint16_t h = B.hp;
    turn(A, mv("FAKE OUT"), B, mv("GROWL"), &la, &lb);
    ck(B.hp == h && noted(la, BN_FAILED), "and fails after the first turn");
  }

  // ---- weather: power, chip, and the five turns
  {
    battleFieldReset(); mk(A, 9, 50, 0); mk(B, 143, 50, 1);
    uint16_t dry = battleDamage(A, B, mv("SURF"), false, 236);
    TurnLog l; battleAct(A, B, mv("RAIN DANCE"), l);
    uint16_t wet = battleDamage(A, B, mv("SURF"), false, 236);
    ck(gField.weather == W_RAIN && wet > dry * 14 / 10, "RAIN DANCE: rain, and Water moves hit half again as hard");
    for (int i = 0; i < 5; i++) battleEndRound(l);
    ck(gField.weather == W_NONE, "and it clears after five turns");
    battleAct(A, B, mv("SANDSTORM"), l);
    uint16_t h = B.hp; battleEndTurn(B, &A, l);
    ck(B.hp == h - B.maxHp / 16 && noted(l, BN_SAND_HURT), "a sandstorm chips a non-Rock 1/16 a turn");
    Combatant R; mk(R, 74, 50, 1); h = R.hp; battleEndTurn(R, &A, l);
    ck(R.hp == h, "and leaves a Rock type alone");
  }

  // ---- hazards on entry, and RAPID SPIN
  {
    battleFieldReset(); mk(A, 95, 50, 0); mk(B, 6, 50, 1);
    TurnLog l; battleAct(A, B, mv("STEALTH ROCK"), l);
    ck(gField.side[1].rocks && noted(l, BN_ROCKS), "STEALTH ROCK lands on the foe's side");
    Combatant C; mk(C, 6, 50, 1); battleOnEntry(C, 1, l);
    ck(C.hp == C.maxHp - C.maxHp / 2, "a CHARIZARD (x4 to Rock) loses half its HP coming in");
    Combatant D; mk(D, 74, 50, 1); battleOnEntry(D, 1, l);
    ck(D.hp == D.maxHp - (uint16_t)((uint32_t)D.maxHp * 50 / 800), "a GEODUDE (x1/2) loses a sixteenth");
    battleAct(A, B, mv("SPIKES"), l); battleAct(A, B, mv("SPIKES"), l);
    Combatant E; mk(E, 143, 50, 1); battleOnEntry(E, 1, l);
    ck(E.hp == E.maxHp - E.maxHp / 6 - (uint16_t)((uint32_t)E.maxHp * 100 / 800), "two layers of SPIKES take a sixth on top");
    Combatant F; mk(F, 6, 50, 1); TurnLog l2; battleOnEntry(F, 1, l2);
    ck(!noted(l2, BN_SPIKES_HURT), "and spare a Flying type");
    Combatant S; mk(S, 91, 50, 1);
    battleAct(S, A, mv("RAPID SPIN"), l);
    ck(!gField.side[1].rocks && !gField.side[1].spikes, "RAPID SPIN clears its own side");
  }

  // ---- screens, and a critical hit going straight through
  {
    battleFieldReset(); mk(A, 68, 50, 0); mk(B, 143, 50, 1);
    uint16_t open = battleDamage(A, B, mv("KARATE CHOP"), false, 236);
    TurnLog l; battleAct(B, A, mv("REFLECT"), l);
    uint16_t walled = battleDamage(A, B, mv("KARATE CHOP"), false, 236);
    uint16_t critd = battleDamage(A, B, mv("KARATE CHOP"), true, 236);
    printf("      KARATE CHOP %u, behind REFLECT %u\n", open, walled);
    ck(walled * 2 >= open - 4 && walled * 2 <= open + 4, "REFLECT halves a physical hit");
    ck(critd > open, "a critical hit ignores it");
    battleAct(A, B, mv("BRICK BREAK"), l);
    ck(!gField.side[1].reflect && noted(l, BN_SCREEN_BROKE), "BRICK BREAK shatters it first");
  }

  // ---- SUBSTITUTE
  {
    battleFieldReset(); mk(A, 143, 50, 0); mk(B, 25, 50, 1);
    TurnLog l; battleAct(A, B, mv("SUBSTITUTE"), l);
    ck(A.subHp == A.maxHp / 4 && A.hp == A.maxHp - A.maxHp / 4, "SUBSTITUTE costs a quarter and makes a decoy of it");
    uint16_t h = A.hp; battleAct(B, A, mv("THUNDERBOLT"), l);
    ck(A.hp == h, "a hit lands on the decoy, not the creature");
    A.subHp = 200; battleAct(B, A, mv("THUNDER WAVE"), l);
    ck(A.ailment == AIL_NONE, "and a status move cannot get past it");
  }

  // ---- accuracy and evasion stages
  {
    int hit = 0, n = 600;
    for (int i = 0; i < n; i++) {
      battleFieldReset(); mk(A, 25, 50, 0); mk(B, 143, 50, 1);
      B.evaStage = 6; TurnLog l; battleAct(A, B, mv("THUNDERBOLT"), l);
      if (!l.missed) hit++;
    }
    printf("      +6 evasion: %d/%d hit\n", hit, n);
    ck(hit > n / 4 && hit < n / 2, "evasion +6 drops a sure hit to about a third");
    battleFieldReset(); mk(A, 25, 50, 0); mk(B, 143, 50, 1);
    TurnLog l; battleAct(A, B, mv("DOUBLE TEAM"), l);
    ck(A.evaStage == 1, "DOUBLE TEAM raises evasion");
  }

  // ---- TOXIC escalates; LEECH SEED drains into the other side
  {
    battleFieldReset(); mk(A, 89, 50, 0); mk(B, 143, 50, 1);
    TurnLog l; do { battleAct(A, B, mv("TOXIC"), l); } while (B.ailment != AIL_POISON);
    uint16_t m = B.maxHp, h0 = B.hp;
    battleEndTurn(B, &A, l); uint16_t h1 = B.hp; battleEndTurn(B, &A, l); uint16_t h2 = B.hp;
    ck(h0 - h1 == m / 16 && h1 - h2 == m * 2 / 16, "TOXIC: one sixteenth, then two");
    battleFieldReset(); mk(A, 1, 50, 0); mk(B, 143, 50, 1); A.hp = A.maxHp / 2;
    do { battleAct(A, B, mv("LEECH SEED"), l); } while (!B.seeded);
    uint16_t a0 = A.hp, b0 = B.hp; battleEndTurn(B, &A, l);
    ck(b0 - B.hp == B.maxHp / 8 && A.hp - a0 == B.maxHp / 8, "LEECH SEED moves an eighth from one to the other");
    Combatant G; mk(G, 1, 50, 1); battleAct(A, G, mv("LEECH SEED"), l);
    ck(!G.seeded, "and cannot seed a Grass type");
  }

  // ---- TRICK ROOM and TAILWIND reorder the turn
  {
    battleFieldReset(); mk(A, 25, 50, 0); mk(B, 143, 50, 1);
    ck(battleMovesFirst(A, mv("THUNDERBOLT"), B, mv("BODY SLAM")), "PIKACHU normally outspeeds SNORLAX");
    TurnLog l; battleAct(B, A, mv("TRICK ROOM"), l);
    ck(!battleMovesFirst(A, mv("THUNDERBOLT"), B, mv("BODY SLAM")), "under TRICK ROOM the slower goes first");
    battleFieldReset(); mk(A, 25, 50, 0); mk(B, 143, 50, 1);
    battleAct(B, A, mv("TAILWIND"), l);
    ck(gField.side[1].tailwind, "TAILWIND blows for the user's side");
  }

  // ---- power decided at use time
  {
    battleFieldReset(); mk(A, 68, 50, 0); Combatant snor, pika; mk(snor, 143, 50, 1); mk(pika, 25, 50, 1);
    uint16_t hv = battleDamage(A, snor, mv("LOW KICK"), false, 236), lt = battleDamage(A, pika, mv("LOW KICK"), false, 236);
    printf("      LOW KICK: snorlax %u, pikachu %u\n", hv, lt);
    ck(hv > lt * 3, "LOW KICK hits a 460 kg SNORLAX far harder than a 6 kg PIKACHU");
    Combatant f; mk(f, 143, 50, 0); uint16_t plain = battleDamage(f, pika, mv("FACADE"), false, 236);
    f.ailment = AIL_PARA; uint16_t angry = battleDamage(f, pika, mv("FACADE"), false, 236);
    ck(angry > plain * 18 / 10, "FACADE doubles while the user has a status");
    Combatant e; mk(e, 6, 50, 0); uint16_t full = battleDamage(e, snor, mv("ERUPTION"), false, 236);
    e.hp = e.maxHp / 4; uint16_t low = battleDamage(e, snor, mv("ERUPTION"), false, 236);
    ck(low < full / 2, "ERUPTION weakens with the user's HP");
    Combatant r; mk(r, 143, 50, 0); r.bond = 100; uint16_t lov = battleDamage(r, pika, mv("RETURN"), false, 236);
    r.bond = 10; uint16_t cold = battleDamage(r, pika, mv("RETURN"), false, 236);
    ck(lov > cold * 4, "RETURN grows with the bond");
  }

  // ---- fixed damage and the one-hit KO
  {
    battleFieldReset(); mk(A, 19, 30, 0); mk(B, 143, 60, 1);
    TurnLog l; battleAct(A, B, mv("SUPER FANG"), l);
    ck(B.hp == B.maxHp - B.maxHp / 2, "SUPER FANG halves the target's HP");
    battleFieldReset(); mk(A, 87, 30, 0); mk(B, 143, 60, 1);
    battleAct(A, B, mv("SHEER COLD"), l);
    ck(noted(l, BN_FAILED) && B.hp == B.maxHp, "a one-hit KO fails on a higher level");
  }

  // ---- COUNTER, ENDURE, EXPLOSION, DESTINY BOND
  {
    battleFieldReset(); mk(A, 68, 50, 0); mk(B, 143, 50, 1);
    TurnLog la, lb; uint16_t h = B.hp;
    turn(B, mv("BODY SLAM"), A, mv("COUNTER"), &lb, &la);   // COUNTER is -5: SNORLAX hits first
    uint16_t took = A.maxHp - A.hp;
    ck(took && (h - B.hp) == took * 2, "COUNTER returns twice the physical damage taken");
    battleFieldReset(); mk(A, 25, 20, 0); mk(B, 143, 80, 1);
    turn(A, mv("ENDURE"), B, mv("BODY SLAM"));
    ck(A.hp == 1, "ENDURE hangs on at 1 HP");
    battleFieldReset(); mk(A, 76, 50, 0); mk(B, 143, 50, 1);
    battleAct(A, B, mv("EXPLOSION"), la);
    ck(A.fainted() && la.userFainted, "EXPLOSION costs the user");
    battleFieldReset(); mk(A, 94, 50, 0); mk(B, 143, 100, 1); A.hp = 1;
    battleAct(A, B, mv("DESTINY BOND"), la); battleAct(B, A, mv("CRUNCH"), lb);   // not BODY SLAM: GENGAR is a Ghost
    ck(A.fainted() && B.fainted(), "DESTINY BOND takes the one that KOs it");
  }

  // ---- two-turn moves out of reach, and the moves that reach them
  {
    battleFieldReset(); mk(A, 6, 50, 0); mk(B, 25, 50, 1);
    TurnLog l; battleAct(A, B, mv("FLY"), l);
    ck(l.charged && A.charging, "FLY spends a turn in the air");
    uint16_t h = A.hp; battleAct(B, A, mv("QUICK ATTACK"), l);
    ck(l.missed && A.hp == h, "and nothing ordinary reaches it there");
    int reached = 0;
    for (int i = 0; i < 40; i++) { A.charging = mv("FLY"); TurnLog t; battleAct(B, A, mv("THUNDER"), t); if (!t.missed) reached++; A.hp = A.maxHp; }
    ck(reached > 10, "THUNDER does (it hits a target in the air)");
  }

  // ---- HIDDEN POWER from the IVs; ENCORE, TAUNT, the traps
  {
    Pet p; p.dbgHatchAs(25, false); p.ivHp = 31; p.ivAtk = 31; p.ivDef = 31; p.ivSpe = 31;
    Combatant c; combatantFromPet(c, p);
    ck(c.hpType == T_DARK && battleMoveType(c, mv("HIDDEN POWER")) == T_DARK, "HIDDEN POWER: all-odd IVs make it Dark");
    p.ivHp = p.ivAtk = p.ivDef = p.ivSpe = 30; combatantFromPet(c, p);
    ck(c.hpType == T_FIGHTING, "and all-even make it Fighting");

    battleFieldReset(); mk(A, 35, 50, 0); mk(B, 143, 50, 1);
    TurnLog l; uint16_t g = mv("GROWL");
    battleAct(B, A, g, l);
    battleAct(A, B, mv("ENCORE"), l);
    uint16_t pick = mv("BODY SLAM"), other = mv("GROWL");
    battleTurnStart(A, other, B, pick);
    ck(pick == g, "ENCORE: whatever was picked, the encored move is used");
    battleFieldReset(); mk(A, 52, 50, 0); mk(B, 143, 50, 1);
    battleAct(A, B, mv("TAUNT"), l); battleAct(B, A, mv("REST"), l);
    ck(noted(l, BN_TAUNT_BLOCK), "TAUNT stops a status move");
    battleFieldReset(); mk(A, 94, 50, 0); mk(B, 143, 50, 1);
    ck(battleCanSwitch(B), "free to switch before");
    battleAct(A, B, mv("MEAN LOOK"), l);
    ck(!battleCanSwitch(B), "MEAN LOOK: no switching out");
    battleOnSwitchOut(B);
    ck(battleCanSwitch(B) && B.stage[0] == 0, "and leaving clears it, with the stages");
  }

  // ---- KNOCK OFF takes the item for THIS battle only
  {
    battleFieldReset();
    Pet p; p.dbgHatchAs(143, false); p.ageMinutes = 49UL * MINUTES_PER_LEVEL; p.relearnFromLevel();
    p.held = IT_LEFTOVERS;
    Combatant tgt; combatantFromPet(tgt, p); tgt.side = 1; tgt.holder = HOLDER_PET;
    mk(A, 52, 50, 0);
    TurnLog l; battleAct(A, tgt, mv("KNOCK OFF"), l);
    ck(tgt.held == ITEM_NONE && noted(l, BN_KNOCKED), "KNOCK OFF: the item is gone for the fight");
    ck(p.held == IT_LEFTOVERS, "but the creature still holds it afterwards");
  }

  // ---- REST, SLEEP TALK
  {
    battleFieldReset(); mk(A, 143, 50, 0); mk(B, 25, 50, 1); A.hp = 10;
    TurnLog l; battleAct(A, B, mv("REST"), l);
    ck(A.hp == A.maxHp && A.ailment == AIL_SLEEP, "REST: full HP, asleep");
    A.moves[0] = mv("SLEEP TALK"); A.moves[1] = mv("BODY SLAM"); A.moves[2] = A.moves[3] = 0;
    uint16_t h = B.hp; battleAct(A, B, mv("SLEEP TALK"), l);
    ck(B.hp < h && l.move == mv("BODY SLAM"), "SLEEP TALK uses another of its moves while asleep");
  }

  // ---- every move in the table can be played without breaking anything
  {
    bool sane = true; int used = 0;
    for (uint16_t m = 1; m < MOVE_COUNT; m++) {
      for (int rep = 0; rep < 6; rep++) {
        battleFieldReset(); mk(A, 143, 50, 0); mk(B, 6, 50, 1);
        A.moves[0] = m; A.moves[1] = mv("TACKLE"); B.moves[0] = mv("TACKLE");
        for (int t = 0; t < 4 && !A.fainted() && !B.fainted(); t++) {
          uint16_t ma = m, mb = mv("TACKLE");
          turn(A, ma, B, mb);
          if (A.hp > A.maxHp || B.hp > B.maxHp) sane = false;
          for (int s = 0; s < SI_COUNT; s++) if (A.stage[s] > 6 || A.stage[s] < -6 || B.stage[s] > 6 || B.stage[s] < -6) sane = false;
        }
        used++;
      }
    }
    printf("      %d short fights, every move\n", used);
    ck(sane, "all %d moves played: no HP over max, no stage past 6");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
