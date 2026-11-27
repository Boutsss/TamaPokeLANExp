#include "wardrobe.h"
#include "pokemmo_art.h"
#include "nvsinfo.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

Wardrobe wardrobe;

// PokeMMO's slot numbers -> ours, and back.
static uint8_t wsOfPm(uint8_t pmSlot) {
  switch (pmSlot) {
    case 2: return WS_HAT;  case 3: return WS_HAIR;  case 4: return WS_EYES;  case 5: return WS_FACE;
    case 6: return WS_BACK; case 7: return WS_TOP;   case 8: return WS_GLOVES; case 9: return WS_SHOES;
    case 10: return WS_LEGS;
  }
  return 0xFF;
}

// Owned cosmetics, one bit per PM_ITEM index (rebuilt from saved PokeMMO ids).
static uint8_t s_owned[(PM_ITEMS + 7) / 8];
static uint32_t s_prizeAt[WARDROBE_GAMES];

static const char *const SKIN_NAME[PM_SKINS] = { "TONE 1", "TONE 2", "TONE 3", "TONE 4", "TONE 5" };

uint16_t cosCount() { return PM_ITEMS; }
const char *cosName(uint16_t i) { return i < PM_ITEMS ? PM_ITEM[i].name : "NONE"; }
bool cosFree(uint16_t i) { return i < PM_ITEMS && PM_ITEM[i].free; }
bool cosOwned(uint16_t i) { return i < PM_ITEMS && (PM_ITEM[i].free || ((s_owned[i >> 3] >> (i & 7)) & 1)); }
uint16_t cosOwnedCount() { uint16_t n = 0; for (uint16_t i = 0; i < PM_ITEMS; i++) if (!PM_ITEM[i].free && cosOwned(i)) n++; return n; }
uint16_t cosUnlockableCount() { uint16_t n = 0; for (uint16_t i = 0; i < PM_ITEMS; i++) if (!PM_ITEM[i].free) n++; return n; }
const char *wardrobeSkinName(uint8_t i) { return SKIN_NAME[i % PM_SKINS]; }
const char *wardrobeDyeName(uint8_t i) { return PM_DYE_NAME[i % PM_DYES]; }
uint8_t wardrobeDyeCount() { return PM_DYES; }
uint8_t wardrobeSkinCount() { return PM_SKINS; }
bool wardrobeSlotMayBeEmpty(uint8_t s) { return s == WS_HAT || s == WS_FACE || s == WS_BACK || s == WS_GLOVES; }
bool wardrobeHairDyeable() {
  uint16_t h = wardrobe.worn[WS_HAIR];
  return h < PM_ITEMS && PM_ITEM[h].dye != 0xFFFF;
}

static uint16_t itemById(uint16_t id) {
  for (uint16_t i = 0; i < PM_ITEMS; i++) if (PM_ITEM[i].id == id) return i;
  return WARDROBE_NONE;
}
static uint8_t slotOf(uint16_t i) { return i < PM_ITEMS ? wsOfPm(PM_ITEM[i].slot) : 0xFF; }

uint16_t wardrobeStep(uint8_t slot, uint16_t cur, int dir) {
  // the cycle is: [NONE,] then every owned item of the slot in table order
  const bool none = wardrobeSlotMayBeEmpty(slot);
  int n = PM_ITEMS + 1;
  int at = cur == WARDROBE_NONE ? PM_ITEMS : cur;   // NONE sits after the last item
  for (int k = 1; k <= n; k++) {
    int i = ((at + (dir >= 0 ? k : -k)) % n + n) % n;
    if (i == PM_ITEMS) { if (none) return WARDROBE_NONE; continue; }
    if (slotOf((uint16_t)i) == slot && cosOwned((uint16_t)i)) return (uint16_t)i;
  }
  return cur;
}

// ---------------------------------------------------------------- NVS
// "wardrobe" v2: [u8 2][u8 skin][u8 hairDye][u16 worn PokeMMO id x WS_SLOTS]
// [u32 prizeAt x WARDROBE_GAMES][u16 n][u16 owned PokeMMO id x n]. Ids, not
// indices, so regenerating the art never changes what anybody owns or wears.
// (v1 was the hand-drawn wardrobe, only ever on one test board: discarded.)
#define WD_KEY "wardrobe"
#define WD_VER 2
#define WD_FIXED (3 + 2 * WS_SLOTS + 4 * WARDROBE_GAMES + 2)

