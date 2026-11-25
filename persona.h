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
enum : uint8_t {
  PT_NONE = 0,
  PT_GLUTTON,     // loves food
  PT_SLEEPY,      // loves being put to bed
  PT_PLAYFUL,     // loves the ball game
  PT_TIDY,        // loves a bath, hates poop
  PT_CUDDLY,      // loves being petted
  PT_ENERGETIC,   // loves training
  PT_PICKY,       // loves its favourite berry, sniffs at the others
  PT_COUNT
};

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

bool personaValid(uint8_t trait);        // 1..PT_COUNT-1
uint8_t personaRoll();                   // a fresh trait, uniform over the seven
const char *personaName(uint8_t trait);  // in the current language ("" for none)
const char *personaLoves(uint8_t trait); // what it loves, short ("" for none)

// Each personality also raises one battle stat by 10% and lowers another by
// 10% (v3.33), like the main series' natures; HP is never touched. THE rule:
// Pet's and Party's stat accessors both apply personaStat(), so the STATS
// page, the card and every battle agree. A creature with no trait (an egg, or
// one banked before personalities) is neutral.
enum : uint8_t { PS_ATK = 0, PS_DEF, PS_SPA, PS_SPD, PS_SPE, PS_COUNT, PS_NONE = 0xFF };
uint8_t personaBoon(uint8_t trait);      // PS_*, PS_NONE for no trait
uint8_t personaBane(uint8_t trait);
uint16_t personaStat(uint8_t trait, uint8_t stat, uint16_t value);   // the value after the trait
