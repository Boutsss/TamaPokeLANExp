#include "held_items.h"
#include "pet.h"
#include "inventory.h"
#include "battle.h"

static PartyMon *banked(Party &pt, uint16_t who) {
  if (who >= HOLDER_PARTY(0) && who < HOLDER_BOX(0))
    return &pt.slots[who - HOLDER_PARTY(0)];
  if (who >= HOLDER_BOX(0) && who < HOLDER_COUNT)
    return &pt.box[who - HOLDER_BOX(0)];
  return nullptr;
}
static const PartyMon *banked(const Party &pt, uint16_t who) {
  if (who >= HOLDER_PARTY(0) && who < HOLDER_BOX(0))
    return &pt.slots[who - HOLDER_PARTY(0)];
  if (who >= HOLDER_BOX(0) && who < HOLDER_COUNT)
    return &pt.box[who - HOLDER_BOX(0)];
  return nullptr;
}

bool holderExists(const Pet &p, const Party &pt, uint16_t who) {
  if (who == HOLDER_PET) return !p.isEgg();
  const PartyMon *m = banked(pt, who);
  return m && !m->empty();
}
ItemKey heldBy(const Pet &p, const Party &pt, uint16_t who) {
  if (!holderExists(p, pt, who)) return ITEM_NONE;
  if (who == HOLDER_PET) return itemIsHeld(p.held) ? p.held : ITEM_NONE;
  ItemKey item = banked(pt, who)->held;
  return itemIsHeld(item) ? item : ITEM_NONE;
}

static ItemKey *slot(Pet &p, Party &pt, uint16_t who) {
  if (!holderExists(p, pt, who)) return nullptr;
  if (who == HOLDER_PET) return &p.held;
  PartyMon *m = banked(pt, who);
  return m ? &m->held : nullptr;
}
static bool saveHolder(Pet &p, Party &pt, uint16_t who) {
  if (who == HOLDER_PET) return p.saveNow();
  if (who < HOLDER_BOX(0)) return pt.save();
  return pt.boxSave();
}

bool heldGive(Pet &p, Party &pt, uint16_t who, ItemKey item) {
  ItemKey *h = slot(p, pt, who);
  if (!h || !itemIsHeld(item) || !bag.count(item)) return false;
  if (*h == item) return true;
  ItemKey old = *h;
  if (!bag.consume(item, 1)) return false;
  if (old && !bag.addChecked(old, 1)) {
    bag.addChecked(item, 1);
    return false;
  }
  *h = item;
  if (saveHolder(p, pt, who)) return true;
  *h = old;
  if (old) bag.consume(old, 1);
  bag.addChecked(item, 1);
  saveHolder(p, pt, who);
  return false;
}

bool heldTake(Pet &p, Party &pt, uint16_t who) {
  ItemKey *h = slot(p, pt, who);
  if (!h || !itemIsHeld(*h)) return false;
  ItemKey old = *h;
  if (!bag.addChecked(old, 1)) return false;
  *h = ITEM_NONE;
  if (saveHolder(p, pt, who)) return true;
  *h = old;
  bag.consume(old, 1);
  saveHolder(p, pt, who);
  return false;
}

void heldReturn(ItemKey &held) {
  if (itemIsHeld(held)) bag.addChecked(held, 1);
  held = ITEM_NONE;
}

// (v3.38) There is no write-back from a battle any more. A berry, a FOCUS SASH
// and the rest work ONCE PER BATTLE -- Combatant::itemUsed, a battle-only copy --
// and the creature still holds them when the fight ends. Nothing in battle.cpp
// touches the stored holder; held_items_test proves the item is back.

int shinyCharmOdds(int base) {
  if (bag.has(IT_SHINY_CHARM)) {
    base /= 2;
    if (base < 4) base = 4;
  }
  return base < 1 ? 1 : base;
}

