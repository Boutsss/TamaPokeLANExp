#pragma once
#include <stdint.h>
#include "avatar_names.h"

// The Rocket Game Corner's cosmetic prizes: card backs and trainer-card
// backgrounds. Every one is DRAWN, not stored: a name, a few RGB565 colours and
// a motif/pattern that ui_casino.ino knows how to paint. That is why there can
// be dozens of them for a few hundred bytes of flash.
//
// APPEND-ONLY, both tables. casino.cpp stores ownership as one bit per INDEX,
// so inserting a design renumbers every later one and hands every player who
// owned design N the design that used to be N-1. New designs go at the END.

// ---- card backs --------------------------------------------------------------
enum CardMotif : uint8_t {
  CM_LATTICE = 0,  // diamond lattice: the plain casino back
  CM_BALL,         // a Poke Ball in the centre
  CM_ROCKET,       // the big red R
  CM_FLAME, CM_DROP, CM_LEAF, CM_BOLT, CM_SNOW, CM_FIST, CM_SKULL, CM_MOUND,
  CM_WING, CM_EYE, CM_BUG, CM_ROCK, CM_GHOST, CM_DRAGON, CM_MOON, CM_GEAR,
  CM_STAR, CM_RING,
  CM_M,            // Master Ball
  CM_STRIPES,      // Great/Ultra Ball bands
  CM_COIN,         // a Game Corner coin
};
struct CardBackDef { const char *name; uint16_t base, accent, ink; uint8_t motif; };

static const CardBackDef CARD_BACKS[] = {
  { "CLASSIC",    0x2A5A, 0xF7BE, 0x18C4, CM_LATTICE },   // 0: free
  { "TEAM ROCKET",0x18C3, 0xE8E4, 0xF7BE, CM_ROCKET },
  { "POKE BALL",  0xE8E4, 0xF7BE, 0x18C4, CM_BALL },
  { "GREAT BALL", 0x2B3F, 0xE8E4, 0x18C4, CM_STRIPES },
  { "ULTRA BALL", 0x2945, 0xFEA0, 0x18C4, CM_STRIPES },
  { "MASTER BALL",0x780F, 0xF8B2, 0xF7BE, CM_M },
  { "GAME CORNER",0x8A00, 0xFEA0, 0x18C4, CM_COIN },
  { "NORMAL",     0xA514, 0xF7BE, 0x18C4, CM_RING },
  { "FIRE",       0xE8E4, 0xFD20, 0x18C4, CM_FLAME },
  { "WATER",      0x2C9F, 0xAEFF, 0x18C4, CM_DROP },
  { "GRASS",      0x3D66, 0xAFE5, 0x18C4, CM_LEAF },
  { "ELECTRIC",   0xFE60, 0xFFF4, 0x18C4, CM_BOLT },
  { "ICE",        0x6EDF, 0xF7FF, 0x18C4, CM_SNOW },
  { "FIGHTING",   0xB1A4, 0xFD8E, 0x18C4, CM_FIST },
  { "POISON",     0x8A31, 0xE4FD, 0x18C4, CM_SKULL },
  { "GROUND",     0xCD0A, 0xF6B2, 0x18C4, CM_MOUND },
  { "FLYING",     0x8CBF, 0xE73F, 0x18C4, CM_WING },
  { "PSYCHIC",    0xF2CF, 0xFDF7, 0x18C4, CM_EYE },
  { "BUG",        0x9E62, 0xD7E9, 0x18C4, CM_BUG },
  { "ROCK",       0xAD0A, 0xDEB2, 0x18C4, CM_ROCK },
  { "GHOST",      0x5A50, 0xBC7F, 0xF7BE, CM_GHOST },
  { "DRAGON",     0x601F, 0xA4FF, 0xF7BE, CM_DRAGON },
  { "DARK",       0x3186, 0x9CD3, 0xF7BE, CM_MOON },
  { "STEEL",      0x8C93, 0xDEFB, 0x18C4, CM_GEAR },
  { "FAIRY",      0xEB59, 0xFE7D, 0x18C4, CM_STAR },
  { "MOON STONE", 0x2108, 0xC618, 0xF7BE, CM_MOON },
  { "SAFARI",     0x5C47, 0xC5C8, 0x18C4, CM_STRIPES },
  { "GOLD",       0xC4A0, 0xFF0F, 0x18C4, CM_COIN },
};
#define CARD_BACK_COUNT ((uint8_t)(sizeof(CARD_BACKS) / sizeof(CARD_BACKS[0])))

// ---- trainer-card backgrounds ------------------------------------------------
enum CardPattern : uint8_t {
  BG_PLAIN = 0,    // the card's own cream: the free default
  BG_GRADIENT,     // c0 at the top fading to c1
  BG_STRIPES,      // diagonal bands c0/c1
  BG_DOTS,         // c1 polka dots on c0
  BG_CHECKER,      // c0/c1 squares
  BG_WAVES,        // rolling water
  BG_STARS,        // night sky, twinkling c2
  BG_FLAMES,       // tongues of fire rising
  BG_LEAVES,       // falling leaves
  BG_BOLTS,        // lightning zig-zags
  BG_SNOW,         // drifting flakes
  BG_BUBBLES,      // rising bubbles
  BG_HEX,          // honeycomb
  BG_ROCKET,       // tiled R
  BG_BALLS,        // tiled Poke Balls
  BG_HILLS,        // layered hills under a sky
  BG_SUNSET,       // horizon bands
  BG_GRID,         // retro Game Boy grid
  // ANIMATED (they move every frame on the card): the walking zones, after
  // Catch 151's (MIT) -- see ui_casino.ino for the credit.
  BG_ANIM_FIELD, BG_ANIM_FOREST, BG_ANIM_BEACH, BG_ANIM_CAVE, BG_ANIM_MANSION,
  BG_ANIM_AURORA,  // shifting light curtains
};
inline bool cardBgAnimated(uint8_t pattern) { return pattern >= BG_ANIM_FIELD; }
struct CardBgDef { const char *name; uint16_t c0, c1, c2; uint8_t pattern; };

