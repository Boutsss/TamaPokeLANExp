#include "battle.h"
#include "dex.h"
#include "types.h"
#include "weights.h"
#include "game_events.h"

static bool inflictAilment(Combatant &def, uint8_t ail, struct TurnLog &log);   // below: THE ailment rule

BattleField gField;
void battleFieldReset() { gField = BattleField(); }

static bool isType(const Combatant &c, uint8_t t) { return hasStab(c.dex, t); }
// "On the ground", for terrain and spikes. No abilities or items lift anyone,
// so this is just: not a Flying type.
static bool grounded(const Combatant &c) { return !isType(c, T_FLYING); }
static BattleSide &sideOf(const Combatant &c) { return gField.side[c.side & 1]; }

// HIDDEN POWER's type, the games' formula over the IV parities. This game has
// four IVs; SP.A and SP.D run off the ATK and DEF ones (see dex_moves.py), so
// the same two stand in for them here.
static uint8_t hiddenPowerType(uint8_t hp, uint8_t at, uint8_t df, uint8_t sp) {
  static const uint8_t T[16] = { T_FIGHTING, T_FLYING, T_POISON, T_GROUND, T_ROCK, T_BUG, T_GHOST, T_STEEL,
                                 T_FIRE, T_WATER, T_GRASS, T_ELECTRIC, T_PSYCHIC, T_ICE, T_DRAGON, T_DARK };
  unsigned v = (hp & 1) + 2 * (at & 1) + 4 * (df & 1) + 8 * (sp & 1) + 16 * (at & 1) + 32 * (df & 1);
  return T[v * 15 / 63];
}

// ---------- building a combatant ----------

static void fill(Combatant &c, int16_t dex, uint8_t lvl, uint16_t hp,
                 uint16_t a, uint16_t d, uint16_t sa, uint16_t sd, uint16_t sp) {
  c = Combatant();
  c.dex = dex;
  c.level = lvl;
  c.maxHp = hp ? hp : 1;
  c.hp = c.maxHp;
  c.base[SI_ATK] = a; c.base[SI_DEF] = d;
  c.base[SI_SPA] = sa; c.base[SI_SPD] = sd; c.base[SI_SPE] = sp;
}

void combatantFromPet(Combatant &c, const Pet &p) {
  fill(c, p.speciesId, p.level(), p.vitStat(), p.atkStat(), p.defStat(),
       p.spaStat(), p.spdStat(), p.speStat());
  for (int i = 0; i < MOVE_SLOTS; i++) c.moves[i] = p.moves[i];
  c.shiny = p.shiny;
  c.skin = p.skinShown();
  c.held = itemIsHeld(p.held) ? p.held : ITEM_NONE;
  c.bond = p.bond;
  c.hpType = hiddenPowerType(p.ivHp, p.ivAtk, p.ivDef, p.ivSpe);
  const char *nm = p.nick[0] ? p.nick : DEX_TBL[p.speciesId].name;
  snprintf(c.name, sizeof(c.name), "%s", nm);
}

void combatantFromParty(Combatant &c, const PartyMon &m) {
  fill(c, m.dex, (uint8_t)m.level, party.vitOf(m), party.atkOf(m), party.defOf(m),
       party.spaOf(m), party.spdOf(m), party.speOf(m));
  for (int i = 0; i < MOVE_SLOTS; i++) c.moves[i] = m.move(i);
  c.shiny = m.shiny != 0;
  c.skin = m.skin();
  c.held = itemIsHeld(m.held) ? m.held : ITEM_NONE;
  c.bond = m.bond;
  c.hpType = hiddenPowerType(m.ivHp, m.ivAtk, m.ivDef, m.ivSpe);
  const char *nm = m.nick[0] ? m.nick : DEX_TBL[m.dex].name;
  snprintf(c.name, sizeof(c.name), "%s", nm);
}

// ---------- stat stages ----------

// The series' own table, as a fraction so it stays integer: +1 is 3/2, -1 is
// 2/3, and so on out to +6 = 4x and -6 = 1/4.
uint16_t stagedStat(uint16_t base, int8_t stage) {
  if (stage > 6) stage = 6;
  if (stage < -6) stage = -6;
  uint16_t num = 2 + (stage > 0 ? stage : 0);
  uint16_t den = 2 + (stage < 0 ? -stage : 0);
  uint32_t v = (uint32_t)base * num / den;
  return v < 1 ? 1 : (v > 65535 ? 65535 : (uint16_t)v);
}

static uint16_t effStat(const Combatant &c, uint8_t idx) {
  uint16_t v = stagedStat(c.base[idx], c.stage[idx]);
  // burn halves physical attack, paralysis halves speed -- the two ailments
  // that do something beyond chip damage
  if (idx == SI_ATK && c.ailment == AIL_BURN) v = v / 2 ? v / 2 : 1;
  if (idx == SI_SPE && c.ailment == AIL_PARA) v = v / 2 ? v / 2 : 1;
  if (idx == SI_SPE && sideOf(c).tailwind) v = v * 2 > 65535 ? 65535 : v * 2;
  // a sandstorm stiffens a Rock type's special defence (Gen 4 on)
  if (idx == SI_SPD && gField.weather == W_SAND && isType(c, T_ROCK)) v = (uint16_t)((uint32_t)v * 3 / 2);
  return v;
}

// ---------- move type and power ----------

uint8_t battleMoveType(const Combatant &atk, uint16_t mv) {
  if (!mv || mv >= MOVE_COUNT) return T_NORMAL;
  const MoveEntry &m = MOVE_TBL[mv];
  if (m.effect == EF_HIDDENPOWER) return atk.hpType;
  if (m.effect == EF_VARPOW && m.param == VP_WEATHERBALL) {
    switch (gField.weather) {
      case W_RAIN: return T_WATER; case W_SUN: return T_FIRE;
      case W_SAND: return T_ROCK;  case W_HAIL: return T_ICE;
      default: return T_NORMAL;
    }
  }
  return m.type;
}

static int stagesUp(const Combatant &c) {
  int n = 0;
  for (int i = 0; i < SI_COUNT; i++) if (c.stage[i] > 0) n += c.stage[i];
  if (c.accStage > 0) n += c.accStage;
  if (c.evaStage > 0) n += c.evaStage;
  return n;
}

static uint8_t weightPower(int16_t dex) {   // LOW KICK, GRASS KNOT
  uint16_t w = (dex >= 1 && dex <= DEX_COUNT) ? DEX_WEIGHT[dex] : 0;
  return w < 100 ? 20 : w < 250 ? 40 : w < 500 ? 60 : w < 1000 ? 80 : w < 2000 ? 100 : 120;
}
static uint8_t heavyPower(int16_t user, int16_t target) {   // HEAVY SLAM, HEAT CRASH
  uint32_t a = (user >= 1 && user <= DEX_COUNT) ? DEX_WEIGHT[user] : 1;
  uint32_t b = (target >= 1 && target <= DEX_COUNT) ? DEX_WEIGHT[target] : 1;
  if (!b) b = 1;
  return a >= 5 * b ? 120 : a >= 4 * b ? 100 : a >= 3 * b ? 80 : a >= 2 * b ? 60 : 40;
}