static void defaults() {
  wardrobe = Wardrobe();
  for (uint8_t s = 0; s < WS_SLOTS; s++) wardrobe.worn[s] = WARDROBE_NONE;
  // PokeMMO's own starting look
  wardrobe.worn[WS_HAIR] = itemById(1183);   // Sideswept
  wardrobe.worn[WS_EYES] = itemById(1441);   // Amber Eyes
  wardrobe.worn[WS_FACE] = itemById(1322);   // Eyebrow
  wardrobe.worn[WS_TOP] = itemById(1323);    // T-Shirt
  wardrobe.worn[WS_LEGS] = itemById(1316);   // Pants
  wardrobe.worn[WS_SHOES] = itemById(1327);  // Boots
}

static void sanitize() {
  wardrobe.skin %= PM_SKINS;
  wardrobe.hairDye %= PM_DYES;
  for (uint8_t s = 0; s < WS_SLOTS; s++) {
    uint16_t w = wardrobe.worn[s];
    bool ok = (w == WARDROBE_NONE && wardrobeSlotMayBeEmpty(s)) || (w < PM_ITEMS && slotOf(w) == s && cosOwned(w));
    if (!ok) wardrobe.worn[s] = wardrobeStep(s, WARDROBE_NONE, 1);
  }
}

void wardrobeBegin() {
  memset(s_owned, 0, sizeof(s_owned));
  memset(s_prizeAt, 0, sizeof(s_prizeAt));
  defaults();
  Preferences p;
  if (p.begin("tamapoke", true)) {
    size_t n = p.getBytesLength(WD_KEY);
    if (n >= WD_FIXED && n <= 4000) {
      uint8_t *b = (uint8_t *)malloc(n);
      if (b && p.getBytes(WD_KEY, b, n) == n && b[0] == WD_VER) {
        size_t at = 1;
        wardrobe.skin = b[at++]; wardrobe.hairDye = b[at++];
        for (uint8_t s = 0; s < WS_SLOTS; s++, at += 2) {
          uint16_t id = (uint16_t)(b[at] | (b[at + 1] << 8));
          wardrobe.worn[s] = id ? itemById(id) : WARDROBE_NONE;
        }
        memcpy(s_prizeAt, b + at, 4 * WARDROBE_GAMES); at += 4 * WARDROBE_GAMES;
        uint16_t cnt = (uint16_t)(b[at] | (b[at + 1] << 8)); at += 2;
        for (uint16_t k = 0; k < cnt && at + 1 < n; k++, at += 2) {
          uint16_t i = itemById((uint16_t)(b[at] | (b[at + 1] << 8)));
          if (i < PM_ITEMS) s_owned[i >> 3] |= (uint8_t)(1u << (i & 7));
        }
      }
      free(b);
    }
    p.end();
  }
  sanitize();
}

