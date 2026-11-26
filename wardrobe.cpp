#include "wardrobe.h"
#include "wardrobe_art.h"
#include "nvsinfo.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

Wardrobe wardrobe;

// ---------------------------------------------------------------- the catalogue
// APPEND-ONLY: an id is stored raw (worn[] and the owned bitmask). `grid` is the
// item's shape, or nullptr for an item that only recolours the body's own
// clothes; c1/c2 fill its main and accent slots.
struct CosDef { const char *name; uint8_t slot; const char *const *grid; uint16_t c1, c2; bool free; };
static const CosDef COS[] = {
  // hats
  { "NO HAT",      WS_HAT,   nullptr,         0,      0,      true  },   // 0
  { "BALL CAP",    WS_HAT,   W_HAT_CAP,       0xD8A3, 0xFFFF, true  },
  { "BEANIE",      WS_HAT,   W_HAT_BEANIE,    0x298C, 0xFFFF, false },
  { "SUN HAT",     WS_HAT,   W_HAT_SUNHAT,    0xE6B0, 0xD8A3, false },
  { "BERET",       WS_HAT,   W_HAT_BERET,     0xC104, 0xC104, false },
  { "BANDANA",     WS_HAT,   W_HAT_BANDANA,   0xD8A3, 0xFFFF, false },   // 5
  { "WITCH HAT",   WS_HAT,   W_HAT_WITCH,     0x5011, 0xFEA0, false },
  { "CROWN",       WS_HAT,   W_HAT_CROWN,     0xD8A3, 0xFEA0, false },
  { "TOP HAT",     WS_HAT,   W_HAT_TOPHAT,    0x2104, 0xC104, false },
  { "FLOWER",      WS_HAT,   W_HAT_FLOWER,    0xFEA0, 0xF3D5, false },
  { "HEADPHONES",  WS_HAT,   W_HAT_PHONES,    0x8410, 0xD8A3, false },   // 10
  { "BOW",         WS_HAT,   W_HAT_BOW,       0xF3D5, 0xD8A3, false },
  // tops
  { "WHITE TEE",   WS_TOP,   nullptr,         0xEF7D, 0x4208, true  },
  { "BLUE TEE",    WS_TOP,   nullptr,         0x2B7F, 0x4208, true  },
  { "HOODIE",      WS_TOP,   W_TOP_HOODIE,    0x8C71, 0x6B4D, false },
  { "DARK HOODIE", WS_TOP,   W_TOP_HOODIE,    0x2104, 0x4208, false },   // 15
  { "JACKET",      WS_TOP,   W_TOP_JACKET,    0x3CAA, 0x4208, false },
  { "STRIPES",     WS_TOP,   W_TOP_STRIPED,   0xEF7D, 0xD8A3, false },
  { "TANK TOP",    WS_TOP,   W_TOP_TANK,      0xFC60, 0x4208, false },
  { "DRESS",       WS_TOP,   W_TOP_DRESS,     0xF3D5, 0xD8A3, false },
  { "RAINCOAT",    WS_TOP,   W_TOP_COAT,      0xFEA0, 0x2104, false },   // 20
  { "KIMONO",      WS_TOP,   W_TOP_KIMONO,    0x8236, 0xC104, false },
  { "SUIT",        WS_TOP,   W_TOP_SUIT,      0x2124, 0xC104, false },
  { "BALL TEE",    WS_TOP,   W_TOP_BALL,      0xD8A3, 0x2104, false },
  // legs
  { "JEANS",       WS_LEGS,  nullptr,         0x3B35, 0x2104, true  },
  { "SHORTS",      WS_LEGS,  W_LEGS_SHORTS,   0xC5AE, 0x2104, true  },   // 25
  { "SKIRT",       WS_LEGS,  W_LEGS_SKIRT,    0x4379, 0x2104, true  },
  { "BLACK PANTS", WS_LEGS,  nullptr,         0x2104, 0x2104, false },
  { "CARGO",       WS_LEGS,  W_LEGS_CARGO,    0x6B6A, 0x4A69, false },
  { "PLAID SKIRT", WS_LEGS,  W_LEGS_PLAID,    0xC104, 0x2104, false },
  { "LONG SKIRT",  WS_LEGS,  W_LEGS_LONGSKIRT,0x8236, 0x2104, false },   // 30
  { "TRACKSUIT",   WS_LEGS,  W_LEGS_TRACK,    0x298C, 0xFFFF, false },
  // shoes
  { "SNEAKERS",    WS_SHOES, nullptr,         0xD8A3, 0,      true  },
  { "BOOTS",       WS_SHOES, W_SHOES_BOOTS,   0x7A23, 0,      false },
  { "WHITE KICKS", WS_SHOES, nullptr,         0xFFFF, 0,      false },
  { "SANDALS",     WS_SHOES, W_SHOES_SANDALS, 0xC5AE, 0,      false },   // 35
  { "GOLD SHOES",  WS_SHOES, nullptr,         0xFEA0, 0,      false },
  { "SKATE SHOES", WS_SHOES, nullptr,         0x2104, 0,      false },
  // face
  { "NO FACE",     WS_FACE,  nullptr,         0,      0,      true  },
  { "GLASSES",     WS_FACE,  W_FACE_GLASSES,  0x2104, 0,      false },
  { "SHADES",      WS_FACE,  W_FACE_SHADES,   0x10A2, 0,      false },   // 40
  { "MASK",        WS_FACE,  W_FACE_MASK,     0xEF7D, 0,      false },
  { "EYEPATCH",    WS_FACE,  W_FACE_EYEPATCH, 0x10A2, 0,      false },
  { "BLUSH",       WS_FACE,  W_FACE_BLUSH,    0xF3D5, 0,      false },
  { "BANDAGE",     WS_FACE,  W_FACE_BANDAGE,  0xE6B0, 0,      false },
  // back
  { "NO BACK",     WS_BACK,  nullptr,         0,      0,      true  },   // 45
  { "BACKPACK",    WS_BACK,  W_BACK_PACK,     0xD8A3, 0xFEA0, false },
  { "CAPE",        WS_BACK,  W_BACK_CAPE,     0xC104, 0xC104, false },
  { "WINGS",       WS_BACK,  W_BACK_WINGS,    0xEF7D, 0xBDF7, false },
  { "SWORD",       WS_BACK,  W_BACK_SWORD,    0x8410, 0xFEA0, false },
  { "BALL PACK",   WS_BACK,  W_BACK_PACK,     0xD8A3, 0xFFFF, false },   // 50
  // APPEND-ONLY BELOW HERE
};
#define COS_N ((uint8_t)(sizeof(COS) / sizeof(COS[0])))
static_assert(sizeof(COS) / sizeof(COS[0]) <= 64, "the owned bitmask is 64 bits");