static const CardBgDef CARD_BGS[] = {
  { "DEFAULT",       0xF79D, 0xF79D, 0x18C4, BG_PLAIN },     // 0: free
  // the eighteen types
  { "NORMAL",        0xD69A, 0xB596, 0x8410, BG_DOTS },
  { "FIRE",          0xFB40, 0xF800, 0xFFE0, BG_FLAMES },
  { "WATER",         0x4D9F, 0x2A7F, 0xAEFF, BG_WAVES },
  { "GRASS",         0x8F2A, 0x4DA4, 0x2B02, BG_LEAVES },
  { "ELECTRIC",      0xFF0A, 0xFE60, 0x18C4, BG_BOLTS },
  { "ICE",           0xBF7F, 0x8E9F, 0xFFFF, BG_SNOW },
  { "FIGHTING",      0xD2A9, 0xA145, 0xFD8E, BG_STRIPES },
  { "POISON",        0xBB37, 0x8A31, 0xE4FD, BG_BUBBLES },
  { "GROUND",        0xE5CE, 0xCD0A, 0x9B66, BG_HILLS },
  { "FLYING",        0xBDDF, 0x8CBF, 0xFFFF, BG_GRADIENT },
  { "PSYCHIC",       0xFB97, 0xF2CF, 0xFDF7, BG_HEX },
  { "BUG",           0xBEE8, 0x9E62, 0x6C40, BG_HEX },
  { "ROCK",          0xC5AE, 0xAD0A, 0x7BCF, BG_CHECKER },
  { "GHOST",         0x7B33, 0x4A0E, 0xBC7F, BG_STARS },
  { "DRAGON",        0x7A3F, 0x401A, 0xA4FF, BG_GRADIENT },
  { "DARK",          0x4A69, 0x2104, 0x9CD3, BG_STARS },
  { "STEEL",         0xBDF7, 0x8C93, 0xDEFB, BG_GRID },
  { "FAIRY",         0xFDBA, 0xEB59, 0xFFFF, BG_DOTS },
  // places and things
  { "PALLET TOWN",   0x9EDF, 0x6E87, 0xFFFF, BG_HILLS },
  { "VIRIDIAN WOOD", 0x5D29, 0x2B02, 0x8F2A, BG_LEAVES },
  { "MT MOON",       0x2108, 0x52AA, 0xF7BE, BG_STARS },
  { "LAVENDER",      0xB57F, 0x7B33, 0xE73F, BG_GRADIENT },
  { "CINNABAR",      0xFC80, 0x9800, 0xFFE0, BG_SUNSET },
  { "SEAFOAM",       0x7F3F, 0x3D7F, 0xFFFF, BG_BUBBLES },
  { "SAFARI ZONE",   0xC5C8, 0x5C47, 0x8F2A, BG_STRIPES },
  { "VICTORY ROAD",  0x9B66, 0x5A69, 0xD69A, BG_CHECKER },
  { "INDIGO",        0x3A1F, 0x180F, 0xFEA0, BG_GRADIENT },
  { "TEAM ROCKET",   0x2104, 0x4208, 0xE8E4, BG_ROCKET },
  { "GAME CORNER",   0x8A00, 0xC4A0, 0xFEA0, BG_DOTS },
  { "POKE BALLS",    0xF79D, 0xE8E4, 0x18C4, BG_BALLS },
  { "MASTER BALLS",  0xBC7F, 0x780F, 0xF8B2, BG_BALLS },
  { "GAME BOY",      0x9DE5, 0x6CA2, 0x3A00, BG_GRID },
  { "SUNSET",        0xFD20, 0xD8A7, 0x5A50, BG_SUNSET },
  { "NIGHT SKY",     0x10A6, 0x0841, 0xFFF4, BG_STARS },
  { "OCEAN",         0x3D7F, 0x1A3A, 0xAEFF, BG_WAVES },
  { "SAKURA",        0xFE7D, 0xFB56, 0xFFFF, BG_LEAVES },
  { "ICE PATH",      0xDF7F, 0x8E9F, 0xFFFF, BG_SNOW },
  { "VOLCANO",       0x9800, 0x4000, 0xFD20, BG_FLAMES },
  { "THUNDERSTORM",  0x4A50, 0x2108, 0xFFE0, BG_BOLTS },
  { "CANDY",         0xFE7D, 0x9EFF, 0xFFF4, BG_STRIPES },
  { "GOLD RUSH",     0xFEA0, 0xC4A0, 0xFFF4, BG_CHECKER },
  // animated
  { "LIVE FIELD",    0x9EDF, 0x6E87, 0xFFFF, BG_ANIM_FIELD },
  { "LIVE FOREST",   0x5D29, 0x2B02, 0x8F2A, BG_ANIM_FOREST },
  { "LIVE BEACH",    0x7F3F, 0xF6B2, 0xFFFF, BG_ANIM_BEACH },
  { "LIVE CAVE",     0x2945, 0x52AA, 0xFEA0, BG_ANIM_CAVE },
  { "LIVE MANSION",  0x3186, 0x5A50, 0xFFE0, BG_ANIM_MANSION },
  { "AURORA",        0x0843, 0x07F0, 0x781F, BG_ANIM_AURORA },
};
#define CARD_BG_COUNT ((uint8_t)(sizeof(CARD_BGS) / sizeof(CARD_BGS[0])))
