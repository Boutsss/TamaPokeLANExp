#pragma once
#include <stdint.h>

// The trainer card's six accomplishment stars. Each is LATCHED: once earned it
// stays, even if what earned it is later spent (the million coins) or broken
// (a tower run ends). Stored under its own NVS key, "stars", which is in
// SAVE_FIELDS so a backup carries it.
//
// The order is the order they are drawn, and each bit is FROZEN once shipped
// (stored raw) -- a new star is APPENDED.
enum : uint8_t {
  STAR_TOWER = 0,   // red: 100 Battle Tower floors in a row
  STAR_DEX,         // blue: every region's obtainable Pokedex complete
  STAR_LADDER_EASY, // silver: every region's ladder (gyms, Elite 4, champion) on easy
  STAR_LADDER_HARD, // gold: the same on hard
  STAR_LAN,         // purple: 100 LAN battle victories
  STAR_COINS,       // black: a million Game Corner coins held at once
  STAR_SHINY,       // rainbow (v3.57): 10 shiny creatures caught in the wild
  STAR_COUNT
};
static_assert(STAR_COUNT <= 8, "stars are one byte: on flash and on a trainer card");
#define STARS_MASK ((uint8_t)((1u << STAR_COUNT) - 1))

#define STAR_TOWER_FLOORS 100
#define STAR_LAN_WINS 100
#define STAR_COINS_HELD 1000000UL
#define STAR_SHINY_CATCHES 10

class Pet;
void starsBegin(const Pet *p);          // after rank/tower/casino begin
uint8_t starsEarned();                  // bit i = star i
bool starHas(uint8_t i);
uint16_t starsLanWins();                // lifetime LAN wins (rivals[] only keeps 10)
uint16_t starsShinyCatches();           // lifetime shiny wild catches, from v3.57 on
// Re-checks every condition and latches any newly met. Not free (the Pokedex
// check walks every species), so callers throttle it -- the card asks on open.
void starsUpdate();
void starsClearAll();                   // WIPE, and tests
// Unlatch every star, then latch only what the save qualifies for NOW. The
// console's STARS RECHECK -- an undo for a star a debug command set by mistake.
void starsRecheck();