// The power a damaging move hits with right now. EF_VARPOW is decided here,
// and so is everything that doubles a move under a condition.
static uint16_t movePower(const Combatant &atk, const Combatant &def, uint16_t mv) {
  const MoveEntry &m = MOVE_TBL[mv];
  uint32_t p = m.power;
  if (m.effect == EF_VARPOW) {
    switch (m.param) {
      case VP_FACADE: if (atk.ailment == AIL_BURN || atk.ailment == AIL_PARA || atk.ailment == AIL_POISON) p *= 2; break;
      case VP_RETURN: p = 102u * atk.bond / 100; break;
      case VP_FRUSTRATION: p = 102u * (100 - (atk.bond > 100 ? 100 : atk.bond)) / 100; break;
      case VP_PAYBACK: if (def.movedThisTurn) p *= 2; break;
      case VP_ASSURANCE: if (def.hurtThisTurn) p *= 2; break;
      case VP_REVENGE: if (atk.hurtThisTurn) p *= 2; break;
      case VP_HEX: if (def.ailment != AIL_NONE) p *= 2; break;
      case VP_VENOSHOCK: if (def.ailment == AIL_POISON) p *= 2; break;
      case VP_BRINE: if ((uint32_t)def.hp * 2 <= def.maxHp) p *= 2; break;
      case VP_ACROBATICS: if (atk.held == ITEM_NONE || atk.itemUsed) p *= 2; break;
      case VP_GYRO: { uint32_t a = effStat(atk, SI_SPE), d = effStat(def, SI_SPE); p = 25 * d / (a ? a : 1) + 1; if (p > 150) p = 150; break; }
      case VP_ELECTRO: { uint32_t a = effStat(atk, SI_SPE), d = effStat(def, SI_SPE); if (!d) d = 1;
        p = a >= 4 * d ? 150 : a >= 3 * d ? 120 : a >= 2 * d ? 80 : a >= d ? 60 : 40; break; }
      case VP_REVERSAL: { uint32_t r = 48u * atk.hp / (atk.maxHp ? atk.maxHp : 1);
        p = r <= 1 ? 200 : r <= 4 ? 150 : r <= 9 ? 100 : r <= 16 ? 80 : r <= 32 ? 40 : 20; break; }
      case VP_ERUPTION: p = 150u * atk.hp / (atk.maxHp ? atk.maxHp : 1); break;
      case VP_STORED: p = 20 + 20 * stagesUp(atk); break;
      case VP_PUNISH: p = 60 + 20 * stagesUp(def); if (p > 200) p = 200; break;
      case VP_WEATHERBALL: if (gField.weather != W_NONE) p *= 2; break;
      case VP_WRINGOUT: p = 120u * def.hp / (def.maxHp ? def.maxHp : 1); break;
      case VP_TANTRUM: if (atk.lastFailed) p *= 2; break;
      case VP_WEIGHT: p = weightPower(def.dex); break;
      case VP_HEAVY: p = heavyPower(atk.dex, def.dex); break;
      default: break;
    }
  }
  if (m.effect == EF_MAGNITUDE) p = 71;   // its average; battleAct rolls the real one
  if (m.effect == EF_ESCALATE && atk.lastMove == mv) {
    uint8_t n = atk.streak < m.param ? atk.streak : (uint8_t)m.param;
    p <<= n;
  }
  if (m.effect == EF_WAKEUP && def.ailment == (uint8_t)m.param) p *= 2;
  if (m.effect == EF_KNOCKOFF && def.held != ITEM_NONE && !def.itemUsed) p = p * 3 / 2;
  // SOLAR BEAM and SOLAR BLADE are halved in any weather but sun
  if (m.effect == EF_CHARGE && (m.param & 2) && gField.weather != W_NONE && gField.weather != W_SUN) p /= 2;
  return p < 1 ? 1 : (p > 255 * 4 ? 255 * 4 : (uint16_t)p);
}

// ---------- damage ----------

static uint16_t damageCore(const Combatant &atk, const Combatant &def, uint16_t mv,
                           bool crit, uint8_t roll, uint16_t powerOverride) {
  if (!mv || mv >= MOVE_COUNT) return 0;
  const MoveEntry &m = MOVE_TBL[mv];
  if (m.cat == MC_STATUS) return 0;
  uint8_t type = battleMoveType(atk, mv);
  uint32_t eff = typeEffVsDex(type, def.dex);
  // FREEZE-DRY: super effective on Water instead of resisted (x4 undoes the x1/2)
  if ((m.flags & MF_FREEZE_DRY) && isType(def, T_WATER)) eff *= 4;
  if (eff == 0) return 0;               // immune: no chip, no minimum

  // fixed damage, which ignores stats (but not immunity: NIGHT SHADE on a Normal type)
  switch (m.effect) {
    case EF_FIXED_LVL: return atk.level ? atk.level : 1;
    case EF_FIXED: return m.param > 0 ? (uint16_t)m.param : 1;
    case EF_HALFHP: return def.hp / 2 ? def.hp / 2 : 1;
    case EF_ENDEAVOR: return def.hp > atk.hp ? def.hp - atk.hp : 0;
    case EF_OHKO: return atk.level >= def.level ? def.hp : 0;
    case EF_FINALGAMBIT: return atk.hp;
    case EF_COUNTER: {
      if (!atk.hurtThisTurn || !atk.lastHitDmg) return 0;
      if (m.param == 1 && atk.lastHitCat != MC_PHYS) return 0;
      if (m.param == 2 && atk.lastHitCat != MC_SPEC) return 0;
      uint32_t d = m.param == 3 ? (uint32_t)atk.lastHitDmg * 3 / 2 : (uint32_t)atk.lastHitDmg * 2;
      return d > 65535 ? 65535 : (uint16_t)d;
    }
    default: break;
  }

  bool phys = m.cat == MC_PHYS;
  const Combatant &src = (m.flags & MF_FOUL) ? def : atk;     // FOUL PLAY: the target's own attack
  uint16_t A = phys ? effStat(src, SI_ATK) : effStat(atk, SI_SPA);
  uint8_t dIdx = (phys || (m.flags & MF_PHYS_DEF)) ? SI_DEF : SI_SPD;
  uint16_t D = effStat(def, dIdx);
  if (m.flags & MF_IGNORE_STAGES) D = def.base[dIdx];
  // A critical hit ignores the defender's positive stages and the attacker's
  // negative ones, so a Barrier cannot make you immune to a lucky roll.
  if (crit) {
    A = phys ? src.base[SI_ATK] : atk.base[SI_SPA];
    D = def.base[dIdx];
  }
  if (!D) D = 1;

  uint32_t power = powerOverride ? powerOverride : movePower(atk, def, mv);
  uint32_t dmg = (2UL * atk.level / 5 + 2) * power * A / D / 50 + 2;
  // weather
  if (gField.weather == W_RAIN) { if (type == T_WATER) dmg = dmg * 3 / 2; else if (type == T_FIRE) dmg /= 2; }
  if (gField.weather == W_SUN)  { if (type == T_FIRE) dmg = dmg * 3 / 2; else if (type == T_WATER) dmg /= 2; }
  // terrain (Gen 7's 1.5x), for a creature on the ground
  if (grounded(atk)) {
    if ((gField.terrain == TR_ELECTRIC && type == T_ELECTRIC) || (gField.terrain == TR_GRASSY && type == T_GRASS) ||
        (gField.terrain == TR_PSYCHIC && type == T_PSYCHIC)) dmg = dmg * 3 / 2;
  }
  if (gField.terrain == TR_MISTY && type == T_DRAGON && grounded(def)) dmg /= 2;
  if (atk.chargedUp && type == T_ELECTRIC) dmg *= 2;
  // screens, which a critical hit goes straight through
  if (!crit) {
    const BattleSide &ds = sideOf(def);
    if ((phys && ds.reflect) || (!phys && ds.lscreen) || ds.veil) dmg /= 2;
  }
  if (crit) dmg = dmg * 3 / 2;          // Gen 6 on: x1.5
  if (hasStab(atk.dex, type)) dmg = dmg * 3 / 2;
  dmg = dmg * eff / 100;
  dmg = dmg * roll / 255;
  return dmg < 1 ? 1 : (dmg > 65535 ? 65535 : (uint16_t)dmg);
}

// roll is 217..255, the series' damage spread, passed in so tests can pin it.
uint16_t battleDamage(const Combatant &atk, const Combatant &def, uint16_t mv,
                      bool crit, uint8_t roll) {
  return damageCore(atk, def, mv, crit, roll, 0);
}

// ---------- turn order ----------

bool battleMovesFirst(const Combatant &a, uint16_t ma,
                      const Combatant &b, uint16_t mb, uint8_t *claw) {
  if (claw) *claw = ITEM_NONE;
  int pa = (ma && ma < MOVE_COUNT) ? MOVE_TBL[ma].prio : 0;
  int pb = (mb && mb < MOVE_COUNT) ? MOVE_TBL[mb].prio : 0;
  if (pa != pb) return pa > pb;
  bool qa = a.held == IT_QUICK_CLAW && !a.fainted() && random(100) < ITEM_TBL[IT_QUICK_CLAW].param;
  bool qb = b.held == IT_QUICK_CLAW && !b.fainted() && random(100) < ITEM_TBL[IT_QUICK_CLAW].param;
  if (qa != qb) {
    if (claw) *claw = qa ? 1 : 2;
    gameEvent(EV_ITEM_USED, IT_QUICK_CLAW);
    return qa;
  }
  uint16_t sa = effStat(a, SI_SPE), sb = effStat(b, SI_SPE);
  // TRICK ROOM: the slower acts first, within a priority bracket
  if (sa != sb) return gField.trickRoom ? sa < sb : sa > sb;
  return random(2) == 0;               // a genuine speed tie is a coin flip
}

// ---------- one action ----------

static const uint8_t STAGE_BIT[SI_COUNT] = { ST_ATK, ST_DEF, ST_SPA, ST_SPD, ST_SPE };

// True if anything moved. MIST stops a FOE from lowering them (from_foe).
static bool applyStages(Combatant &c, uint8_t mask, int8_t delta, bool fromFoe = false) {
  if (fromFoe && delta < 0 && sideOf(c).mist) return false;
  bool moved = false;
  for (int i = 0; i < SI_COUNT; i++) {
    if (!(mask & STAGE_BIT[i])) continue;
    int v = c.stage[i] + delta;
    v = v > 6 ? 6 : (v < -6 ? -6 : v);
    if (v != c.stage[i]) moved = true;
    c.stage[i] = (int8_t)v;
  }
  if (mask & ST_ACC) { int v = c.accStage + delta; v = v > 6 ? 6 : v < -6 ? -6 : v; if (v != c.accStage) moved = true; c.accStage = (int8_t)v; }
  if (mask & ST_EVA) { int v = c.evaStage + delta; v = v > 6 ? 6 : v < -6 ? -6 : v; if (v != c.evaStage) moved = true; c.evaStage = (int8_t)v; }
  return moved;
}
static void clearStages(Combatant &c) {
  for (int i = 0; i < SI_COUNT; i++) c.stage[i] = 0;
  c.accStage = c.evaStage = 0;
}