// The free choices: always unlocked, so stored as plain indices.
static const char *const SKIN_NAME[WARDROBE_SKINS] = { "LIGHT", "FAIR", "MEDIUM", "TAN", "BROWN", "DARK" };
static const uint16_t SKIN_COL[WARDROBE_SKINS] = { 0xFED7, 0xF655, 0xCCAD, 0xC46C, 0xA369, 0x59E5 };
static const char *const EYE_NAME[WARDROBE_EYES] = { "BROWN", "BLUE", "GREEN", "HAZEL", "GRAY", "VIOLET" };
static const uint16_t EYE_COL[WARDROBE_EYES] = { 0x6A25, 0x2B7F, 0x2D4A, 0x9B26, 0x7BEF, 0x895F };
static const char *const HAIR_NAME[WARDROBE_HAIRS] = {
  "BALD", "SHORT", "SPIKY", "SIDE PART", "CURLY", "BUZZ", "LONG", "PONYTAIL", "BOB", "BRAIDS", "BUNS", "MOHAWK" };
static const char *const *const HAIR_GRID[WARDROBE_HAIRS] = {
  nullptr, W_HAIR_SHORT, W_HAIR_SPIKY, W_HAIR_SIDEPART, W_HAIR_CURLY, W_HAIR_BUZZ, W_HAIR_LONG,
  W_HAIR_PONYTAIL, W_HAIR_BOB, W_HAIR_BRAIDS, W_HAIR_BUNS, W_HAIR_MOHAWK };
static const char *const HAIRCOL_NAME[WARDROBE_HAIR_COLS] = {
  "BLACK", "DARK BROWN", "BROWN", "BLONDE", "GINGER", "RED", "GRAY", "WHITE", "BLUE", "PINK", "PURPLE", "GREEN" };
static const uint16_t HAIRCOL[WARDROBE_HAIR_COLS] = {
  0x2104, 0x4A02, 0x7A23, 0xF64A, 0xDB04, 0xB8A3, 0x9CF3, 0xEF7D, 0x2B7F, 0xF3D5, 0x895F, 0x3CAA };

static uint64_t s_owned = 0;
static uint32_t s_prizeAt[WARDROBE_GAMES] = { 0 };   // epoch of the last cosmetic prize, per game

uint8_t cosCount() { return COS_N; }
const char *cosName(uint8_t id) { return id < COS_N ? COS[id].name : "-"; }
uint8_t cosSlot(uint8_t id) { return id < COS_N ? COS[id].slot : 0xFF; }
bool cosOwned(uint8_t id) { return id < COS_N && (COS[id].free || ((s_owned >> id) & 1)); }
bool cosNone(uint8_t id) { return id < COS_N && COS[id].free && !COS[id].grid && (COS[id].slot == WS_HAT || COS[id].slot == WS_FACE || COS[id].slot == WS_BACK); }
const char *wardrobeSkinName(uint8_t i) { return SKIN_NAME[i % WARDROBE_SKINS]; }
const char *wardrobeEyeName(uint8_t i) { return EYE_NAME[i % WARDROBE_EYES]; }
const char *wardrobeHairName(uint8_t i) { return HAIR_NAME[i % WARDROBE_HAIRS]; }
const char *wardrobeHairColName(uint8_t i) { return HAIRCOL_NAME[i % WARDROBE_HAIR_COLS]; }

