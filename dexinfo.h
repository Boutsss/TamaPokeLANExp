#pragma once
#include <stdint.h>
#include <stddef.h>

// Pokedex entries (v3.42): each species' category, height and description, in
// the six UI languages, from /mons/dexinfo.bin on the SD card -- 850 KB that has
// no business in flash. Format: tools/gen_dexinfo.py.
//
// ONE parser for the board and the emulator: each supplies only "read n bytes
// at this offset" (sdmon.cpp over SD_MMC, host_impl.cpp over a FILE*), so the
// two cannot read the format differently. GAME LOGIC ONLY: no gfx, no SD.

#define DEXINFO_GENUS 25
#define DEXINFO_TEXT 241

struct DexEntryText {
  char genus[DEXINFO_GENUS] = "";   // "Seed Pokemon"
  char text[DEXINFO_TEXT] = "";
  uint16_t heightDm = 0;            // decimetres; the weight is DEX_WEIGHT (weights.h)
};

// Returns how many bytes it could read at `off` into `dst` (at most n).
typedef uint32_t (*DexReadFn)(void *ctx, uint32_t off, void *dst, uint32_t n);

// False if the file is missing, damaged, or has no such species/language --
// the screen then simply shows no entry.
bool dexInfoRead(DexReadFn rd, void *ctx, int16_t dex, uint8_t lang, DexEntryText &out);