static void hurt(Combatant &c, uint16_t amount) {
  c.hp = (amount >= c.hp) ? 0 : c.hp - amount;
}

static void heal(Combatant &c, uint16_t amount) {
  uint32_t v = (uint32_t)c.hp + amount;
  c.hp = v > c.maxHp ? c.maxHp : (uint16_t)v;
}

static void itemFired(Combatant &c, uint8_t &slot) {
  c.itemUsed = true;
  slot = c.held;
  gameEvent(EV_ITEM_USED, c.held);
}
static void checkBerry(Combatant &c, uint8_t &logItem) {
  if (c.itemUsed || c.fainted()) return;
  if (c.held == IT_LUM_BERRY) {
    if (c.ailment == AIL_NONE && !c.confuseTurns) return;
    c.ailment = AIL_NONE; c.ailTurns = 0; c.confuseTurns = 0; c.toxicN = 0;
    itemFired(c, logItem); return;
  }
  if (c.held != IT_ORAN_BERRY && c.held != IT_SITRUS_BERRY) return;
  if ((uint32_t)c.hp * 2 > c.maxHp) return;
  uint16_t amount = c.held == IT_ORAN_BERRY ? (uint16_t)ITEM_TBL[IT_ORAN_BERRY].param
      : (uint16_t)((uint32_t)c.maxHp * ITEM_TBL[IT_SITRUS_BERRY].param / 100);
  heal(c, amount ? amount : 1);
  itemFired(c, logItem);
}
static uint16_t sashCap(Combatant &c, uint16_t damage, uint8_t &logItem) {
  if (c.held != IT_FOCUS_SASH || c.itemUsed || c.hp != c.maxHp || c.hp < 2 || damage < c.hp) return damage;
  itemFired(c, logItem);
  return c.hp - 1;
}

// A hit landing on the creature itself (not a substitute): ENDURE, the sash,
// and what COUNTER and friends remember. DESTINY BOND takes the attacker too.
static uint16_t takeHit(Combatant &atk, Combatant &def, uint16_t d, uint8_t cat, TurnLog &log) {
  if (def.enduring && d >= def.hp && def.hp > 1) { d = def.hp - 1; log.add(BN_ENDURED, 1); }
  d = sashCap(def, d, log.defItem);
  hurt(def, d);
  def.hurtThisTurn = (uint16_t)(def.hurtThisTurn + d);
  def.lastHitDmg = d;
  def.lastHitCat = cat;
  if (def.fainted() && def.destinyBond && !atk.fainted()) {
    atk.hp = 0;
    log.add(BN_DESTINY_TOOK, 0);
    log.userFainted = true;
  }
  return d;
}

// Moves that reach the foe and so meet its PROTECT and its SUBSTITUTE.
// Hazards are laid on the field, not on the creature, and pass both.
static bool aimsAtFoe(const MoveEntry &m) {
  return m.target == TG_FOE && m.effect != EF_HAZARD;
}

static bool hitsInvulnerable(const MoveEntry &m, const Combatant &def) {
  if (!def.charging || def.charging >= MOVE_COUNT || !(MOVE_TBL[def.charging].param & 1)) return true;
  uint8_t t = MOVE_TBL[def.charging].type;      // where it went: up, down, under water, or nowhere
  if (t == T_FLYING) return (m.flags & MF_HITS_AIR) != 0;
  if (t == T_GROUND) return (m.flags & MF_HITS_GROUND) != 0;
  if (t == T_WATER) return (m.flags & MF_HITS_WATER) != 0;
  return false;                                  // PHANTOM FORCE, SHADOW FORCE: nothing reaches it
}

static bool rollHit(const Combatant &atk, const Combatant &def, const MoveEntry &m) {
  if (!hitsInvulnerable(m, def)) return false;
  if (m.effect == EF_OHKO) {
    if (atk.level < def.level) return false;
    return random(100) < (int)(30 + atk.level - def.level);
  }
  if (!m.acc || m.effect == EF_NEVER_MISS) return true;
  if ((m.flags & MF_RAIN_SURE) && gField.weather == W_RAIN) return true;
  if ((m.flags & MF_HAIL_SURE) && gField.weather == W_HAIL) return true;
  uint32_t acc = m.acc;
  if ((m.flags & MF_RAIN_SURE) && gField.weather == W_SUN) acc = 50;
  int st = atk.accStage - def.evaStage;
  st = st > 6 ? 6 : st < -6 ? -6 : st;
  acc = st >= 0 ? acc * (3 + st) / 3 : acc * 3 / (3 - st);
  return random(100) < (long)acc;
}

static void fail(Combatant &atk, TurnLog &log) {
  log.add(BN_FAILED, 0);
  atk.lastFailed = true;
}

static bool crit(const Combatant &atk, const MoveEntry &m) {
  uint8_t s = m.crit + atk.critStage;
  if (atk.laserFocus || s >= 3) return true;
  return s == 2 ? random(2) == 0 : s == 1 ? random(8) == 0 : random(16) == 0;   // base ~6%, the series' rate
}

// SLEEP TALK picks among the user's other moves; these cannot be picked.
static bool sleepTalkable(uint16_t mv) {
  if (!mv || mv >= MOVE_COUNT) return false;
  uint8_t e = MOVE_TBL[mv].effect;
  return e != EF_SLEEPTALK && e != EF_CHARGE && e != EF_FOCUSPUNCH && e != EF_LOCKIN;
}

static void statusMove(Combatant &atk, Combatant &def, uint16_t mv, const MoveEntry &m, TurnLog &log);
static void endLockIn(Combatant &atk, TurnLog &log);
static bool futureSight(Combatant &atk, Combatant &def, uint16_t mv, TurnLog &log);
static void actCore(Combatant &atk, Combatant &def, uint16_t mv, TurnLog &log);

// One creature's action, then the bookkeeping every path shares: which move it
// last used and how many times running (FURY CUTTER, PROTECT's odds, ENCORE).
void battleAct(Combatant &atk, Combatant &def, uint16_t mv, TurnLog &log) {
  actCore(atk, def, mv, log);
  if (log.skipped || !log.move) return;
  if (log.move == atk.lastMove && !log.missed && !atk.lastFailed) atk.streak = atk.streak < 255 ? atk.streak + 1 : 255;
  else atk.streak = (log.missed || atk.lastFailed) ? 0 : 1;
  atk.lastMove = log.move;
}