// ---------------------------------------------------------------- descriptions
#include "types.h"
static const char *const STAT_NM[5] = { "ATK", "DEF", "SP.ATK", "SP.DEF", "SPEED" };
static const char *statNm(uint8_t i) { return i < 5 ? STAT_NM[i] : "?"; }
static const char *ailNm(uint8_t a) {
  switch (a) {
    case AIL_PARA: return "paralysis"; case AIL_BURN: return "a burn"; case AIL_POISON: return "poison";
    case AIL_SLEEP: return "sleep"; case AIL_FREEZE: return "a freeze"; case AIL_CONFUSE: return "confusion";
    default: return "an ailment";
  }
}
static const char *weatherNm(uint8_t w) {
  switch (w) { case W_RAIN: return "Rain"; case W_SUN: return "Sun"; case W_SAND: return "Sandstorm"; case W_HAIL: return "Hail"; default: return "Weather"; }
}
static const char *terrainNm(uint8_t t) {
  switch (t) { case TR_ELECTRIC: return "Electric"; case TR_GRASSY: return "Grassy"; case TR_MISTY: return "Misty"; case TR_PSYCHIC: return "Psychic"; default: return "Any"; }
}
void itemDescribe(ItemKey k, char *o, size_t n) {
  if (!n) return;
  o[0] = 0;
  if (!itemValid(k)) return;
  const ItemEntry &e = itemEntry(k);
  switch (e.category) {
    case IC_BALL:
      if (e.param == ITEM_CATCH_GUARANTEED) snprintf(o, n, "Catches any wild Pokemon, every time");
      else snprintf(o, n, "Catch rate x%d.%d", e.param / 100, (e.param % 100) / 10);
      return;
    case IC_HEAL: snprintf(o, n, "Restores %d HP in battle", e.param); return;
    case IC_CURE: snprintf(o, n, "Cures any ailment in battle"); return;
    case IC_TRAIN: snprintf(o, n, "+%u %s training, out of battle", e.amount, e.param == TRAIN_ATK ? "ATK" : e.param == TRAIN_DEF ? "DEF" : "SPEED"); return;
    case IC_KEY:
      if (k == IT_SHINY_CHARM) snprintf(o, n, "Shinies more likely, in eggs and the wild");
      else if (k == IT_OVAL_CHARM) snprintf(o, n, "Daycare eggs inherit every IV");
      return;
    default: break;
  }
  const char *T1 = typeName(e.fa);
  switch (e.fx) {
    case HX_HEAL_FLAT: snprintf(o, n, "At half HP: heals %d HP", e.param); break;
    case HX_HEAL_PCT: snprintf(o, n, "At half HP: heals %d%% of its HP", e.param); break;
    case HX_PINCH_HEAL: snprintf(o, n, "At 1/4 HP: heals %d%% of its HP", e.param); break;
    case HX_LUM: snprintf(o, n, "Cures any ailment or confusion, once"); break;
    case HX_CURE: snprintf(o, n, "Cures %s, once", ailNm(e.fa)); break;
    case HX_LEFTOVERS: snprintf(o, n, "Heals 1/16 of its HP every turn"); break;
    case HX_SASH: snprintf(o, n, "From full HP, survives a KO hit"); break;
    case HX_QUICK_CLAW: snprintf(o, n, "%d%% chance to move first", e.param); break;
    case HX_TYPE_BOOST: snprintf(o, n, "%s moves x1.2", T1); break;
    case HX_PLATE: snprintf(o, n, "%s moves x1.2; ARCEUS turns %s", T1, T1); break;
    case HX_GEM: snprintf(o, n, "One %s move x1.3, then it is used", T1); break;
    case HX_RESIST:
      if (e.fa == T_NORMAL) snprintf(o, n, "Halves one NORMAL hit, once");
      else snprintf(o, n, "Halves a super-effective %s hit", T1);
      break;
    case HX_ENIGMA: snprintf(o, n, "Super-effective hit taken: heals %d%%", e.param); break;
    case HX_PINCH_STAT: snprintf(o, n, "At 1/4 HP: +1 %s", statNm(e.fa)); break;
    case HX_LANSAT: snprintf(o, n, "At 1/4 HP: critical hits more likely"); break;
    case HX_STARF: snprintf(o, n, "At 1/4 HP: +2 to a random stat"); break;
    case HX_MICLE: snprintf(o, n, "At 1/4 HP: next move more accurate"); break;
    case HX_CUSTAP: snprintf(o, n, "At 1/4 HP: moves first next turn"); break;
    case HX_HIT_STAT: snprintf(o, n, "%s hit taken: +1 %s, once", e.fa == MC_PHYS ? "Physical" : "Special", statNm(e.fb)); break;
    case HX_JABOCA: snprintf(o, n, "%s attacker loses 1/8 of its HP", e.fa == MC_PHYS ? "Physical" : "Special"); break;
    case HX_CHOICE: snprintf(o, n, "%s x1.5, but locked into one move", statNm(e.fa)); break;
    case HX_LIFE_ORB: snprintf(o, n, "Moves x1.3; costs 1/10 HP a hit"); break;
    case HX_ASSAULT_VEST: snprintf(o, n, "SP.DEF x1.5; attacking moves only"); break;
    case HX_ROCKY_HELMET: snprintf(o, n, "Physical attackers lose 1/6 of their HP"); break;
    case HX_EXPERT_BELT: snprintf(o, n, "Super-effective moves x1.2"); break;
    case HX_EVIOLITE: snprintf(o, n, "DEF and SP.DEF x1.5 if it can evolve"); break;
    case HX_BLACK_SLUDGE: snprintf(o, n, "Heals POISON types; hurts the rest"); break;
    case HX_WEAKNESS_POLICY: snprintf(o, n, "Hit super-effectively: +2 ATK, SP.ATK"); break;
    case HX_AIR_BALLOON: snprintf(o, n, "Immune to GROUND until it is hit"); break;
    case HX_RED_CARD: snprintf(o, n, "When hit, the attacker is switched out"); break;
    case HX_LIGHT_CLAY: snprintf(o, n, "Reflect and Light Screen last 8 turns"); break;
    case HX_METRONOME: snprintf(o, n, "The same move in a row: up to x2"); break;
    case HX_TOXIC_ORB: snprintf(o, n, "Badly poisons the holder"); break;
    case HX_SHED_SHELL: snprintf(o, n, "Can always switch out"); break;
    case HX_MENTAL_HERB: snprintf(o, n, "Ends Taunt or Encore, once"); break;
    case HX_WEATHER_ROCK: snprintf(o, n, "%s it starts lasts 8 turns", weatherNm(e.fa)); break;
    case HX_SEED: snprintf(o, n, "%s Terrain: +1 %s, once", terrainNm(e.fa), statNm(e.fb)); break;
    case HX_SPECIES: {
      static const char *const W[6] = { "PIKACHU: ATK and SP.ATK x2", "CUBONE, MAROWAK: ATK x2", "CLAMPERL: SP.ATK x2",
                                         "CLAMPERL: SP.DEF x2", "DITTO: DEF x2", "DITTO: SPEED x2" };
      snprintf(o, n, "%s", e.fa < 6 ? W[e.fa] : "");
      break;
    }
    case HX_SPECIES_BOOST: {
      static const char *const W[3] = { "LATIAS, LATIOS: PSYCHIC, DRAGON x1.2", "DIALGA: STEEL, DRAGON x1.2", "PALKIA: WATER, DRAGON x1.2" };
      snprintf(o, n, "%s", e.fa < 3 ? W[e.fa] : "");
      break;
    }
    case HX_TERRAIN_EXT: snprintf(o, n, "Terrains it starts last 8 turns"); break;
    case HX_EJECT_BUTTON: snprintf(o, n, "When hit, the holder switches out"); break;
    case HX_FLAME_ORB: snprintf(o, n, "Burns the holder"); break;
    case HX_WHITE_HERB: snprintf(o, n, "Undoes lowered stats, once"); break;
    case HX_POWER_HERB: snprintf(o, n, "Skips one charging turn"); break;
    case HX_MUSCLE_BAND: snprintf(o, n, "Physical moves x1.1"); break;
    case HX_WISE_GLASSES: snprintf(o, n, "Special moves x1.1"); break;
    case HX_CRIT: snprintf(o, n, "Critical hits more likely"); break;
    case HX_FLINCH: snprintf(o, n, "10%% chance to make the foe flinch"); break;
    case HX_WIDE_LENS: snprintf(o, n, "Accuracy x1.1"); break;
    case HX_ZOOM_LENS: snprintf(o, n, "Accuracy x1.2 when moving second"); break;
    case HX_BRIGHT_POWDER: snprintf(o, n, "The foe's accuracy x0.9"); break;
    case HX_FOCUS_BAND: snprintf(o, n, "10%% chance to survive a KO hit"); break;
    case HX_SHELL_BELL: snprintf(o, n, "Heals 1/8 of the damage it deals"); break;
    case HX_BIG_ROOT: snprintf(o, n, "Draining moves heal 30%% more"); break;
    case HX_SAFETY_GOGGLES: snprintf(o, n, "No sandstorm or hail damage"); break;
    case HX_PROTECTIVE_PADS: snprintf(o, n, "No Rocky Helmet damage when attacking"); break;
    case HX_ABSORB: snprintf(o, n, "Hit by %s: +1 %s, once", T1, statNm(e.fb)); break;
    case HX_BINDING_BAND: snprintf(o, n, "Binding moves deal 1/6 a turn"); break;
    case HX_GRIP_CLAW: snprintf(o, n, "Binding moves last 7 turns"); break;
    case HX_SPECIES_CRIT: snprintf(o, n, "%s", e.fa == 0 ? "FARFETCHD, SIRFETCHD: big crit chance" : "CHANSEY: big crit chance"); break;
    case HX_IRON_BALL: snprintf(o, n, "Halves SPEED; GROUND moves hit it"); break;
    case HX_LAGGING: snprintf(o, n, "Always moves last"); break;
    case HX_STICKY_BARB: snprintf(o, n, "Hurts the holder 1/8 every turn"); break;
    case HX_FLOAT_STONE: snprintf(o, n, "Halves the holder's weight"); break;
    case HX_RING_TARGET: snprintf(o, n, "The holder loses its immunities"); break;
    default: break;
  }
}
