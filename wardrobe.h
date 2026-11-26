#pragma once
#include <stdint.h>

// The customisable trainer avatar ("YOU", AVATAR_YOU in avatar_names.h) and the
// wardrobe that dresses it. Skin, eyes, hair style and hair colour are all free
// from the start; clothes come as COSMETICS, a few owned from the start and the
// rest won from the arcade (an S grade, at most once every WARDROBE_PRIZE_SECS
// per game).
//
// COSMETIC IDS ARE STORED RAW (the unlock bitmask and the worn item per slot),
// so the COS[] table in wardrobe.cpp is APPEND-ONLY, like the move table.

enum : uint8_t { WS_HAT = 0, WS_TOP, WS_LEGS, WS_SHOES, WS_FACE, WS_BACK, WS_SLOTS };

#define WARDROBE_SKINS 6
#define WARDROBE_EYES 6
#define WARDROBE_HAIRS 12        // styles, BALD included
#define WARDROBE_HAIR_COLS 12
#define WARDROBE_PRIZE_SECS (4UL * 3600UL)   // per arcade game, between cosmetic prizes
#define WARDROBE_GAMES 6         // = ARC_GAMES; static_assert'ed in ui_arcade.ino

struct Wardrobe {
  uint8_t skin = 2, eyes = 0, hair = 1, hairCol = 2;   // the look a new save starts with
  uint8_t worn[WS_SLOTS] = { 0, 0, 0, 0, 0, 0 };       // a COS id per slot (filled by wardrobeBegin)
};

uint8_t cosCount();
const char *cosName(uint8_t id);
uint8_t cosSlot(uint8_t id);
bool cosOwned(uint8_t id);
bool cosNone(uint8_t id);                 // the "no hat" / "no face item" entries
const char *wardrobeSkinName(uint8_t i);
const char *wardrobeEyeName(uint8_t i);
const char *wardrobeHairName(uint8_t i);
const char *wardrobeHairColName(uint8_t i);

extern Wardrobe wardrobe;
void wardrobeBegin();
bool wardrobeSave();
void wardrobeClearAll();                  // WIPE, and tests
// The next / previous OWNED item for `slot` from `cur` (wraps).
uint8_t wardrobeStep(uint8_t slot, uint8_t cur, int dir);
uint8_t cosOwnedCount();
uint8_t cosUnlockableCount();             // owned-or-not, excluding the free starters

// An S grade in arcade game `game` at `nowSecs` (epoch): grant one random cosmetic
// not yet owned, if the game is off cooldown. Returns its id, or 0xFF for none
// (on cooldown, or everything is owned). Saved before it returns.
uint8_t wardrobeArcadePrize(uint8_t game, uint32_t nowSecs);
uint32_t wardrobePrizeReadyAt(uint8_t game);   // epoch secs; 0 = ready now

// Compose the current look into a 16x16 index map (0xFF = transparent) and its
// palette. Same shape as AvatarArt, so drawAvatar() draws it like any other.
#define WARDROBE_PAL 16
void wardrobeCompose(uint8_t idx[256], uint16_t pal[WARDROBE_PAL]);