bool wardrobeSave() {
  uint16_t cnt = 0;
  for (uint16_t i = 0; i < PM_ITEMS; i++) if (!PM_ITEM[i].free && cosOwned(i)) cnt++;
  size_t n = WD_FIXED + 2 * cnt;
  uint8_t *b = (uint8_t *)malloc(n);
  if (!b) return false;
  size_t at = 0;
  b[at++] = WD_VER; b[at++] = wardrobe.skin; b[at++] = wardrobe.hairDye;
  for (uint8_t s = 0; s < WS_SLOTS; s++) {
    uint16_t id = wardrobe.worn[s] < PM_ITEMS ? PM_ITEM[wardrobe.worn[s]].id : 0;
    b[at++] = (uint8_t)id; b[at++] = (uint8_t)(id >> 8);
  }
  memcpy(b + at, s_prizeAt, 4 * WARDROBE_GAMES); at += 4 * WARDROBE_GAMES;
  b[at++] = (uint8_t)cnt; b[at++] = (uint8_t)(cnt >> 8);
  for (uint16_t i = 0; i < PM_ITEMS; i++)
    if (!PM_ITEM[i].free && cosOwned(i)) { b[at++] = (uint8_t)PM_ITEM[i].id; b[at++] = (uint8_t)(PM_ITEM[i].id >> 8); }
  Preferences p;
  bool ok = p.begin("tamapoke", false) && p.putBytes(WD_KEY, b, at) == at;
  p.end();
  free(b);
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

uint16_t wardrobeArcadePrize(uint8_t game, uint32_t nowSecs) {
  if (game >= WARDROBE_GAMES) return WARDROBE_NONE;
  // a clock set BACKWARDS (an RTC reset) must not lock the game for good
  if (s_prizeAt[game] && nowSecs >= s_prizeAt[game] && nowSecs - s_prizeAt[game] < WARDROBE_PRIZE_SECS) return WARDROBE_NONE;
  uint16_t left = 0;
  for (uint16_t i = 0; i < PM_ITEMS; i++) if (!cosOwned(i)) left++;
  if (!left) return WARDROBE_NONE;
  uint16_t pick = (uint16_t)random(left), id = WARDROBE_NONE;
  for (uint16_t i = 0; i < PM_ITEMS; i++) if (!cosOwned(i) && pick-- == 0) { id = i; break; }
  s_owned[id >> 3] |= (uint8_t)(1u << (id & 7));
  uint32_t before = s_prizeAt[game];
  s_prizeAt[game] = nowSecs ? nowSecs : 1;
  if (!wardrobeSave()) { s_owned[id >> 3] &= (uint8_t)~(1u << (id & 7)); s_prizeAt[game] = before; return WARDROBE_NONE; }
  return id;
}

// ---------------------------------------------------------------- composing the trainer
int wardrobeCanvasW() { return PM_CANVAS_W; }
int wardrobeCanvasH() { return PM_CANVAS_H; }

// Paint one RLE layer: (run, index) pairs over its bounding box, index 0 = clear.
static void paint(uint16_t *px, uint8_t *mask, int x0, int y0, int w, int h, uint32_t data, uint16_t len, uint16_t pal) {
  int cell = 0, cells = w * h;
  for (uint16_t k = 0; k + 1 < len && cell < cells; k += 2) {
    uint8_t run = PM_DATA[data + k], v = PM_DATA[data + k + 1];
    for (uint8_t r = 0; r < run && cell < cells; r++, cell++) {
      if (!v) continue;
      int x = x0 + cell % w, y = y0 + cell / w;
      if (x < 0 || y < 0 || x >= PM_CANVAS_W || y >= PM_CANVAS_H) continue;
      px[y * PM_CANVAS_W + x] = PM_PAL[pal + v - 1];
      mask[y * PM_CANVAS_W + x] = 1;
    }
  }
}
static void paintItem(uint16_t *px, uint8_t *mask, uint16_t i) {
  if (i >= PM_ITEMS) return;
  const PmItem &it = PM_ITEM[i];
  uint16_t pal = it.pal;
  if (wsOfPm(it.slot) == WS_HAIR && it.dye != 0xFFFF && wardrobe.hairDye) {
    uint16_t d = PM_DYE_PAL[it.dye + wardrobe.hairDye % PM_DYES];
    if (d != 0xFFFF) pal = d;
  }
  paint(px, mask, it.x, it.y, it.w, it.h, it.data, it.len, pal);
}

void wardrobeCompose(uint16_t *px, uint8_t *mask) {
  memset(mask, 0, PM_CANVAS_W * PM_CANVAS_H);
  // PokeMMO's order: what hangs behind, the body, then outward to the hat.
  paintItem(px, mask, wardrobe.worn[WS_BACK]);
  paint(px, mask, PM_BODY_X, PM_BODY_Y, PM_BODY_W, PM_BODY_H, PM_BODY_DATA, PM_BODY_LEN, PM_SKIN_PAL[wardrobe.skin % PM_SKINS]);
  static const uint8_t ORDER[] = { WS_LEGS, WS_SHOES, WS_TOP, WS_GLOVES, WS_FACE, WS_EYES, WS_HAIR, WS_HAT };
  for (uint8_t s : ORDER) paintItem(px, mask, wardrobe.worn[s]);
}
