#pragma once
#include <stdint.h>

// The CUSTOM trainer (AVATAR_YOU in avatar_names.h) and the wardrobe that
// dresses it, in PokeMMO's own art (pokemmo_art.h, tools/gen_pokemmo_wardrobe.py).
//
// Free from the start: every skin tone, every free hair style in every dye,
// every free eye style, and PokeMMO's own starting clothes. Everything else is a
// COSMETIC, won with an S grade in the arcade (one at most every
// WARDROBE_PRIZE_SECS per game).
//
// An item is stored as its index in PM_ITEM[]. That table is GENERATED, so the
// saved form is the item's PokeMMO id where it must survive a regeneration (the
// owned list) -- see wardrobe.cpp.

enum : uint8_t { WS_HAT = 0, WS_HAIR, WS_EYES, WS_FACE, WS_BACK, WS_TOP, WS_GLOVES, WS_SHOES, WS_LEGS, WS_SLOTS };
#define WARDROBE_NONE 0xFFFF
#define WARDROBE_PRIZE_SECS (4UL * 3600UL)   // per arcade game, between cosmetic prizes
#define WARDROBE_GAMES 6                     // = ARC_GAMES; static_assert'ed in ui_arcade.ino

struct Wardrobe {
  uint8_t skin = 0;                     // 0..PM_SKINS-1
  uint8_t hairDye = 0;                  // 0 = the style's own colour, else a PM_DYE_NAME
  uint16_t worn[WS_SLOTS];              // a PM_ITEM index per slot, or WARDROBE_NONE
};

extern Wardrobe wardrobe;
void wardrobeBegin();
bool wardrobeSave();
void wardrobeClearAll();                      // WIPE, and tests

uint16_t cosCount();                          // PM_ITEMS
const char *cosName(uint16_t item);
bool cosOwned(uint16_t item);
bool cosFree(uint16_t item);
uint16_t cosOwnedCount();                     // won, not counting the free ones
uint16_t cosUnlockableCount();
const char *wardrobeSkinName(uint8_t i);
const char *wardrobeDyeName(uint8_t i);
uint8_t wardrobeDyeCount();
uint8_t wardrobeSkinCount();
bool wardrobeHairDyeable();                   // the worn hair has dye palettes
bool wardrobeSlotMayBeEmpty(uint8_t slot);    // hat, face, back, gloves: yes
// The next / previous OWNED choice for a slot from `cur` (WARDROBE_NONE included
// where the slot may be empty). Wraps.
uint16_t wardrobeStep(uint8_t slot, uint16_t cur, int dir);

// An S grade in arcade game `game` at `nowSecs` (epoch): one random cosmetic not
// yet owned, if the game is off cooldown. Returns its index, or WARDROBE_NONE.
uint16_t wardrobeArcadePrize(uint8_t game, uint32_t nowSecs);
uint32_t wardrobePrizeReadyAt(uint8_t game);  // epoch secs; 0 = ready now

// The composed trainer: PM_CANVAS_W x PM_CANVAS_H, RGB565, `mask` 1 = drawn.
int wardrobeCanvasW();
int wardrobeCanvasH();
void wardrobeCompose(uint16_t *px, uint8_t *mask);