static void actCore(Combatant &atk, Combatant &def, uint16_t mv, TurnLog &log) {
  log = TurnLog();
  log.move = mv;
  if (atk.fainted() || def.fainted()) { log.skipped = true; return; }
  atk.movedThisTurn = true;
  // DESTINY BOND lasts until the user's own next move
  if (atk.destinyBond && !(mv && mv < MOVE_COUNT && MOVE_TBL[mv].effect == EF_DESTINYBOND)) atk.destinyBond = false;

  // --- things that can cost the turn before a move is even chosen
  if (atk.recharge) { atk.recharge = false; log.skipped = true; return; }
  if (atk.flinched) {
    atk.flinched = false;
    log.move = 0;
    log.add(BN_FLINCHED, 0);
    return;
  }
  if (atk.ailment == AIL_FREEZE) {
    if (random(100) < 20) atk.ailment = AIL_NONE;   // thaws
    else { log.skipped = true; return; }
  }
  bool sleepMove = mv && mv < MOVE_COUNT && (MOVE_TBL[mv].effect == EF_SLEEPTALK || MOVE_TBL[mv].effect == EF_SNORE);
  if (atk.ailment == AIL_SLEEP) {
    if (atk.ailTurns) atk.ailTurns--;
    if (atk.ailTurns == 0) { atk.ailment = AIL_NONE; log.add(BN_WOKE, 0); }
    else if (!sleepMove) { log.skipped = true; return; }
  }
  if (atk.ailment == AIL_PARA && random(100) < 25) { log.skipped = true; return; }
  if (atk.confuseTurns) {
    atk.confuseTurns--;
    if (random(100) < 33) {               // hits itself instead
      uint16_t self = (2UL * atk.level / 5 + 2) * 40 *
                          atk.base[SI_ATK] / (atk.base[SI_DEF] ? atk.base[SI_DEF] : 1) / 50 + 2;
      self = sashCap(atk, self, log.atkItem);
      hurt(atk, self);
      log.hurtSelf = true;
      checkBerry(atk, log.atkItem);
      log.damage = self;
      if (atk.lockMove) endLockIn(atk, log);
      return;
    }
  }

  // a wound-up EF_CHARGE move fires this turn instead of whatever was picked
  if (atk.charging) { mv = atk.charging; atk.charging = 0; }
  else if (mv && mv < MOVE_COUNT && MOVE_TBL[mv].effect == EF_CHARGE &&
           !((MOVE_TBL[mv].param & 2) && gField.weather == W_SUN)) {   // SOLAR BEAM fires at once in sun
    atk.charging = mv;
    if (MOVE_TBL[mv].param & 4) applyStages(atk, ST_DEF, 1);          // SKULL BASH
    log.charged = true;
    return;
  }

  if (!mv || mv >= MOVE_COUNT) { log.skipped = true; return; }
  const MoveEntry *mp = &MOVE_TBL[mv];
  log.move = mv;

  // TAUNT: no status moves
  if (atk.tauntTurns && mp->cat == MC_STATUS) { log.add(BN_TAUNT_BLOCK, 0); atk.lastFailed = true; return; }

  // moves that only work in a particular situation
  if (mp->effect == EF_SLEEPTALK) {
    if (atk.ailment != AIL_SLEEP) { fail(atk, log); return; }
    uint16_t opts[MOVE_SLOTS]; uint8_t n = 0;
    for (int i = 0; i < MOVE_SLOTS; i++) if (sleepTalkable(atk.moves[i])) opts[n++] = atk.moves[i];
    if (!n) { fail(atk, log); return; }
    mv = opts[random(n)];
    mp = &MOVE_TBL[mv];
    log.move = mv;
  }
  const MoveEntry &m = *mp;
  if (m.effect == EF_SNORE && atk.ailment != AIL_SLEEP) { fail(atk, log); return; }
  if (m.effect == EF_FAKEOUT && atk.turnsOut > 0) { fail(atk, log); return; }
  if (m.effect == EF_FOCUSPUNCH && atk.hurtThisTurn) { log.add(BN_FOCUS_LOST, 0); atk.lastFailed = true; return; }
  if (m.effect == EF_SUCKER && !(def.queuedMove && def.queuedMove < MOVE_COUNT && !def.movedThisTurn &&
                                 MOVE_TBL[def.queuedMove].cat != MC_STATUS)) { fail(atk, log); return; }
  if (m.effect == EF_DREAMEATER && def.ailment != AIL_SLEEP) { fail(atk, log); return; }
  if (gField.terrain == TR_PSYCHIC && m.prio > 0 && aimsAtFoe(m) && grounded(def)) { fail(atk, log); return; }
  if (m.effect == EF_OHKO && atk.level < def.level) { fail(atk, log); return; }   // before the roll: it cannot work at all

  // FUTURE SIGHT is aimed now and lands later, through PROTECT and all
  if (futureSight(atk, def, mv, log)) return;

  // PROTECT, before anything can land. ROAR and WHIRLWIND go straight through it.
  if (def.protecting && aimsAtFoe(m) && !(m.effect == EF_FORCESWITCH && m.cat == MC_STATUS)) {
    log.add(BN_PROTECTED, 1);
    atk.lastFailed = true;
    if (m.effect == EF_SELFKO) { atk.hp = 0; log.userFainted = true; }
    if (atk.lockMove) endLockIn(atk, log);
    return;
  }

  // --- accuracy
  if (!rollHit(atk, def, m)) {
    log.missed = true;
    atk.lastFailed = true;
    if (m.effect == EF_SELFKO) { atk.hp = 0; log.userFainted = true; }
    if (atk.lockMove) endLockIn(atk, log);
    return;
  }
  atk.lastFailed = false;

  if (m.cat == MC_STATUS) {
    statusMove(atk, def, mv, m, log);
    if (m.effect != EF_PROTECT && m.effect != EF_ENDURE) atk.protectChain = 0;
    return;
  }
  atk.protectChain = 0;

  // --- damage, including multi-hit
  uint8_t hits = (m.effect == EF_MULTI) ? (m.param ? (uint8_t)m.param : (uint8_t)(2 + random(4))) : 1;
  uint16_t total = 0;
  uint8_t type = battleMoveType(atk, mv);
  log.effPct = typeEffVsDex(type, def.dex);
  if ((m.flags & MF_FREEZE_DRY) && isType(def, T_WATER)) log.effPct *= 4;
  if (log.effPct == 0) { log.immune = true; atk.lastFailed = true; return; }
  if (m.effect == EF_OHKO && atk.level < def.level) { fail(atk, log); return; }
  if (m.effect == EF_ENDEAVOR && def.hp <= atk.hp) { fail(atk, log); return; }
  if (m.effect == EF_COUNTER && !damageCore(atk, def, mv, false, 255, 0)) { fail(atk, log); return; }

  if (m.flags & MF_BREAK_SCREEN) {
    BattleSide &ds = sideOf(def);
    if (ds.reflect || ds.lscreen || ds.veil) { ds.reflect = ds.lscreen = ds.veil = 0; log.add(BN_SCREEN_BROKE, 1); }
  }
  uint16_t power = 0;
  if (m.effect == EF_MAGNITUDE) {
    static const uint8_t P[7] = { 10, 30, 50, 70, 90, 110, 150 };
    static const uint8_t ODDS[7] = { 5, 15, 35, 65, 85, 95, 100 };   // cumulative, the games' 5/10/20/30/20/10/5
    long r = random(100); uint8_t k = 0;
    while (k < 6 && r >= ODDS[k]) k++;
    power = P[k];
  }
  bool subHit = false;
  for (uint8_t h = 0; h < hits; h++) {
    // fixed damage never crits
    bool fixedDmg = m.effect == EF_OHKO || m.effect == EF_HALFHP || m.effect == EF_ENDEAVOR ||
                    m.effect == EF_FIXED || m.effect == EF_FIXED_LVL || m.effect == EF_COUNTER ||
                    m.effect == EF_FINALGAMBIT;
    bool c = fixedDmg ? false : crit(atk, m);
    uint16_t d = damageCore(atk, def, mv, c, (uint8_t)(217 + random(39)), power);
    if (c) log.crit = true;
    if (def.subHp) {
      subHit = true;
      uint16_t take = d < def.subHp ? d : def.subHp;
      def.subHp -= take;
      total += take;
      if (!def.subHp) { log.add(BN_SUB_GONE, 1); break; }
      continue;
    }
    total += takeHit(atk, def, d, m.cat, log);
    if (def.fainted()) { hits = h + 1; break; }
  }
  atk.laserFocus = false;
  if (subHit && def.subHp) log.add(BN_SUB_HIT, 1);
  if (type == T_ELECTRIC) atk.chargedUp = false;
  log.hits = hits;
  log.damage = total;
  if (m.effect == EF_OHKO && def.fainted()) log.add(BN_OHKO, 1);

  // A damaging move's stage change: param is its CHANCE (ROCK TOMB 100, ENERGY
  // BALL 10). Onto the foe only if it is still standing; onto the user
  // (SUPERPOWER, CLOSE COMBAT, OVERHEAT) whatever happened to the foe.
  if (m.effect == EF_STAGE && (m.target == TG_SELF || (!def.fainted() && !subHit)) &&
      random(100) < (m.param ? m.param : 100)) {
    if (applyStages(m.target == TG_SELF ? atk : def, m.statMask, m.stages, m.target != TG_SELF)) {
      log.stageMask = m.statMask;
      log.stageDelta = m.stages;
    }
  }
  if (m.effect == EF_RECOIL && m.param > 0) { hurt(atk, total / m.param ? total / m.param : 1); if (atk.fainted()) log.userFainted = true; }
  if ((m.effect == EF_DRAIN || m.effect == EF_DREAMEATER) && m.param > 0) heal(atk, total * m.param / 100);
  if (m.effect == EF_RECHARGE) atk.recharge = true;
  if (m.effect == EF_SELFKO || m.effect == EF_FINALGAMBIT) { atk.hp = 0; log.userFainted = true; log.add(BN_SELF_FAINT, 0); }
  if (m.effect == EF_KNOCKOFF && !subHit && !def.fainted() && def.held != ITEM_NONE && !def.itemUsed) {
    // gone for THIS battle only: the stored creature still holds it afterwards
    def.held = ITEM_NONE;
    log.add(BN_KNOCKED, 1);
  }
  if (m.effect == EF_SPIN && !atk.fainted()) {
    BattleSide &s = sideOf(atk);
    if (s.rocks || s.spikes || s.tspikes || s.web || atk.seeded || atk.bindTurns) log.add(BN_HAZARDS_GONE, 0);
    s.rocks = s.spikes = s.tspikes = s.web = 0;
    atk.seeded = false; atk.bindTurns = 0;
  }
  if (m.effect == EF_CLEARSMOG && !subHit && !def.fainted()) { clearStages(def); log.add(BN_STAGES_RESET, 1); }
  if (m.effect == EF_FORCESWITCH && !subHit && !def.fainted() && !def.ingrained) log.forceOut = true;   // DRAGON TAIL
  if (m.effect == EF_UTURN && !atk.fainted()) log.selfOut = true;                                      // U-TURN, VOLT SWITCH
  if (m.effect == EF_BIND && !subHit && !def.fainted() && !def.bindTurns) {
    def.bindTurns = (uint8_t)(4 + random(2));
    log.add(BN_BOUND, 1);
  }
  if (m.effect == EF_WAKEUP && !def.fainted() && def.ailment == (uint8_t)m.param) {
    def.ailment = AIL_NONE; def.ailTurns = 0;
    log.add(BN_CURED, 1);
  }
  if (m.effect == EF_LOCKIN && !atk.lockMove) { atk.lockMove = mv; atk.lockTurns = (uint8_t)(1 + random(2)); }
  else if (atk.lockMove && atk.lockTurns == 0) endLockIn(atk, log);

  // --- secondary ailment, and flinching. Neither gets through a substitute.
  if (!subHit && !def.fainted()) {
    if (m.effect == EF_TRIATTACK) {
      static const uint8_t TRI[3] = { AIL_BURN, AIL_FREEZE, AIL_PARA };
      if (random(100) < m.ailChance) inflictAilment(def, TRI[random(3)], log);
    } else if (m.ailment != AIL_NONE && m.ailChance && random(100) < m.ailChance) {
      inflictAilment(def, m.ailment, log);
    }
    bool flinch = (m.effect == EF_FAKEOUT && m.param == 0) || (m.flinch && random(100) < m.flinch);
    if (flinch && !def.movedThisTurn) def.flinched = true;   // only the one still to act can flinch
  }
  checkBerry(def, log.defItem);
  checkBerry(atk, log.atkItem);
  log.targetFainted = def.fainted();
}

