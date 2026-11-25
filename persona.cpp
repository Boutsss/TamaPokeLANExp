#include "persona.h"
#include "i18n.h"

// The numbers live here and nowhere else. README "Personalities" quotes them;
// persona_test measures each one through the real Pet methods.
#define PJ_FOOD     6   // GLUTTON: extra joy from any food
#define PJ_BED      8   // SLEEPY: joy from being put to bed by hand
#define PE_BED      2   // SLEEPY: extra energy per minute asleep (+6 -> +8)
#define PJ_BATH    10   // TIDY: joy from a bath
#define PB_BATH     2   // TIDY: bond from a bath
#define PJ_PET      5   // CUDDLY: extra joy from a caress (+5 -> +10)
#define PB_PET      1   // CUDDLY: extra bond from a caress (+1 -> +2)
#define PJ_TRAIN    8   // ENERGETIC: extra joy from a training session
#define PB_TRAIN    1
#define PJ_FAVB    10   // PICKY: extra joy from its favourite berry (+10 -> +20)
#define PB_FAVB     2
#define PF_PLAIN   -5   // PICKY: food from any other berry (+25 -> +20)

// The legacy seven, each as the nature with exactly its old boon and bane.
static const uint8_t LEGACY_NATURE[PT_NATURE0] = {
  NAT_NONE, NAT_RELAXED, NAT_SASSY, NAT_HASTY, NAT_MODEST, NAT_IMPISH, NAT_NAUGHTY, NAT_MILD };

uint8_t personaNature(uint8_t t) {
  if (t >= PT_NATURE0 && t < PT_END) return (uint8_t)(t - PT_NATURE0);
  if (t > PT_NONE && t < PT_NATURE0) return LEGACY_NATURE[t];
  return NAT_NONE;
}
uint8_t personaCode(uint8_t n) { return n < NATURE_COUNT ? (uint8_t)(PT_NATURE0 + n) : PT_NONE; }
bool personaValid(uint8_t t) { return personaNature(t) != NAT_NONE; }
uint8_t personaRoll() { return personaCode((uint8_t)random(NATURE_COUNT)); }

// What each nature loves. Mostly by the stat it raises -- +ATK training,
// +SPE play, +SP.D bedtime -- and the seven legacy natures keep the like their
// old trait had (RELAXED food, IMPISH cuddles, MODEST baths, MILD its berry...).
static const uint8_t LIKE[NATURE_COUNT] = {
  /* HARDY   */ PL_TRAIN,  /* LONELY  */ PL_TRAIN, /* BRAVE  */ PL_TRAIN,  /* ADAMANT */ PL_TRAIN,  /* NAUGHTY */ PL_TRAIN,
  /* BOLD    */ PL_CUDDLE, /* DOCILE  */ PL_CUDDLE, /* RELAXED */ PL_FOOD, /* IMPISH  */ PL_CUDDLE, /* LAX     */ PL_FOOD,
  /* TIMID   */ PL_PLAY,   /* HASTY   */ PL_PLAY,  /* SERIOUS */ PL_TRAIN, /* JOLLY   */ PL_PLAY,   /* NAIVE   */ PL_PLAY,
  /* MODEST  */ PL_BATH,   /* MILD    */ PL_BERRY, /* QUIET  */ PL_BATH,   /* BASHFUL */ PL_BED,    /* RASH    */ PL_BERRY,
  /* CALM    */ PL_BED,    /* GENTLE  */ PL_CUDDLE, /* SASSY  */ PL_BED,   /* CAREFUL */ PL_BERRY,  /* QUIRKY  */ PL_BERRY,
};
uint8_t personaLike(uint8_t t) { uint8_t n = personaNature(t); return n == NAT_NONE ? PL_NONE : LIKE[n]; }

// Names live in the ONE positional string table (i18n.cpp), in nature order,
// so adding a language row cannot silently miss them.
const char *personaName(uint8_t t) {
  uint8_t n = personaNature(t);
  return n == NAT_NONE ? "" : T((StrId)(S_NAT_HARDY + n));
}
const char *personaLoves(uint8_t t) {
  uint8_t l = personaLike(t);
  return l == PL_NONE ? "" : T((StrId)(S_PTL_GLUTTON + (l - 1)));
}

