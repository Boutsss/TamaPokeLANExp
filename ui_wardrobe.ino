// The WARDROBE: dressing the CUSTOM trainer (AVATAR_YOU, wardrobe.cpp) in
// PokeMMO's cosmetics (pokemmo_art.h). Opened from the trainer card's WARDROBE
// button, which only shows while CUSTOM is the avatar worn. A level below the
// card (SCR_WARDROBE): pages on the rim, held BACK returns to the card.
//
// The trainer stands large at the top; under it, one row per thing you can
// change, each with < and > that step through what you OWN (wardrobeStep()).
bool wardrobeOpen = false;
uint8_t wardrobePage = 0;

enum : uint8_t { WR_SKIN = 0, WR_EYES, WR_HAIR, WR_HAIRCOL, WR_HAT, WR_FACE, WR_TOP, WR_GLOVES,
                 WR_LEGS, WR_SHOES, WR_BACK, WR_COUNT };
#define WR_PER_PAGE 4
#define WR_PAGES ((WR_COUNT + WR_PER_PAGE - 1) / WR_PER_PAGE)
#define WR_ROW_Y(i) (164 + (i) * 56)
#define WR_ROW_H 48
#define WR_X0 64
#define WR_X1 402
#define WR_ARROW_W 48

uint8_t wardrobePages() { return WR_PAGES; }

// The wardrobe slot each item row edits (the free rows have none).
static uint8_t wrSlot(uint8_t r) {
  switch (r) {
    case WR_EYES: return WS_EYES;  case WR_HAIR: return WS_HAIR;  case WR_HAT: return WS_HAT;
    case WR_FACE: return WS_FACE;  case WR_TOP: return WS_TOP;    case WR_GLOVES: return WS_GLOVES;
    case WR_LEGS: return WS_LEGS;  case WR_SHOES: return WS_SHOES; case WR_BACK: return WS_BACK;
  }
  return 0xFF;
}
static StrId wrLabel(uint8_t r) {
  static const StrId L[WR_COUNT] = { S_WD_SKIN, S_WD_EYES, S_WD_HAIR, S_WD_HAIRCOL, S_WD_HAT, S_WD_FACE,
                                     S_WD_TOP, S_WD_GLOVES, S_WD_LEGS, S_WD_SHOES, S_WD_BACK };
  return L[r];
}
static const char *wrValue(uint8_t r) {
  if (r == WR_SKIN) return wardrobeSkinName(wardrobe.skin);
  if (r == WR_HAIRCOL) return wardrobeHairDyeable() ? wardrobeDyeName(wardrobe.hairDye) : "-";
  return cosName(wardrobe.worn[wrSlot(r)]);
}
static void wrStep(uint8_t r, int dir) {
  if (r == WR_SKIN) { uint8_t n = wardrobeSkinCount(); wardrobe.skin = (uint8_t)((wardrobe.skin + (dir > 0 ? 1 : n - 1)) % n); }
  else if (r == WR_HAIRCOL) {
    if (!wardrobeHairDyeable()) return;
    uint8_t n = wardrobeDyeCount();
    wardrobe.hairDye = (uint8_t)((wardrobe.hairDye + (dir > 0 ? 1 : n - 1)) % n);
  } else {
    uint8_t s = wrSlot(r);
    wardrobe.worn[s] = wardrobeStep(s, wardrobe.worn[s], dir);
  }
  wardrobeSave();
}

// Geometry, asked rather than restated: the renderer, the taps and the tests.
void wardrobeArrowRect(uint8_t row, int dir, int *x, int *y, int *w, int *h) {
  *y = WR_ROW_Y(row); *h = WR_ROW_H; *w = WR_ARROW_W;
  *x = dir < 0 ? WR_X0 : WR_X1 - WR_ARROW_W;
}

