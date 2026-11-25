#pragma once
#include <stdint.h>
#include <stdio.h>

// Alternate skins: a creature that LOOKS different and is otherwise the same
// species. Today there is one, the armored MEWTWO (a Game Corner prize).
//
// A skin is cosmetic and travels with the individual creature: the live pet
// (Pet::skin, pet checkpoint v3 tail + legacy key "skin"), a banked or boxed
// one (PartyMon::skinTag) and a trade (the PartyMon goes over the wire as is).
// Its art is its own sprite file on the SD -- /mons/p<tag><dex>.bin, e.g.
// pa150.bin -- and a missing file simply draws the ordinary species.
//
// skinFor() is THE rule for "does this creature wear this skin": every reader
// (the sprite loaders, the battle, the stats page) asks it, so a skin can never
// be drawn on a species it was not made for, whatever a record or the wire says.
enum : uint8_t { SKIN_NONE = 0, SKIN_ARMORED = 1, SKIN_COUNT };

inline uint8_t skinFor(int16_t dex, uint8_t skin) {
  switch (skin) {
    case SKIN_ARMORED: return dex == 150 ? SKIN_ARMORED : SKIN_NONE;
    default: return SKIN_NONE;
  }
}

// The letter in the sprite file name ('a' -> pa150.bin), 0 for none.
inline char skinFileTag(uint8_t skin) { return skin == SKIN_ARMORED ? 'a' : 0; }

// PartyMon stores a skin in what used to be padding, so it is kept as a MAGIC
// value (PM_SKIN_TAG | skin): stale padding in an older record must not read as
// a skin. Same reasoning as PM_EGG_PAID and PM_FAV.
#define PM_SKIN_TAG 0xA0

// THE sprite file name, for the board's loader and the emulator's alike:
// "/p150.bin", "/ps150.bin" (shiny) or "/pa150.bin" (a skin). A skin is tried
// first, then shiny, then plain -- see PmdMon::load().
inline void sdSpriteName(char *out, size_t n, int16_t dex, bool shiny, uint8_t skin) {
  char tag = skinFileTag(skinFor(dex, skin));
  if (tag) snprintf(out, n, "/p%c%03u.bin", tag, (unsigned)dex);
  else snprintf(out, n, "/p%s%03u.bin", shiny ? "s" : "", (unsigned)dex);
}
