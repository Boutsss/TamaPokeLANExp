// The WARDROBE: dressing the customisable trainer (AVATAR_YOU, wardrobe.cpp).
// Opened from the trainer card's WARDROBE button, which only shows while YOU is
// the avatar worn. A level below the card (SCR_WARDROBE): pages on the rim,
// held BACK returns to the card.
//
// The trainer stands large at the top; under it, one row per thing you can
// change, each with < and > that step through what you OWN (wardrobeStep()).
// Skin, eyes, hair and hair colour are all free; clothes are cosmetics, a few
// owned from the start and the rest won with an S in the arcade.
bool wardrobeOpen = false;
uint8_t wardrobePage = 0;

enum : uint8_t { WR_SKIN = 0, WR_EYES, WR_HAIR, WR_HAIRCOL, WR_HAT, WR_TOP, WR_LEGS, WR_SHOES, WR_FACE, WR_BACK, WR_COUNT };
#define WR_PER_PAGE 4
#define WR_PAGES ((WR_COUNT + WR_PER_PAGE - 1) / WR_PER_PAGE)
#define WR_ROW_Y(i) (150 + (i) * 58)
#define WR_ROW_H 50
#define WR_X0 64
#define WR_X1 402
#define WR_ARROW_W 48

uint8_t wardrobePages() { return WR_PAGES; }

static StrId wrLabel(uint8_t r) {
  static const StrId L[WR_COUNT] = { S_WD_SKIN, S_WD_EYES, S_WD_HAIR, S_WD_HAIRCOL, S_WD_HAT,
                                     S_WD_TOP, S_WD_LEGS, S_WD_SHOES, S_WD_FACE, S_WD_BACK };
  return L[r];
}
static const char *wrValue(uint8_t r) {
  switch (r) {
    case WR_SKIN: return wardrobeSkinName(wardrobe.skin);
    case WR_EYES: return wardrobeEyeName(wardrobe.eyes);
    case WR_HAIR: return wardrobeHairName(wardrobe.hair);
    case WR_HAIRCOL: return wardrobeHairColName(wardrobe.hairCol);
    default: return cosName(wardrobe.worn[r - WR_HAT]);
  }
}
static void wrStep(uint8_t r, int dir) {
  auto cyc = [dir](uint8_t v, uint8_t n) { return (uint8_t)((v + (dir > 0 ? 1 : n - 1)) % n); };
  switch (r) {
    case WR_SKIN: wardrobe.skin = cyc(wardrobe.skin, WARDROBE_SKINS); break;
    case WR_EYES: wardrobe.eyes = cyc(wardrobe.eyes, WARDROBE_EYES); break;
    case WR_HAIR: wardrobe.hair = cyc(wardrobe.hair, WARDROBE_HAIRS); break;
    case WR_HAIRCOL: wardrobe.hairCol = cyc(wardrobe.hairCol, WARDROBE_HAIR_COLS); break;
    default: {
      uint8_t s = r - WR_HAT;
      wardrobe.worn[s] = wardrobeStep(s, wardrobe.worn[s], dir);
    }
  }
  wardrobeSave();
}

// Geometry, asked rather than restated: the renderer, the taps and the tests.
void wardrobeArrowRect(uint8_t row, int dir, int *x, int *y, int *w, int *h) {
  *y = WR_ROW_Y(row); *h = WR_ROW_H; *w = WR_ARROW_W;
  *x = dir < 0 ? WR_X0 : WR_X1 - WR_ARROW_W;
}

void renderWardrobe() {
  gfx->fillScreen(RGB565_BLACK);
  drawCardBackground(casino.bg, millis());
  drawAvatar(AVATAR_YOU, CX - AVATAR_PX * 3, 44, 6);     // 96 px: the look, large
  if (wardrobePage >= WR_PAGES) wardrobePage = 0;
  for (uint8_t i = 0; i < WR_PER_PAGE; i++) {
    uint8_t r = wardrobePage * WR_PER_PAGE + i;
    if (r >= WR_COUNT) break;
    int y = WR_ROW_Y(i);
    gfx->fillRoundRect(WR_X0, y, WR_X1 - WR_X0, WR_ROW_H, 12, UI_WHITE);
    gfx->drawRoundRect(WR_X0, y, WR_X1 - WR_X0, WR_ROW_H, 12, UI_INK);
    for (int d = -1; d <= 1; d += 2) {
      int ax, ay, aw, ah; wardrobeArrowRect(i, d, &ax, &ay, &aw, &ah);
      gfx->fillRoundRect(ax, ay, aw, ah, 12, 0xFF98);
      gfx->drawRoundRect(ax, ay, aw, ah, 12, UI_INK);
      int cx = ax + aw / 2, cy = ay + ah / 2;
      if (d < 0) gfx->fillTriangle(cx - 8, cy, cx + 6, cy - 9, cx + 6, cy + 9, UI_INK);
      else gfx->fillTriangle(cx + 8, cy, cx - 6, cy - 9, cx - 6, cy + 9, UI_INK);
    }
    const char *lbl = T(wrLabel(r));
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(lbl) * 3, y + 8);
    gfx->print(lbl);
    const char *v = wrValue(r);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(v) * 6, y + 24);
    gfx->print(v);
  }
  char l[32];
  snprintf(l, sizeof(l), T(S_WD_OWNED_FMT), (unsigned)cosOwnedCount(), (unsigned)cosUnlockableCount());
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(l) * 3, 382);
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
