#pragma once
#include <Arduino.h>

// Creature personalities. Ported from vunguyen2712/TamaPoke (feature 6); the
// rules and numbers are theirs, the storage is rebuilt for this base.
//
// GAME LOGIC ONLY: no gfx, no SD, no touch -- links into every headless test.
//
// Every creature rolls ONE trait when it hatches (or is caught). The trait
// changes which care it enjoys (extra joy/bond and a heart when it gets its
// favourite) and nudges its awake needs a little. Two functions are the whole
// rule, and every care path in pet.cpp asks them rather than keeping its own copy:
//
//   personaCare(trait, care, baseJoy)          -- one act of care
//   personaDrain(trait, minute, poops, asleep) -- one minute's extra drain
//
// Overnight safety (CLAUDE.md "The three drain paths"): personaDrain() changes
// NOTHING while asleep except a faster energy refill for SLEEPY, and the offline
// catch-up applies no awake nudge at all. No trait can make a night -- screen
// off and asleep, or the board switched off -- any harder to survive.
//
// Storage (this base, not the feature fork's):
//   the live pet  -> the pet checkpoint's reserved tail byte (PET_TAIL_RSVD),
//                    which v3.28 always wrote as 0, so it commits atomically
//                    with everything else; plus legacy key "prsn"
//   a banked one  -> PartyMon::persona, in what was padding after `held`
// 0 is "no trait": an egg, or a creature from before this feature. A live
// creature with none rolls one once on load (Pet::begin()).
//
// THE TRAIT IS A NATURE (v3.36): the 25 of the main series, with their real
// names and their real +10% / -10% stats, so nobody has to learn a second
// system. Stored codes:
//   0         none
//   1..7      a LEGACY trait from v3.33-3.35 (GLUTTON..PICKY). Never written
//             any more, but every save and banked creature may hold one, and
//             each reads as the one nature with EXACTLY its old boon and bane
//             (GLUTTON = RELAXED, ...), so no creature's stats move.
//   8..32     nature n as PT_NATURE0 + n, n in the games' own order (HARDY 0
//             ... QUIRKY 24)
// Every function below takes a stored code and asks personaNature() first, so
// both kinds behave identically and nothing needs migrating.
//
// The care side (what it loves) stays: each nature loves one of the seven
// kinds of care, mostly by the stat it raises (training for +ATK, play for
// +SPE...). The legacy seven keep exactly the like they had.
enum : uint8_t {
  PT_NONE = 0,
  PT_GLUTTON,     // legacy: RELAXED
  PT_SLEEPY,      // legacy: SASSY
  PT_PLAYFUL,     // legacy: HASTY
  PT_TIDY,        // legacy: MODEST
  PT_CUDDLY,      // legacy: IMPISH
  PT_ENERGETIC,   // legacy: NAUGHTY
  PT_PICKY,       // legacy: MILD
  PT_NATURE0,     // the first nature code
};
#define NATURE_COUNT 25
#define PT_END (PT_NATURE0 + NATURE_COUNT)   // one past the last valid code
enum : uint8_t {
  NAT_HARDY = 0, NAT_LONELY, NAT_BRAVE, NAT_ADAMANT, NAT_NAUGHTY,
  NAT_BOLD, NAT_DOCILE, NAT_RELAXED, NAT_IMPISH, NAT_LAX,
  NAT_TIMID, NAT_HASTY, NAT_SERIOUS, NAT_JOLLY, NAT_NAIVE,
  NAT_MODEST, NAT_MILD, NAT_QUIET, NAT_BASHFUL, NAT_RASH,
  NAT_CALM, NAT_GENTLE, NAT_SASSY, NAT_CAREFUL, NAT_QUIRKY,
  NAT_NONE = 0xFF,
};
// What a nature loves -- the old seven traits' care rules, by name.
enum : uint8_t { PL_NONE = 0, PL_FOOD, PL_BED, PL_PLAY, PL_BATH, PL_CUDDLE, PL_TRAIN, PL_BERRY, PL_COUNT };

// The acts of care a trait can react to.
enum : uint8_t {
  PC_BERRY = 0,   // a berry it does NOT love
  PC_FAV_BERRY,   // its favourite flavour
  PC_CANDY,
  PC_PLAY,        // the ball game (playResult) and play()
  PC_TRAIN,       // the training bag and the reaction test
  PC_BATH,
  PC_CARESS,
  PC_LIGHTS_OFF,  // put to bed with the light button
  PC_COUNT
};

struct PersonaEffect {
  int8_t joy = 0;     // added to the action's own joy
  int8_t food = 0;    // added to the action's own fullness
  uint8_t bond = 0;   // added bond (still under addBond()'s daily cap)
  bool fav = false;   // its favourite: the creature cheers
};

// Extra drain for ONE minute, on top of the normal one. Positive drains more,
// negative drains less (or, for energy while asleep, refills more).
struct PersonaDrain {
  int8_t food = 0;
  int8_t joy = 0;
  int8_t energy = 0;
};

// The extra drain lands on every PERSONA_NUDGE_EVERY-th minute: 1/4 point a
// minute, which is the "a little" in "nudges its needs a little".
#define PERSONA_NUDGE_EVERY 4

PersonaEffect personaCare(uint8_t trait, uint8_t care, uint8_t baseJoy);
PersonaDrain personaDrain(uint8_t trait, uint32_t minute, uint8_t poops, bool asleep);

bool personaValid(uint8_t trait);        // a nature code or a legacy trait
uint8_t personaRoll();                   // a fresh nature code, uniform over the 25
uint8_t personaNature(uint8_t trait);    // NAT_*, NAT_NONE for none
uint8_t personaCode(uint8_t nature);     // the stored code for NAT_n
uint8_t personaLike(uint8_t trait);      // PL_*: the care it loves
const char *personaName(uint8_t trait);  // the nature's name, current language ("" for none)
const char *personaLoves(uint8_t trait); // what it loves, short ("" for none)

// A nature raises one battle stat by 10% and lowers another by 10%, exactly
// as in the main series; HP is never touched, and the five neutral natures
// (HARDY, DOCILE, SERIOUS, BASHFUL, QUIRKY) change nothing. THE rule:
// Pet's and Party's stat accessors both apply personaStat(), so the STATS
// page, the card and every battle agree. A creature with no trait (an egg, or
// one banked before personalities) is neutral.
enum : uint8_t { PS_ATK = 0, PS_DEF, PS_SPA, PS_SPD, PS_SPE, PS_COUNT, PS_NONE = 0xFF };
uint8_t personaBoon(uint8_t trait);      // PS_*, PS_NONE for none or a neutral nature
uint8_t personaBane(uint8_t trait);
uint16_t personaStat(uint8_t trait, uint8_t stat, uint16_t value);   // the value after the trait