// The composed trainer at an integer zoom, centred in a box -- shared with
// drawAvatar(), so the card, the picker and this screen draw the same thing.
void drawWardrobeTrainer(int bx, int by, int box) {
  // The art lives in wardrobe.cpp alone (pokemmo_art.h is static data, and a
  // second include would put a second copy in flash): ask it for the size.
  const int W = wardrobeCanvasW(), H = wardrobeCanvasH();
  static uint16_t *px = nullptr;
  static uint8_t *mask = nullptr;
  if (!px) { px = (uint16_t *)malloc(W * H * 2); mask = (uint8_t *)malloc(W * H); }
  if (!px || !mask) return;
  wardrobeCompose(px, mask);
  int side = W > H ? W : H;
  int k = box / side; if (k < 1) k = 1;
  int ox = bx + (box - W * k) / 2, oy = by + (box - H * k) / 2;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      if (mask[y * W + x]) gfx->fillRect(ox + x * k, oy + y * k, k, k, px[y * W + x]);
}

void renderWardrobe() {
  gfx->fillScreen(RGB565_BLACK);
  drawCardBackground(casino.bg, millis());
  drawWardrobeTrainer(CX - 60, 36, 120);   // the look, large
  if (wardrobePage >= WR_PAGES) wardrobePage = 0;
  for (uint8_t i = 0; i < WR_PER_PAGE; i++) {
    uint8_t r = wardrobePage * WR_PER_PAGE + i;
    if (r >= WR_COUNT) break;
    int y = WR_ROW_Y(i);
    bool live = r != WR_HAIRCOL || wardrobeHairDyeable();
    gfx->fillRoundRect(WR_X0, y, WR_X1 - WR_X0, WR_ROW_H, 12, live ? UI_WHITE : UI_TRACK);
    gfx->drawRoundRect(WR_X0, y, WR_X1 - WR_X0, WR_ROW_H, 12, UI_INK);
    for (int d = -1; d <= 1; d += 2) {
      int ax, ay, aw, ah; wardrobeArrowRect(i, d, &ax, &ay, &aw, &ah);
      gfx->fillRoundRect(ax, ay, aw, ah, 12, live ? 0xFF98 : UI_TRACK);
      gfx->drawRoundRect(ax, ay, aw, ah, 12, UI_INK);
      int cx = ax + aw / 2, cy = ay + ah / 2;
      if (d < 0) gfx->fillTriangle(cx - 8, cy, cx + 6, cy - 9, cx + 6, cy + 9, UI_INK);
      else gfx->fillTriangle(cx + 8, cy, cx - 6, cy - 9, cx - 6, cy + 9, UI_INK);
    }
    const char *lbl = T(wrLabel(r));
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(lbl) * 3, y + 7);
    gfx->print(lbl);
    // PokeMMO's names run long: size 2 when it fits between the arrows, else 1
    const char *v = wrValue(r);
    int room = WR_X1 - WR_X0 - 2 * WR_ARROW_W - 8;
    int ts = (int)strlen(v) * 12 <= room ? 2 : 1;
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(ts);
    gfx->setCursor(CX - (int)strlen(v) * 3 * ts, y + (ts == 2 ? 22 : 26));
    gfx->print(v);
  }
  char l[32];
  snprintf(l, sizeof(l), T(S_WD_OWNED_FMT), (unsigned)cosOwnedCount(), (unsigned)cosUnlockableCount());
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(l) * 3, 390);
  gfx->print(l);
  uiChrome();
  gfx->flush();
}

// True if the tap was the wardrobe's (it always is while it is open).
bool wardrobeTap(int16_t px, int16_t py) {
  for (uint8_t i = 0; i < WR_PER_PAGE; i++) {
    uint8_t r = wardrobePage * WR_PER_PAGE + i;
    if (r >= WR_COUNT) break;
    for (int d = -1; d <= 1; d += 2) {
      int x, y, w, h; wardrobeArrowRect(i, d, &x, &y, &w, &h);
      if (px >= x && px <= x + w && py >= y && py <= y + h) { wrStep(r, d); sfxPlay(SFX_TAP); return true; }
    }
  }
  return true;   // a miss does nothing: it never closes the wardrobe
}