static void endLockIn(Combatant &atk, TurnLog &log) {
  atk.lockMove = 0;
  atk.lockTurns = 0;
  if (!atk.confuseTurns && !atk.fainted()) {
    atk.confuseTurns = (uint8_t)(2 + random(3));
    log.add(BN_TIRED, 0);
  }
}

// Everything a STATUS move does, once it has hit.
static void statusMove(Combatant &atk, Combatant &def, uint16_t mv, const MoveEntry &m, TurnLog &log) {
  BattleSide &mine = sideOf(atk), &theirs = sideOf(def);
  // a substitute blocks what is aimed at the creature behind it
  if (def.subHp && aimsAtFoe(m) && m.effect != EF_DEFOG && m.effect != EF_PERISH &&
      m.effect != EF_FORCESWITCH) { fail(atk, log); return; }
  // A STATUS move that inflicts an ailment (THUNDER WAVE, SLEEP POWDER,
  // HYPNOSIS, TOXIC, WILL-O-WISP, CONFUSE RAY). As in the games, a status
  // move ignores the type chart -- CONFUSE RAY works on a Normal type --
  // except THUNDER WAVE, which a Ground type is immune to. The rest is the
  // same rule as a secondary ailment (inflictAilment: a Fire type cannot be
  // burned, a Steel or Poison type poisoned, nobody gets a second status).
  if (m.ailment != AIL_NONE && m.effect == EF_NONE) {
    if (m.type == T_ELECTRIC && typeEffVsDex(m.type, def.dex) == 0) { log.immune = true; return; }
    if (!inflictAilment(def, m.ailment, log)) log.missed = true;   // nothing to take hold of
    else if (mv == MV_TOXIC) {
      def.toxicN = 1;                                               // TOXIC: badly poisoned
      log.add(BN_TOXIC, 1);
    }
    return;
  }
  switch (m.effect) {
    case EF_HEAL:
      heal(atk, (uint32_t)atk.maxHp * (m.param > 0 ? m.param : 50) / 100);
      log.healed = true;
      break;
    case EF_HEAL_WEATHER: {
      uint32_t num = 1, den = 2;
      bool boost = (m.param == 1) ? gField.weather == W_SAND : gField.weather == W_SUN;
      if (boost) { num = 2; den = 3; }
      else if (m.param == 0 && gField.weather != W_NONE) { num = 1; den = 4; }
      heal(atk, (uint32_t)atk.maxHp * num / den);
      log.healed = true;
      break;
    }
    case EF_STAGE: {
      Combatant &t = (m.target == TG_SELF) ? atk : def;
      if (applyStages(t, m.statMask, m.stages, m.target != TG_SELF)) {
        log.stageMask = m.statMask;
        log.stageDelta = m.stages;
      } else fail(atk, log);
      break;
    }
    case EF_PROTECT: case EF_ENDURE: {
      // each consecutive use is a third as likely to work
      uint32_t odds = 1;
      for (uint8_t i = 0; i < atk.protectChain && odds < 729; i++) odds *= 3;
      if (!def.movedThisTurn && random(odds) == 0) {
        if (m.effect == EF_PROTECT) atk.protecting = true; else atk.enduring = true;
        atk.protectChain++;
        log.add(BN_BRACED, 0);
      } else { atk.protectChain = 0; fail(atk, log); }
      return;
    }
    case EF_WEATHER:
      if (gField.weather == (uint8_t)m.param) { fail(atk, log); break; }
      gField.weather = (uint8_t)m.param; gField.weatherTurns = 5;
      log.add((uint8_t)(BN_RAIN + m.param - 1), 0);
      break;
    case EF_TERRAIN:
      if (gField.terrain == (uint8_t)m.param) { fail(atk, log); break; }
      gField.terrain = (uint8_t)m.param; gField.terrainTurns = 5;
      log.add(BN_TERRAIN, 0);
      break;
    case EF_HAZARD: {
      bool ok = false;
      switch (m.param) {
        case 1: if (!theirs.rocks) { theirs.rocks = 1; ok = true; log.add(BN_ROCKS, 1); } break;
        case 2: if (theirs.spikes < 3) { theirs.spikes++; ok = true; log.add(BN_SPIKES, 1); } break;
        case 3: if (theirs.tspikes < 2) { theirs.tspikes++; ok = true; log.add(BN_TSPIKES, 1); } break;
        case 4: if (!theirs.web) { theirs.web = 1; ok = true; log.add(BN_WEB, 1); } break;
      }
      if (!ok) fail(atk, log);
      break;
    }
    case EF_DEFOG:
      applyStages(def, ST_EVA, -1, true);
      if (theirs.reflect || theirs.lscreen || theirs.veil) log.add(BN_SCREEN_BROKE, 1);
      theirs.reflect = theirs.lscreen = theirs.veil = theirs.safeguard = theirs.mist = 0;
      for (int s = 0; s < 2; s++) gField.side[s].rocks = gField.side[s].spikes = gField.side[s].tspikes = gField.side[s].web = 0;
      log.add(BN_HAZARDS_GONE, 0);
      break;
    case EF_SCREEN:
      if (m.param == 1) { if (mine.reflect) { fail(atk, log); break; } mine.reflect = 5; log.add(BN_REFLECT, 0); }
      else if (m.param == 2) { if (mine.lscreen) { fail(atk, log); break; } mine.lscreen = 5; log.add(BN_LSCREEN, 0); }
      else { if (mine.veil || gField.weather != W_HAIL) { fail(atk, log); break; } mine.veil = 5; log.add(BN_VEIL, 0); }
      break;
    case EF_TAILWIND: if (mine.tailwind) fail(atk, log); else { mine.tailwind = 4; log.add(BN_TAILWIND, 0); } break;
    case EF_SAFEGUARD: if (mine.safeguard) fail(atk, log); else { mine.safeguard = 5; log.add(BN_SAFEGUARD, 0); } break;
    case EF_MIST: if (mine.mist) fail(atk, log); else { mine.mist = 5; log.add(BN_MIST, 0); } break;
    case EF_TRICKROOM:
      gField.trickRoom = gField.trickRoom ? 0 : 5;   // using it again ends it, as in the games
      log.add(BN_TRICKROOM, 0);
      break;
    case EF_SUBSTITUTE: {
      uint16_t cost = atk.maxHp / 4;
      if (atk.subHp || atk.hp <= cost || !cost) { fail(atk, log); break; }
      hurt(atk, cost);
      atk.subHp = cost;
      log.add(BN_SUB_UP, 0);
      break;
    }
    case EF_REST:
      if (atk.hp == atk.maxHp || atk.ailment == AIL_SLEEP) { fail(atk, log); break; }
      atk.hp = atk.maxHp;
      atk.ailment = AIL_SLEEP; atk.ailTurns = 3;    // asleep for exactly two turns
      atk.toxicN = 0;
      log.add(BN_REST, 0);
      break;
    case EF_LEECHSEED:
      if (def.seeded || isType(def, T_GRASS)) { fail(atk, log); break; }
      def.seeded = true;
      log.add(BN_SEEDED, 1);
      break;
    case EF_YAWN:
      if (def.yawn || def.ailment != AIL_NONE || theirs.safeguard) { fail(atk, log); break; }
      def.yawn = 2;
      log.add(BN_DROWSY, 1);
      break;
    case EF_TAUNT:
      if (def.tauntTurns) { fail(atk, log); break; }
      def.tauntTurns = 3;
      log.add(BN_TAUNTED, 1);
      break;
    case EF_ENCORE:
      if (def.encoreTurns || !def.lastMove || def.lastMove >= MOVE_COUNT ||
          MOVE_TBL[def.lastMove].effect == EF_ENCORE) { fail(atk, log); break; }
      def.encoreTurns = 3; def.encoreMove = def.lastMove;
      log.add(BN_ENCORED, 1);
      break;
    case EF_CURSE:
      if (isType(atk, T_GHOST)) {
        if (def.cursed) { fail(atk, log); break; }
        hurt(atk, atk.maxHp / 2 ? atk.maxHp / 2 : 1);
        def.cursed = true;
        log.add(BN_CURSED, 1);
        if (atk.fainted()) log.userFainted = true;
      } else {
        applyStages(atk, ST_ATK | ST_DEF, 1);
        applyStages(atk, ST_SPE, -1);
        log.add(BN_STAGE_UP, 0);
      }
      break;
    case EF_BELLYDRUM:
      if (atk.hp <= atk.maxHp / 2 || atk.stage[SI_ATK] >= 6) { fail(atk, log); break; }
      hurt(atk, atk.maxHp / 2);
      atk.stage[SI_ATK] = 6;
      log.add(BN_DRUM, 0);
      break;
    case EF_SHELLSMASH:
      applyStages(atk, ST_ATK | ST_SPA | ST_SPE, 2);
      applyStages(atk, ST_DEF | ST_SPD, -1);
      log.add(BN_STAGE_UP, 0);
      break;
    case EF_HAZE: clearStages(atk); clearStages(def); log.add(BN_STAGES_RESET, 0); break;
    case EF_PSYCHUP:
      for (int i = 0; i < SI_COUNT; i++) atk.stage[i] = def.stage[i];
      atk.accStage = def.accStage; atk.evaStage = def.evaStage;
      log.add(BN_PSYCHUP, 0);
      break;
    case EF_TOPSYTURVY:
      for (int i = 0; i < SI_COUNT; i++) def.stage[i] = (int8_t)-def.stage[i];
      def.accStage = (int8_t)-def.accStage; def.evaStage = (int8_t)-def.evaStage;
      log.add(BN_INVERTED, 1);
      break;
    case EF_ACUPRESSURE: {
      uint8_t open[7], n = 0;
      for (int i = 0; i < SI_COUNT; i++) if (atk.stage[i] < 6) open[n++] = STAGE_BIT[i];
      if (atk.accStage < 6) open[n++] = ST_ACC;
      if (atk.evaStage < 6) open[n++] = ST_EVA;
      if (!n) { fail(atk, log); break; }
      uint8_t bit = open[random(n)];
      applyStages(atk, bit, 2);
      log.stageMask = bit; log.stageDelta = 2;
      break;
    }
    case EF_PAINSPLIT: {
      uint16_t avg = (uint16_t)(((uint32_t)atk.hp + def.hp) / 2);
      atk.hp = avg > atk.maxHp ? atk.maxHp : avg;
      def.hp = avg > def.maxHp ? def.maxHp : avg;
      log.add(BN_PAINSPLIT, 0);
      break;
    }
    case EF_SWAGGER:
      applyStages(def, m.statMask, m.stages);
      log.stageMask = m.statMask; log.stageDelta = m.stages;
      if (!inflictAilment(def, AIL_CONFUSE, log)) {}
      break;
    case EF_DESTINYBOND:
      if (atk.lastMove && atk.lastMove < MOVE_COUNT && MOVE_TBL[atk.lastMove].effect == EF_DESTINYBOND && atk.streak) { fail(atk, log); break; }
      atk.destinyBond = true;
      log.add(BN_DESTINY, 0);
      break;
    case EF_PERISH:
      if (!atk.perish) atk.perish = 4;
      if (!def.perish) def.perish = 4;
      log.add(BN_PERISH, 0);
      break;
    case EF_AQUARING:
      if (m.param) { if (atk.ingrained) { fail(atk, log); break; } atk.ingrained = true; }
      else { if (atk.aquaRing) { fail(atk, log); break; } atk.aquaRing = true; }
      log.add(BN_AQUARING, 0);
      break;
    case EF_WISH:
      if (mine.wishTurns) { fail(atk, log); break; }
      mine.wishTurns = 2; mine.wishHp = atk.maxHp / 2;
      log.add(BN_WISH, 0);
      break;
    case EF_FOCUSENERGY:
      if (m.param == 3) atk.laserFocus = true;
      else { if (atk.critStage >= 2) { fail(atk, log); break; } atk.critStage = 2; }
      log.add(BN_PUMPED, 0);
      break;
    case EF_CHARGEUP:
      atk.chargedUp = true;
      applyStages(atk, ST_SPD, 1);
      log.add(BN_CHARGED, 0);
      break;
    case EF_TRAP:
      if (def.trapped) { fail(atk, log); break; }
      def.trapped = true;
      log.add(BN_TRAPPED, 1);
      break;
    case EF_MEMENTO:
      applyStages(def, m.statMask, m.stages, true);
      log.stageMask = m.statMask; log.stageDelta = m.stages;
      atk.hp = 0; log.userFainted = true; log.add(BN_SELF_FAINT, 0);
      break;
    case EF_SPLASH: log.add(BN_NOTHING, 0); break;
    case EF_FORCESWITCH:
      if (def.ingrained) { fail(atk, log); break; }
      log.forceOut = true;          // the screen picks the teammate, or ends a wild fight
      break;
    case EF_UTURN:                  // PARTING SHOT: the drop, then out
      if (m.statMask) { applyStages(def, m.statMask, m.stages, true); log.stageMask = m.statMask; log.stageDelta = m.stages; }
      log.selfOut = true;
      break;
    case EF_BATONPASS: log.selfOut = true; log.baton = true; break;
    case EF_FUTURE: break;   // damaging; never reaches here
    default: break;
  }
}