// The games' own table: nature n raises stat n / 5 and lowers stat n % 5, in
// the order ATK, DEF, SPE, SP.A, SP.D. Equal = neutral.
static const uint8_t GAME_ORDER[5] = { PS_ATK, PS_DEF, PS_SPE, PS_SPA, PS_SPD };
uint8_t personaBoon(uint8_t t) {
  uint8_t n = personaNature(t);
  if (n == NAT_NONE || n / 5 == n % 5) return PS_NONE;
  return GAME_ORDER[n / 5];
}
uint8_t personaBane(uint8_t t) {
  uint8_t n = personaNature(t);
  if (n == NAT_NONE || n / 5 == n % 5) return PS_NONE;
  return GAME_ORDER[n % 5];
}
uint16_t personaStat(uint8_t t, uint8_t stat, uint16_t v) {
  if (stat == PS_NONE) return v;
  if (stat == personaBoon(t)) return (uint16_t)((uint32_t)v * 110 / 100);
  if (stat == personaBane(t)) return (uint16_t)((uint32_t)v * 90 / 100);
  return v;
}

PersonaEffect personaCare(uint8_t trait, uint8_t care, uint8_t baseJoy) {
  PersonaEffect e;
  switch (personaLike(trait)) {
    case PL_FOOD:
      if (care == PC_BERRY || care == PC_FAV_BERRY || care == PC_CANDY) { e.joy = PJ_FOOD; e.fav = true; }
      break;
    case PL_BED:
      if (care == PC_LIGHTS_OFF) { e.joy = PJ_BED; e.fav = true; }
      break;
    case PL_PLAY:
      // double the session's own joy, whatever it was worth
      if (care == PC_PLAY) { e.joy = (int8_t)(baseJoy > 100 ? 100 : baseJoy); e.fav = true; }
      break;
    case PL_BATH:
      if (care == PC_BATH) { e.joy = PJ_BATH; e.bond = PB_BATH; e.fav = true; }
      break;
    case PL_CUDDLE:
      if (care == PC_CARESS) { e.joy = PJ_PET; e.bond = PB_PET; e.fav = true; }
      break;
    case PL_TRAIN:
      if (care == PC_TRAIN) { e.joy = PJ_TRAIN; e.bond = PB_TRAIN; e.fav = true; }
      break;
    case PL_BERRY:
      if (care == PC_FAV_BERRY) { e.joy = PJ_FAVB; e.bond = PB_FAVB; e.fav = true; }
      else if (care == PC_BERRY) e.food = PF_PLAIN;
      break;
    default: break;
  }
  return e;
}

PersonaDrain personaDrain(uint8_t trait, uint32_t minute, uint8_t poops, bool asleep) {
  PersonaDrain d;
  if (asleep) {
    // The night is never harder: the only thing a trait does asleep is help.
    if (personaLike(trait) == PL_BED) d.energy = -PE_BED;
    return d;
  }
  bool nudge = (minute % PERSONA_NUDGE_EVERY) == 0;
  switch (personaLike(trait)) {
    case PL_FOOD:   if (nudge) d.food = 1; break;               // -2 -> -2.25/min
    case PL_BED:    if (nudge) d.energy = 1; break;             // -1 -> -1.25/min
    case PL_PLAY:   if (nudge) d.joy = 1; break;                // -1 -> -1.25/min
    case PL_BATH:   if (poops) d.joy = 1; break;                // -1 more a minute with poop out
    case PL_TRAIN:  if (nudge) { d.food = 1; d.energy = -1; } break;  // eats more, tires less
    default: break;   // CUDDLE and BERRY: no change to the drain
  }
  return d;
}

// The names are read by offset from S_NAT_HARDY / S_PTL_GLUTTON, so the string
// table must hold exactly one entry per nature and per like, in order.
static_assert(S_NAT_QUIRKY - S_NAT_HARDY == NAT_QUIRKY - NAT_HARDY, "nature names out of step with NAT_");
static_assert(S_PTL_PICKY - S_PTL_GLUTTON == PL_BERRY - PL_FOOD, "likes out of step with PL_");
