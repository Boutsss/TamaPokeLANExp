// Finding creatures in the box (box_filter.cpp holds the rules) and marking
// FAVOURITES. The box's hub opens the FILTER sheet: one chip per filter --
// everything, favourites, shinies, holding an item, and each of the 18 types --
// and a SORT pill that cycles slot order, level, IV total, the four battle
// stats, dex number and name. Geometry is asked, never restated: bfChip(),
// bfSortBtn() and sheetFavBtn() answer for the renderer, the taps and the tests.


uint8_t boxFilter = BF_ALL;
uint8_t boxSort = BS_SLOT;
bool boxFilterOpen = false;

// The view the box screen shows right now. A creature waiting to come in from
// the party needs the EMPTY slots, so an armed swap always shows the plain box.
uint8_t boxViewNow(uint8_t *out) {
  if (boxSwapFrom) return boxView(party, BF_ALL, BS_SLOT, out);
  return boxView(party, boxFilter, boxSort, out);
}
uint8_t boxPages() {
  uint8_t v[BOX_SLOTS];
  uint8_t n = boxViewNow(v);
  return n ? (uint8_t)((n + BOX_PER_PAGE - 1) / BOX_PER_PAGE) : 1;
}
// The box index shown at ring position `pos` of the current page, -1 if none.
int boxIndexAt(uint8_t pos) {
  uint8_t v[BOX_SLOTS];
  uint8_t n = boxViewNow(v);
  uint16_t k = (uint16_t)boxPage * BOX_PER_PAGE + pos;
  return k < n ? v[k] : -1;
}
bool boxFiltered() { return !boxSwapFrom && !boxViewPlain(boxFilter, boxSort); }

const char *boxFilterName(uint8_t f) {
  switch (f) {
    case BF_ALL: return T(S_BF_ALL);
    case BF_FAV: return T(S_BF_FAV);
    case BF_SHINY: return T(S_BF_SHINY);
    case BF_HELD: return T(S_BF_HELD);
    default: return typeName((uint8_t)(f - BF_TYPE0));
  }
}
const char *boxSortName(uint8_t s) {
  switch (s) {
    case BS_LEVEL: return T(S_BF_S_LEVEL);
    case BS_IV: return T(S_BF_S_IV);
    case BS_ATK: return T(S_BF_S_ATK);
    case BS_DEF: return T(S_BF_S_DEF);
    case BS_SPE: return T(S_BF_S_SPE);
    case BS_HP: return T(S_BF_S_HP);
    case BS_DEX: return T(S_BF_S_DEX);
    case BS_NAME: return T(S_BF_S_NAME);
    default: return T(S_BF_S_SLOT);
  }
}
const char *boxFilterWord() { return T(S_BF_FILTER); }
const char *boxFavLockLine() { return T(S_BF_FAVLOCK); }
void boxFoundLine(char *out, size_t n, uint8_t found) { snprintf(out, n, T(S_BF_FOUND_FMT), (unsigned)found); }
void boxSortLine(char *out, size_t n) { snprintf(out, n, T(S_BF_SORT_FMT), boxSortName(boxSort)); }

// A small five-pointed star, the favourite's mark.
void drawFavStar(int cx, int cy, int r, uint16_t col) {
  for (int k = 0; k < 5; k++) {
    float a = -1.5708f + k * 1.2566f;
    int tx = cx + (int)(r * cosf(a)), ty = cy + (int)(r * sinf(a));
    int lx = cx + (int)(r * 0.42f * cosf(a - 0.628f)), ly = cy + (int)(r * 0.42f * sinf(a - 0.628f));
    int rx = cx + (int)(r * 0.42f * cosf(a + 0.628f)), ry = cy + (int)(r * 0.42f * sinf(a + 0.628f));
    gfx->fillTriangle(cx, cy, tx, ty, lx, ly, col);
    gfx->fillTriangle(cx, cy, tx, ty, rx, ry, col);
  }
}

// ---- the FILTER sheet: 4 columns of chips, then the SORT pill
#define BF_CHIP_W 84
#define BF_CHIP_H 44   // UI_TAP_MIN: the floor, not the target, but 22 chips have to fit
void bfChip(uint8_t i, int *x, int *y, int *w, int *h) {
  *w = BF_CHIP_W; *h = BF_CHIP_H;
  *x = CX - 2 * (BF_CHIP_W + 4) + 2 + (i % 4) * (BF_CHIP_W + 4);
  *y = 78 + (i / 4) * (BF_CHIP_H + 4);
}
void bfSortBtn(int *x, int *y, int *w, int *h) { *w = 240; *h = 46; *x = CX - 120; *y = 370; }

void renderBoxFilter() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  const char *t = T(S_BF_FILTER);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(t) * 6, 48);
  gfx->print(t);
  for (uint8_t i = 0; i < BF_COUNT; i++) {
    int x, y, w, h; bfChip(i, &x, &y, &w, &h);
    bool on = boxFilter == i;
    uint16_t fill = UI_WHITE, ink = UI_INK;
    if (i >= BF_TYPE0) {
      uint8_t ty = (uint8_t)(i - BF_TYPE0);
      fill = typeColor(ty);
      ink = typeColorIsLight(ty) ? UI_INK : UI_WHITE;
    } else if (i == BF_FAV) { fill = 0xFEA0; }
    gfx->fillRoundRect(x, y, w, h, 10, fill);
    gfx->drawRoundRect(x, y, w, h, 10, UI_INK);
    if (on) { gfx->drawRoundRect(x - 2, y - 2, w + 4, h + 4, 12, UI_INK); gfx->drawRoundRect(x - 3, y - 3, w + 6, h + 6, 13, UI_INK); }
    const char *nm = boxFilterName(i);
    gfx->setTextColor(ink);
    gfx->setTextSize(1);
    gfx->setCursor(x + w / 2 - (int)strlen(nm) * 3, y + h / 2 - 4);
    gfx->print(nm);
    if (i == BF_FAV) drawFavStar(x + 10, y + 10, 7, UI_INK);
  }
  int x, y, w, h; bfSortBtn(&x, &y, &w, &h);
  char b[40]; boxSortLine(b, sizeof(b));
  gfx->fillRoundRect(x, y, w, h, 14, C565(0xff, 0xf1, 0xc4));
  gfx->drawRoundRect(x, y, w, h, 14, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(b) * 6, y + h / 2 - 8);
  gfx->print(b);
  gfx->flush();
}

// A chip chooses and closes (the box shows the result at once); the SORT pill
// cycles and stays. Anywhere else closes.
void boxFilterTap(int16_t px, int16_t py) {
  for (uint8_t i = 0; i < BF_COUNT; i++) {
    int x, y, w, h; bfChip(i, &x, &y, &w, &h);
    if (px < x || px >= x + w || py < y || py >= y + h) continue;
    boxFilter = i;
    boxPage = 0;
    boxFilterOpen = false;
    sfxPlay(SFX_TAP);
    return;
  }
  int x, y, w, h; bfSortBtn(&x, &y, &w, &h);
  if (px >= x && px < x + w && py >= y && py < y + h) {
    boxSort = (uint8_t)((boxSort + 1) % BS_COUNT);
    boxPage = 0;
    sfxPlay(SFX_TAP);
    return;
  }
  boxFilterOpen = false;
}