// FUTURE SIGHT and DOOM DESIRE are SPECIAL moves (they have power), but they do
// not land on the turn they are used: battleAct's damage path would hit at
// once, so they are caught here first. Returns true if it handled the move.
static bool futureSight(Combatant &atk, Combatant &def, uint16_t mv, TurnLog &log) {
  if (!mv || mv >= MOVE_COUNT || MOVE_TBL[mv].effect != EF_FUTURE) return false;
  BattleSide &theirs = sideOf(def);
  if (theirs.futureTurns) { fail(atk, log); return true; }
  theirs.futureTurns = 3;   // lands at the end of the turn after next
  theirs.futureDmg = damageCore(atk, def, mv, false, 236, 0);
  log.add(BN_FUTURE, 0);
  return true;
}

// THE ailment rule, for a damaging move's secondary chance and a status move
// alike. Never overwrites an existing status; confusion is tracked separately
// so it can stack with a real one, as in the games; and a type cannot be given
// the status it is made of. True if it took hold.
static bool inflictAilment(Combatant &def, uint8_t ail, TurnLog &log) {
  if (sideOf(def).safeguard) return false;
  if (ail == AIL_CONFUSE) {
    if (def.confuseTurns) return false;
    if (gField.terrain == TR_MISTY && grounded(def)) return false;
    def.confuseTurns = 2 + random(3);
    log.inflicted = AIL_CONFUSE;
    return true;
  }
  if (def.ailment != AIL_NONE) return false;
  if (gField.terrain == TR_MISTY && grounded(def)) return false;
  if (ail == AIL_SLEEP && gField.terrain == TR_ELECTRIC && grounded(def)) return false;
  bool immune = (ail == AIL_BURN && hasStab(def.dex, T_FIRE)) ||
                (ail == AIL_FREEZE && (hasStab(def.dex, T_ICE) || gField.weather == W_SUN)) ||
                (ail == AIL_POISON && (hasStab(def.dex, T_POISON) || hasStab(def.dex, T_STEEL))) ||
                (ail == AIL_PARA && hasStab(def.dex, T_ELECTRIC));
  if (immune) return false;
  def.ailment = ail;
  if (ail == AIL_SLEEP) def.ailTurns = 2 + random(3);
  log.inflicted = ail;
  return true;
}

// ---------- turn bookkeeping ----------

void battleTurnStart(Combatant &a, uint16_t &ma, Combatant &b, uint16_t &mb) {
  Combatant *c[2] = { &a, &b };
  uint16_t *m[2] = { &ma, &mb };
  for (int i = 0; i < 2; i++) {
    Combatant &x = *c[i];
    x.protecting = x.enduring = false;
    x.flinched = false;
    x.movedThisTurn = false;
    x.hurtThisTurn = 0;
    // the locks override the choice: a THRASH in progress, then ENCORE
    if (x.lockMove && *m[i]) { *m[i] = x.lockMove; if (x.lockTurns) x.lockTurns--; }
    else if (x.encoreTurns && x.encoreMove && *m[i]) *m[i] = x.encoreMove;
    x.queuedMove = *m[i];
  }
}

void battleOnSwitchOut(Combatant &c) {
  clearStages(c);
  c.confuseTurns = 0;
  c.critStage = 0; c.laserFocus = false;
  c.subHp = 0;
  c.seeded = false; c.bindTurns = 0; c.trapped = false; c.ingrained = false; c.aquaRing = false;
  c.yawn = 0; c.tauntTurns = 0; c.encoreTurns = 0; c.encoreMove = 0;
  c.lastMove = 0; c.streak = 0; c.lastFailed = false;
  if (c.toxicN) c.toxicN = 1;   // still badly poisoned, but the count starts over
  c.destinyBond = false; c.perish = 0; c.cursed = false; c.chargedUp = false;
  c.lockMove = 0; c.lockTurns = 0;
  c.charging = 0; c.recharge = false;
  c.protecting = c.enduring = false; c.protectChain = 0;
  c.turnsOut = 0;
}