uint8_t cosOwnedCount() { uint8_t n = 0; for (uint8_t i = 0; i < COS_N; i++) if (!COS[i].free && cosOwned(i)) n++; return n; }
uint8_t cosUnlockableCount() { uint8_t n = 0; for (uint8_t i = 0; i < COS_N; i++) if (!COS[i].free) n++; return n; }

// The first item of a slot, which is what a fresh save wears.
static uint8_t firstOf(uint8_t slot) {
  for (uint8_t i = 0; i < COS_N; i++) if (COS[i].slot == slot) return i;
  return 0;
}

uint8_t wardrobeStep(uint8_t slot, uint8_t cur, int dir) {
  for (uint8_t n = 1; n <= COS_N; n++) {
    int i = ((int)cur + (dir >= 0 ? n : -(int)n)) % COS_N;
    if (i < 0) i += COS_N;
    if (COS[i].slot == slot && cosOwned((uint8_t)i)) return (uint8_t)i;
  }
  return cur;
}

// ---------------------------------------------------------------- NVS
// One blob, append-only: [u8 ver][u8 len] skin eyes hair hairCol worn[6]
// owned(u64) prizeAt[6](u32). A shorter (older) blob leaves the rest default.
#define WD_KEY "wardrobe"
#define WD_VER 1
#define WD_LEN (2 + 4 + WS_SLOTS + 8 + 4 * WARDROBE_GAMES)

static void sanitize() {
  wardrobe.skin %= WARDROBE_SKINS; wardrobe.eyes %= WARDROBE_EYES;
  wardrobe.hair %= WARDROBE_HAIRS; wardrobe.hairCol %= WARDROBE_HAIR_COLS;
  for (uint8_t s = 0; s < WS_SLOTS; s++)
    if (cosSlot(wardrobe.worn[s]) != s || !cosOwned(wardrobe.worn[s])) wardrobe.worn[s] = firstOf(s);
}

void wardrobeBegin() {
  wardrobe = Wardrobe();
  for (uint8_t s = 0; s < WS_SLOTS; s++) wardrobe.worn[s] = firstOf(s);
  wardrobe.worn[WS_TOP] = 13;   // the blue tee, so a new trainer is not all white
  s_owned = 0;
  memset(s_prizeAt, 0, sizeof(s_prizeAt));
  Preferences p;
  if (p.begin("tamapoke", true)) {
    uint8_t b[64] = { 0 };
    size_t n = p.getBytesLength(WD_KEY);
    if (n >= 2 && n <= sizeof(b) && p.getBytes(WD_KEY, b, n) == n && b[1] <= n) {
      uint8_t cur[64] = { 0 };
      // defaults first, then the stored prefix over them
      cur[2] = wardrobe.skin; cur[3] = wardrobe.eyes; cur[4] = wardrobe.hair; cur[5] = wardrobe.hairCol;
      memcpy(cur + 6, wardrobe.worn, WS_SLOTS);
      size_t take = b[1] < WD_LEN ? b[1] : WD_LEN;
      memcpy(cur + 2, b + 2, take > 2 ? take - 2 : 0);
      size_t at = 2;
      wardrobe.skin = cur[at++]; wardrobe.eyes = cur[at++]; wardrobe.hair = cur[at++]; wardrobe.hairCol = cur[at++];
      memcpy(wardrobe.worn, cur + at, WS_SLOTS); at += WS_SLOTS;
      memcpy(&s_owned, cur + at, 8); at += 8;
      memcpy(s_prizeAt, cur + at, 4 * WARDROBE_GAMES); at += 4 * WARDROBE_GAMES;
    }
    p.end();
  }
  sanitize();
}

bool wardrobeSave() {
  uint8_t b[WD_LEN];
  size_t at = 2;
  b[at++] = wardrobe.skin; b[at++] = wardrobe.eyes; b[at++] = wardrobe.hair; b[at++] = wardrobe.hairCol;
  memcpy(b + at, wardrobe.worn, WS_SLOTS); at += WS_SLOTS;
  memcpy(b + at, &s_owned, 8); at += 8;
  memcpy(b + at, s_prizeAt, 4 * WARDROBE_GAMES); at += 4 * WARDROBE_GAMES;
  b[0] = WD_VER; b[1] = (uint8_t)at;
  Preferences p;
  if (!p.begin("tamapoke", false)) return false;
  bool ok = p.putBytes(WD_KEY, b, at) == at;
  p.end();
  if (!ok) logKeyFailure("wardrobe", WD_KEY);
  return ok;
}