void battleBatonPass(const Combatant &from, Combatant &to) {
  for (int i = 0; i < SI_COUNT; i++) to.stage[i] = from.stage[i];
  to.accStage = from.accStage; to.evaStage = from.evaStage;
  to.subHp = from.subHp;
  to.confuseTurns = from.confuseTurns;
  to.critStage = from.critStage;
  to.seeded = from.seeded;
  to.aquaRing = from.aquaRing; to.ingrained = from.ingrained;
  to.cursed = from.cursed;
  to.perish = from.perish;
}

bool battleCanSwitch(const Combatant &c) {
  return !(c.trapped || c.bindTurns || c.ingrained);
}

void battleOnEntry(Combatant &c, uint8_t side, TurnLog &log) {
  log = TurnLog();
  c.side = side & 1;
  c.turnsOut = 0;
  if (c.fainted()) return;
  BattleSide &s = sideOf(c);
  if (s.rocks) {
    // an eighth, times the type chart's view of Rock on it
    uint32_t d = (uint32_t)c.maxHp * typeEffVsDex(T_ROCK, c.dex) / 800;
    hurt(c, d ? (uint16_t)d : 1);
    log.add(BN_ROCKS_HURT, 0);
    log.damage = (uint16_t)d;
  }
  if (s.spikes && grounded(c)) {
    uint16_t d = s.spikes == 1 ? c.maxHp / 8 : s.spikes == 2 ? c.maxHp / 6 : c.maxHp / 4;
    hurt(c, d ? d : 1);
    log.add(BN_SPIKES_HURT, 0);
  }
  if (s.tspikes && grounded(c)) {
    if (isType(c, T_POISON)) { s.tspikes = 0; log.add(BN_HAZARDS_GONE, 0); }   // a Poison type soaks them up
    else if (!c.fainted() && c.ailment == AIL_NONE && !isType(c, T_STEEL) && !s.safeguard) {
      c.ailment = AIL_POISON;
      if (s.tspikes >= 2) c.toxicN = 1;
      log.inflicted = AIL_POISON;
    }
  }
  if (s.web && grounded(c) && !c.fainted()) { applyStages(c, ST_SPE, -1); log.add(BN_WEB_SLOW, 0); }
  log.targetFainted = c.fainted();
}

// ---------- end of turn ----------

void battleEndTurn(Combatant &c, TurnLog &log) { battleEndTurn(c, nullptr, log); }

void battleEndTurn(Combatant &c, Combatant *other, TurnLog &log) {
  log = TurnLog();
  if (c.fainted()) return;
  BattleSide &s = sideOf(c);
  uint16_t eighth = c.maxHp / 8 ? c.maxHp / 8 : 1, sixteenth = c.maxHp / 16 ? c.maxHp / 16 : 1;
  // weather chip
  if (gField.weather == W_SAND && !isType(c, T_ROCK) && !isType(c, T_GROUND) && !isType(c, T_STEEL)) {
    hurt(c, sixteenth); log.add(BN_SAND_HURT, 0); log.damage += sixteenth;
  }
  if (gField.weather == W_HAIL && !isType(c, T_ICE)) {
    hurt(c, sixteenth); log.add(BN_HAIL_HURT, 0); log.damage += sixteenth;
  }
  // FUTURE SIGHT lands on whoever is out on that side by then
  if (s.futureTurns && --s.futureTurns == 0 && !c.fainted()) {
    hurt(c, s.futureDmg); log.add(BN_FUTURE_HIT, 0); log.damage += s.futureDmg;
  }
  if (s.wishTurns && --s.wishTurns == 0 && !c.fainted() && c.hp < c.maxHp) { heal(c, s.wishHp); log.add(BN_WISH_HEAL, 0); }
  if (gField.terrain == TR_GRASSY && grounded(c) && !c.fainted() && c.hp < c.maxHp) heal(c, sixteenth);
  if ((c.aquaRing || c.ingrained) && !c.fainted() && c.hp < c.maxHp) { heal(c, sixteenth); log.add(BN_RING_HEAL, 0); }
  if (c.fainted()) { log.targetFainted = true; return; }
  if (c.held == IT_LEFTOVERS && c.hp < c.maxHp) {
    heal(c, sixteenth);
    log.atkItem = IT_LEFTOVERS;
    gameEvent(EV_ITEM_USED, IT_LEFTOVERS);
  }
  if (c.seeded) {
    uint16_t d = eighth < c.hp ? eighth : c.hp;
    hurt(c, d);
    if (other && !other->fainted()) heal(*other, d);
    log.add(BN_SEED_DRAIN, 0);
    log.damage += d;
  }
  if (c.ailment == AIL_BURN || c.ailment == AIL_POISON) {
    uint16_t chip = sixteenth;
    if (c.ailment == AIL_POISON && c.toxicN) {        // badly poisoned: one more sixteenth each turn
      chip = (uint16_t)((uint32_t)c.maxHp * c.toxicN / 16);
      if (!chip) chip = 1;
      if (c.toxicN < 15) c.toxicN++;
    }
    hurt(c, chip);
    log.damage += chip;
    log.inflicted = c.ailment;
  }
  if (c.cursed && !c.fainted()) { uint16_t d = c.maxHp / 4 ? c.maxHp / 4 : 1; hurt(c, d); log.add(BN_CURSE_HURT, 0); log.damage += d; }
  if (c.bindTurns && !c.fainted()) {
    hurt(c, eighth); log.add(BN_BIND_HURT, 0); log.damage += eighth;
    c.bindTurns--;
  }
  if (c.yawn && !c.fainted()) {
    if (--c.yawn == 0 && c.ailment == AIL_NONE) {
      TurnLog tmp;
      if (inflictAilment(c, AIL_SLEEP, tmp)) log.add(BN_ASLEEP, 0);
    }
  }
  if (c.perish && !c.fainted()) {
    if (--c.perish == 0) { c.hp = 0; }
    log.add(BN_PERISH, 0);
  }
  if (c.tauntTurns) c.tauntTurns--;
  if (c.encoreTurns && --c.encoreTurns == 0) c.encoreMove = 0;
  if (!c.protecting && !c.enduring) c.protectChain = 0;
  c.flinched = false;
  if (c.turnsOut < 255) c.turnsOut++;
  log.targetFainted = c.fainted();
  checkBerry(c, log.atkItem);
}

void battleEndRound(TurnLog &log) {
  log = TurnLog();
  if (gField.weatherTurns && --gField.weatherTurns == 0) { gField.weather = W_NONE; log.add(BN_WEATHER_END, 0); }
  if (gField.terrainTurns && --gField.terrainTurns == 0) gField.terrain = TR_NONE;
  if (gField.trickRoom) gField.trickRoom--;
  for (int i = 0; i < 2; i++) {
    BattleSide &s = gField.side[i];
    bool had = s.reflect || s.lscreen || s.veil;
    if (s.reflect) s.reflect--;
    if (s.lscreen) s.lscreen--;
    if (s.veil) s.veil--;
    if (had && !(s.reflect || s.lscreen || s.veil)) log.add(BN_SCREEN_END, (uint8_t)i);
    if (s.tailwind) s.tailwind--;
    if (s.safeguard) s.safeguard--;
    if (s.mist) s.mist--;
  }
}

// ---------- move choice ----------

static int32_t statusScore(const Combatant &self, const Combatant &foe, const MoveEntry &m,
                           const uint16_t *legal, uint8_t n) {
  const BattleSide &mine = gField.side[self.side & 1], &theirs = gField.side[foe.side & 1];
  // the best this creature could hit for instead, the yardstick a status move must beat
  uint16_t hit = 0;
  for (uint8_t k = 0; k < n; k++)
    if (MOVE_TBL[legal[k]].cat != MC_STATUS) { uint16_t d = battleDamage(self, foe, legal[k], false, 236); if (d > hit) hit = d; }
  bool finisher = hit >= foe.hp;
  if (finisher) return -40;            // never set up when one more hit wins
  int32_t hpPct = (int32_t)self.hp * 100 / (self.maxHp ? self.maxHp : 1);
  int32_t foePct = (int32_t)foe.hp * 100 / (foe.maxHp ? foe.maxHp : 1);
  switch (m.effect) {
    case EF_HEAL: case EF_HEAL_WEATHER: return (100 - hpPct) - 30;
    case EF_REST: return self.ailment != AIL_NONE ? 40 : (100 - hpPct) - 45;
    case EF_PROTECT: return self.protectChain ? -60 : (foe.seeded || foe.toxicN || foe.cursed || foe.bindTurns ? 25 : 4);
    case EF_ENDURE: return self.protectChain ? -60 : (hpPct < 20 ? 10 : -20);
    case EF_WEATHER: {
      if (gField.weather == (uint8_t)m.param) return -60;
      static const uint8_t LIKES[5] = { 0, T_WATER, T_FIRE, T_ROCK, T_ICE };
      return isType(self, LIKES[m.param]) ? 30 : -15;
    }
    case EF_TERRAIN: {
      static const uint8_t LIKES[5] = { 0, T_ELECTRIC, T_GRASS, T_FAIRY, T_PSYCHIC };
      return gField.terrain == (uint8_t)m.param ? -60 : isType(self, LIKES[m.param]) ? 22 : -15;
    }
    case EF_HAZARD:
      switch (m.param) {
        case 1: return theirs.rocks ? -60 : 28;
        case 2: return theirs.spikes >= 3 ? -60 : 20 - 4 * theirs.spikes;
        case 3: return theirs.tspikes >= 2 ? -60 : 14;
        default: return theirs.web ? -60 : 14;
      }
    case EF_SCREEN: return (m.param == 1 ? mine.reflect : m.param == 2 ? mine.lscreen : (mine.veil || gField.weather != W_HAIL)) ? -60 : 24;
    case EF_SUBSTITUTE: return (self.subHp || hpPct <= 30) ? -60 : 18;
    case EF_LEECHSEED: return (foe.seeded || isType(foe, T_GRASS)) ? -60 : 24;
    case EF_YAWN: return (foe.yawn || foe.ailment != AIL_NONE) ? -60 : 26;
    case EF_TAUNT: return foe.tauntTurns ? -60 : 8;
    case EF_ENCORE: return (foe.encoreTurns || !foe.lastMove) ? -60 : (MOVE_TBL[foe.lastMove].cat == MC_STATUS ? 30 : 4);
    case EF_TRICKROOM: return (effStat(self, SI_SPE) < effStat(foe, SI_SPE)) != (gField.trickRoom != 0) ? 26 : -40;
    case EF_TAILWIND: return mine.tailwind ? -60 : (effStat(self, SI_SPE) <= effStat(foe, SI_SPE) ? 24 : 2);
    case EF_HAZE: return stagesUp(foe) >= 2 ? 30 : -30;
    case EF_PSYCHUP: return stagesUp(foe) - stagesUp(self) >= 2 ? 28 : -30;
    case EF_TOPSYTURVY: return stagesUp(foe) >= 2 ? 28 : -30;
    case EF_BELLYDRUM: return (hpPct > 60 && self.stage[SI_ATK] < 2) ? 34 : -40;
    case EF_SHELLSMASH: return self.stage[SI_ATK] < 2 ? 30 : -30;
    case EF_CURSE: return isType(self, T_GHOST) ? (foe.cursed || hpPct < 60 ? -50 : 20) : (self.stage[SI_ATK] < 2 ? 18 : -20);
    case EF_PAINSPLIT: return foePct - hpPct >= 25 ? 30 : -30;
    case EF_DESTINYBOND: return hpPct < 25 ? 30 : -30;
    case EF_MEMENTO: return hpPct < 20 ? 22 : -60;
    case EF_PERISH: return -20;
    case EF_SWAGGER: return foe.confuseTurns ? -60 : 2;
    case EF_AQUARING: case EF_WISH: return hpPct < 70 ? 10 : -10;
    case EF_SAFEGUARD: return mine.safeguard ? -60 : 6;
    case EF_MIST: return mine.mist ? -60 : 3;
    case EF_FOCUSENERGY: return self.critStage ? -60 : 8;
    case EF_CHARGEUP: return self.chargedUp ? -60 : 8;
    case EF_ACUPRESSURE: return 12;
    case EF_TRAP: return foe.trapped ? -60 : 2;
    case EF_SLEEPTALK: return self.ailment == AIL_SLEEP ? 60 : -100;
    case EF_SPLASH: return -100;
    case EF_FORCESWITCH: return stagesUp(foe) >= 2 || foe.subHp ? 30 : 2;   // undo the foe's set-up
    case EF_BATONPASS: return stagesUp(self) >= 2 ? 30 : -40;               // only worth it with boosts to pass
    case EF_UTURN: return 8;                                                // PARTING SHOT
    case EF_DEFOG: return (mine.rocks || mine.spikes || mine.tspikes || mine.web) ? 26 : -20;
    default: break;
  }
  if (m.effect == EF_STAGE) {
    int stacked = 0, hitn = 0;
    const Combatant &t = (m.target == TG_SELF) ? self : foe;
    for (int k = 0; k < SI_COUNT; k++)
      if (m.statMask & STAGE_BIT[k]) { stacked += t.stage[k] * (m.stages > 0 ? 1 : -1); hitn++; }
    if (m.statMask & ST_ACC) { stacked += t.accStage * (m.stages > 0 ? 1 : -1); hitn++; }
    if (m.statMask & ST_EVA) { stacked += t.evaStage * (m.stages > 0 ? 1 : -1); hitn++; }
    if (!hitn) hitn = 1;
    // diminishing: +2 ATK is strong at stage 0, pointless at +6
    int32_t sc = 26 - (stacked * 12 / hitn);
    if (self.hp * 3 < self.maxHp) sc -= 40;
    return sc;
  }
  if (m.ailment != AIL_NONE) {
    // A status move is worth a turn only if it can take hold: the foe has
    // no status yet (or no confusion, for CONFUSE RAY) and its type is not
    // immune. Sleep is the strongest, then paralysis, then the chip damage
    // ones; all discounted by accuracy.
    bool canTake = m.ailment == AIL_CONFUSE ? !foe.confuseTurns : foe.ailment == AIL_NONE;
    bool typeImmune = (m.ailment == AIL_BURN && hasStab(foe.dex, T_FIRE)) ||
                      (m.ailment == AIL_POISON && (hasStab(foe.dex, T_POISON) || hasStab(foe.dex, T_STEEL))) ||
                      (m.ailment == AIL_PARA && hasStab(foe.dex, T_ELECTRIC)) ||
                      (m.type == T_ELECTRIC && typeEffVsDex(m.type, foe.dex) == 0);
    if (!canTake || typeImmune || foe.subHp || theirs.safeguard) return -50;
    // Worth a share of the foe's HP -- the same units an attack is scored in
    // (its damage), so a status move competes fairly with a strong attack.
    int pct = m.ailment == AIL_SLEEP ? 45 : m.ailment == AIL_PARA ? 35 : m.ailment == AIL_CONFUSE ? 25 : 30;
    int32_t sc = (int32_t)foe.maxHp * pct / 100;
    return sc * (m.acc ? m.acc : 100) / 100;
  }
  return 5;
}

uint16_t aiChooseMove(const Combatant &self, const Combatant &foe, bool smart) {
  uint16_t legal[MOVE_SLOTS];
  uint8_t n = 0;
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (self.moves[i] && self.moves[i] < MOVE_COUNT) legal[n++] = self.moves[i];
  if (!n) return 0;
  if (!smart) return legal[random(n)];

  int32_t bestScore = -2147483647;
  uint16_t best = legal[0];
  for (uint8_t i = 0; i < n; i++) {
    uint16_t mv = legal[i];
    const MoveEntry &m = MOVE_TBL[mv];
    int32_t sc;
    if (self.tauntTurns && m.cat == MC_STATUS) sc = -1000;
    else if (m.cat == MC_STATUS) sc = statusScore(self, foe, m, legal, n);
    else {
      uint16_t dmg = battleDamage(self, foe, mv, false, 236);  // average roll
      if (foe.subHp) dmg = dmg < foe.subHp ? dmg : foe.subHp;
      sc = dmg;
      if (dmg >= foe.hp && !foe.subHp) sc += 1000;  // a kill this turn beats all
      uint8_t acc = m.acc ? m.acc : 100;
      if (m.effect == EF_OHKO) acc = self.level >= foe.level ? 30 + self.level - foe.level : 0;
      sc = sc * acc / 100;                        // discount what tends to miss
      if (m.effect == EF_RECHARGE) sc -= dmg / 4; // a free turn for the foe
      if (m.effect == EF_RECOIL) sc -= dmg / 6;
      if (m.effect == EF_CHARGE) sc -= dmg / 3;   // a turn spent winding up
      if (m.effect == EF_SELFKO || m.effect == EF_FINALGAMBIT) sc -= (int32_t)self.hp * 2;   // costs the creature
      if (m.effect == EF_FOCUSPUNCH) sc /= 3;     // the foe will usually hit first
      if (m.effect == EF_FAKEOUT && self.turnsOut) sc = -500;
      if (m.effect == EF_SNORE && self.ailment != AIL_SLEEP) sc = -500;
      if (m.effect == EF_DREAMEATER && foe.ailment != AIL_SLEEP) sc = -500;
      if (m.effect == EF_COUNTER) sc = -20;       // only after being hit, which it cannot know
      if (m.effect == EF_FUTURE) sc = sc * 2 / 3;
      if (m.effect == EF_LOCKIN) sc -= dmg / 8;
      if (m.ailment != AIL_NONE && foe.ailment == AIL_NONE)
        sc += m.ailChance / 4;
    }
    if (sc > bestScore) { bestScore = sc; best = mv; }
  }
  return best;
}