void wardrobeClearAll() {
  Preferences p;
  if (p.begin("tamapoke", false)) { p.remove(WD_KEY); p.end(); }
  wardrobeBegin();
}

// ---------------------------------------------------------------- the arcade prize
uint32_t wardrobePrizeReadyAt(uint8_t game) {
  if (game >= WARDROBE_GAMES || !s_prizeAt[game]) return 0;
  return s_prizeAt[game] + WARDROBE_PRIZE_SECS;
}

uint8_t wardrobeArcadePrize(uint8_t game, uint32_t nowSecs) {
  if (game >= WARDROBE_GAMES) return 0xFF;
  // A clock that went BACKWARDS (an RTC reset) must not lock the game for good:
  // anything stamped in the future is treated as ready.
  if (s_prizeAt[game] && nowSecs >= s_prizeAt[game] && nowSecs - s_prizeAt[game] < WARDROBE_PRIZE_SECS) return 0xFF;
  uint8_t pool[64], n = 0;
  for (uint8_t i = 0; i < COS_N; i++) if (!cosOwned(i)) pool[n++] = i;
  if (!n) return 0xFF;
  uint8_t id = pool[random(n)];
  s_owned |= (uint64_t)1 << id;
  s_prizeAt[game] = nowSecs ? nowSecs : 1;
  if (!wardrobeSave()) { s_owned &= ~((uint64_t)1 << id); s_prizeAt[game] = 0; return 0xFF; }
  return id;
}

// ---------------------------------------------------------------- composing the look
enum : uint8_t { P_OUT = 0, P_SKIN, P_EYE, P_HAIR, P_TOP, P_TOPX, P_WHITE, P_LEGS, P_LEGSX, P_SHOES,
                 P_HAT, P_HATX, P_FACE, P_BACK, P_BACKX, P_SPARE };
static_assert(P_SPARE < WARDROBE_PAL, "palette slots");

static uint8_t slotOf(char ch) {
  switch (ch) {
    case '#': return P_OUT;  case 's': return P_SKIN; case 'e': return P_EYE;  case 'h': return P_HAIR;
    case 'c': return P_TOP;  case 'x': return P_TOPX; case 'w': return P_WHITE;
    case 'd': return P_LEGS; case 'y': return P_LEGSX; case 'f': return P_SHOES;
    case 'k': return P_HAT;  case 'K': return P_HATX; case 'g': return P_FACE;
    case 'b': return P_BACK; case 'B': return P_BACKX;
  }
  return 0xFF;
}
static void paint(uint8_t idx[256], const char *const *g) {
  if (!g) return;
  for (int r = 0; r < 16; r++)
    for (int c = 0; c < 16; c++) {
      uint8_t s = slotOf(g[r][c]);
      if (s != 0xFF) idx[r * 16 + c] = s;
    }
}

void wardrobeCompose(uint8_t idx[256], uint16_t pal[WARDROBE_PAL]) {
  const CosDef &top = COS[wardrobe.worn[WS_TOP]], &legs = COS[wardrobe.worn[WS_LEGS]];
  const CosDef &shoes = COS[wardrobe.worn[WS_SHOES]], &hat = COS[wardrobe.worn[WS_HAT]];
  const CosDef &face = COS[wardrobe.worn[WS_FACE]], &back = COS[wardrobe.worn[WS_BACK]];
  pal[P_OUT] = 0x18C4;                pal[P_SKIN] = SKIN_COL[wardrobe.skin % WARDROBE_SKINS];
  pal[P_EYE] = EYE_COL[wardrobe.eyes % WARDROBE_EYES];
  pal[P_HAIR] = HAIRCOL[wardrobe.hairCol % WARDROBE_HAIR_COLS];
  pal[P_TOP] = top.c1;  pal[P_TOPX] = top.c2;   pal[P_WHITE] = 0xEF7D;
  pal[P_LEGS] = legs.c1; pal[P_LEGSX] = legs.c2; pal[P_SHOES] = shoes.c1;
  pal[P_HAT] = hat.c1;  pal[P_HATX] = hat.c2;   pal[P_FACE] = face.c1;
  pal[P_BACK] = back.c1; pal[P_BACKX] = back.c2; pal[P_SPARE] = 0;
  memset(idx, 0xFF, 256);
  paint(idx, back.grid);
  paint(idx, W_BODY);
  paint(idx, legs.grid);
  paint(idx, top.grid);
  paint(idx, shoes.grid);
  paint(idx, face.grid);
  paint(idx, HAIR_GRID[wardrobe.hair % WARDROBE_HAIRS]);
  paint(idx, hat.grid);
}
